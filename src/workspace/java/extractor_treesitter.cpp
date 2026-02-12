
#include "workspace/java/extractor.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <tree_sitter/api.h>


extern "C"
{
// from external/tree-sitter-java/src/parser.c
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


static bool method_has_body(TSNode method_decl)
{
    // In tree-sitter-java, method_declaration normally has a "body" field when implemented.
    // Abstract/interface methods usually end with ';' and have no body.
    TSNode body = ts_node_child_by_field_name(method_decl, "body", 4);
    if (ts_node_is_null(body)) {
        return false;
    }

    // Body should be a "block" for implemented methods.
    // (If grammar changes, this is still a safe check.)
    if (node_type_is(body, "block")) {
        return true;
    }

    return true;
}


static bool find_first_method_decl(TSNode root,
                                  const std::string &src,
                                  const std::string &method_name,
                                  TSNode *out_method)
{
    TSTreeCursor cur = ts_tree_cursor_new(root);
    bool found = false;
    for (;;) {
        TSNode n = ts_tree_cursor_current_node(&cur);
        if (node_type_is(n, "method_declaration")) {
            TSNode name = ts_node_child_by_field_name(n, "name", 4);
            if (!ts_node_is_null(name)) {
                std::string_view name_sv = node_text_view(src, name);
                if (name_sv == method_name) {
                    if (method_has_body(n)) {
                        *out_method = n;
                        found = true;
                        break;
                    }
                }
            }
        }

        // dfs
        if (ts_tree_cursor_goto_first_child(&cur)) continue;
        if (ts_tree_cursor_goto_next_sibling(&cur)) continue;
        // backtrack
        bool backtracked = false;
        while (ts_tree_cursor_goto_parent(&cur)) {
            if (ts_tree_cursor_goto_next_sibling(&cur)) {
                backtracked = true;
                break;
            }
        }
        if (!backtracked) {
            break;
        }
    }
    ts_tree_cursor_delete(&cur);
    return found;
}


Method extract_method_from_file(const std::string &abs_path,
                                const std::string &rel_path,
                                const std::string &method_name)
{
    Method out;
    out.abs_path = abs_path;
    out.rel_path = rel_path;

    std::string src;
    if (!read_entire_file(abs_path, src)) {
        out.found = false;
        out.reason = "failed to read file";
        return out;
    }

    TSParser *parser = ts_parser_new();
    if (!parser) {
        out.found = false;
        out.reason = "ts_parser_new failed";
        return out;
    }

    if (!ts_parser_set_language(parser, tree_sitter_java())) {
        ts_parser_delete(parser);
        out.found = false;
        out.reason = "ts_parser_set_language(java) failed";
        return out;
    }

    TSTree *tree = ts_parser_parse_string(parser, nullptr, src.data(), static_cast<uint32_t>(src.size()));
    if (!tree) {
        ts_parser_delete(parser);
        out.found = false;
        out.reason = "ts_parser_parse_string failed";
        return out;
    }

    TSNode root = ts_tree_root_node(tree);

    TSNode method = TSNode{};
    bool ok = find_first_method_decl(root, src, method_name, &method);

    if (!ok) {
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        out.found = false;
        out.reason = "method_declaration not found (or no body)";
        return out;
    }

    uint32_t a = ts_node_start_byte(method);
    uint32_t b = ts_node_end_byte(method);

    if (a > b || b > src.size()) {
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        out.found = false;
        out.reason = "invalid node byte range";
        return out;
    }

    out.found = true;
    out.start = static_cast<size_t>(a);
    out.end = static_cast<size_t>(b);
    out.text = src.substr(out.start, out.end - out.start);
    out.reason = "tree-sitter method_declaration match";

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return out;
}

