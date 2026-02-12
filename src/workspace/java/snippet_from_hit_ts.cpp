
#include "workspace/java/snippet_from_hit.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

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

    // zero init
    out.assign(static_cast<size_t>(n), '\0');
    if (n == 0) {
        return true;
    }

    in.read(out.data(), n);
    return in.good() || in.eof();
}


static bool node_type_is(TSNode n, const char *type)
{
    const char *t = ts_node_type(n);
    return t && std::strcmp(t, type) == 0;
}


static const char *preferred_types[] =
{
    "method_declaration",
    "constructor_declaration",
    "class_declaration",
    "interface_declaration",
    "enum_declaration",
    "record_declaration"
};


static bool is_preferred_type(TSNode n)
{
    const char *t = ts_node_type(n);
    if (!t) {
        return false;
    }

    for (const char *want : preferred_types) {
        if (std::strcmp(t, want) == 0) {
            return true;
        }
    }

    return false;
}


static TSNode climb_to_preferred(TSNode n)
{
    TSNode cur = n;
    while (!ts_node_is_null(cur)) {
        if (is_preferred_type(cur)) {
            return cur;
        }
        TSNode parent = ts_node_parent(cur);
        if (ts_node_is_null(parent)) {
            break;
        }
        cur = parent;
    }
    return TSNode{};
}


HitSnippet snippet_from_hit(const std::string &abs_path,
                            const std::string &rel_path,
                            uint64_t hit_byte_offset)
{
    HitSnippet out;
    out.abs_path = abs_path;
    out.rel_path = rel_path;

    std::string src;
    if (!read_entire_file(abs_path, src)) {
        out.found = false;
        out.reason = "failed to read file";
        return out;
    }

    if (hit_byte_offset >= src.size()) {
        out.found = false;
        out.reason = "hit_byte_offset out of range";
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

    // find the smallest node that spans the byte offset.
    uint32_t b = static_cast<uint32_t>(hit_byte_offset);
    TSNode leaf = ts_node_descendant_for_byte_range(root, b, b);

    if (ts_node_is_null(leaf)) {
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        out.found = false;
        out.reason = "descendant_for_byte_range returned null";
        return out;
    }

    // expand to meaningful scope
    TSNode best = climb_to_preferred(leaf);
    if (ts_node_is_null(best)) {
        // fallback to root for best effort
        best = root;
    }

    uint32_t a = ts_node_start_byte(best);
    uint32_t e = ts_node_end_byte(best);

    if (a > e || e > src.size()) {
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        out.found = false;
        out.reason = "invalid node byte range";
        return out;
    }

    out.found = true;
    out.kind = ts_node_type(best);
    out.start = static_cast<size_t>(a);
    out.end = static_cast<size_t>(e);
    out.text = src.substr(out.start, out.end - out.start);
    out.reason = "tree-sitter enclosing node";

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return out;
}

