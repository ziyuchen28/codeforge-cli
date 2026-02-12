
#include "workspace/java/dep_harvest.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <tree_sitter/api.h>

extern "C"
{
const TSLanguage *tree_sitter_java(void);
}


static bool read_entire_file(const std::string &path, std::string &out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }

    in.seekg(0, std::ios::end);
    std::streamoff n = in.tellg();
    if (n < 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);

    out.assign(static_cast<size_t>(n), '\0');
    if (n == 0) {
        return true;
    }

    in.read(out.data(), n);
    return in.good() || in.eof();
}


static std::string_view node_text_view(const std::string &src, TSNode n)
{
    uint32_t a = ts_node_start_byte(n);
    uint32_t b = ts_node_end_byte(n);
    if (a > b || b > src.size()) {
        return {};
    }
    return std::string_view(src.data() + a, b - a);
}


static bool node_type_is(TSNode n, const char *type)
{
    const char *t = ts_node_type(n);
    return t && std::strcmp(t, type) == 0;
}


static TSNode find_invocation_name_node(TSNode node)
{
    // example
    // in f() : a(x)
    // (method_invocation
    //   name: (identifier)        // "a"
    //   arguments: (argument_list (identifier)))  // "x"
    TSNode name = ts_node_child_by_field_name(node, "name", 4);
    if (!ts_node_is_null(name)) {
        return name;
    }
    // example
    // in f(): println
    // (method_invocation
    //   object: (field_access
    //             object: (identifier)   ; "System"
    //             field:  (identifier))  ; "out"
    //   member: (identifier)             ; "println"
    //   arguments: (argument_list
    //               (string_literal)))
    TSNode member = ts_node_child_by_field_name(node, "member", 6);
    if (!ts_node_is_null(member)) {
        return member;
    }
    // better than nothing fallback
    uint32_t nchild = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nchild; i++) {
        TSNode c = ts_node_named_child(node, i);
        const char *t = ts_node_type(c);
        if (!t) {
            continue;
        }
        if (std::strcmp(t, "identifier") == 0) {
            return c;
        }
    }
    return TSNode{};
}


static bool is_noise_method(const std::string &name)
{
    static const std::unordered_set<std::string> stop = {
        "toString", "hashCode", "equals",
        "getClass", "notify", "notifyAll", "wait",
        "size", "isEmpty", "get", "set", "add", "remove", "contains",
        "stream", "map", "flatMap", "filter", "collect", "forEach",
        "of", "valueOf"
    };

    if (name.size() < 2) {
        return true;
    }
    return stop.find(name) != stop.end();
}


std::vector<std::string> harvest_callees_in_range(const std::string &abs_path,
                                                  size_t node_start,
                                                  size_t node_end)
{
    std::vector<std::string> out;

    std::string src;
    if (!read_entire_file(abs_path, src)) {
        return out;
    }

    if (node_start >= src.size() || node_end > src.size() || node_start >= node_end) {
        return out;
    }

    TSParser *parser = ts_parser_new();
    if (!parser) {
        return out;
    }

    if (!ts_parser_set_language(parser, tree_sitter_java())) {
        ts_parser_delete(parser);
        return out;
    }

    TSTree *tree = ts_parser_parse_string(parser, nullptr, src.data(), static_cast<uint32_t>(src.size()));
    if (!tree) {
        ts_parser_delete(parser);
        return out;
    }

    TSNode root = ts_tree_root_node(tree);

    // Locate the smallest node spanning the range start; climb to method/constructor.
    uint32_t b = static_cast<uint32_t>(node_start);
    TSNode leaf = ts_node_descendant_for_byte_range(root, b, b);
    if (ts_node_is_null(leaf)) {
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        return out;
    }

    TSNode cur = leaf;
    while (!ts_node_is_null(cur)) {
        if (node_type_is(cur, "method_declaration") || node_type_is(cur, "constructor_declaration")) {
            break;
        }
        TSNode p = ts_node_parent(cur);
        if (ts_node_is_null(p)) {
            break;
        }
        cur = p;
    }

    if (ts_node_is_null(cur) || !(node_type_is(cur, "method_declaration") || node_type_is(cur, "constructor_declaration"))) {
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        return out;
    }

    std::unordered_set<std::string> seen;
    seen.reserve(128);

    TSTreeCursor cursor = ts_tree_cursor_new(cur);
    for (;;) {
        TSNode n = ts_tree_cursor_current_node(&cursor);
        if (node_type_is(n, "method_invocation")) {
            TSNode name_node = find_invocation_name_node(n);
            if (!ts_node_is_null(name_node)) {
                std::string_view sv = node_text_view(src, name_node);
                if (!sv.empty()) {
                    std::string name(sv);
                    if (!is_noise_method(name)) {
                        if (seen.insert(name).second) {
                            out.push_back(std::move(name));
                        }
                    }
                }
            }
        }

        if (ts_tree_cursor_goto_first_child(&cursor)) {
            continue;
        }

        if (ts_tree_cursor_goto_next_sibling(&cursor)) {
            continue;
        }

        bool climbed = false;
        while (ts_tree_cursor_goto_parent(&cursor)) {
            if (ts_tree_cursor_goto_next_sibling(&cursor)) {
                climbed = true;
                break;
            }
        }

        if (!climbed) {
            break;
        }
    }

    ts_tree_cursor_delete(&cursor);
    ts_tree_delete(tree);
    ts_parser_delete(parser);

    // Keep output stable.
    std::sort(out.begin(), out.end());
    return out;
}

