
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>


struct HitSnippet
{
    bool found = false;

    std::string abs_path;
    std::string rel_path;

    std::string kind;   // e.g. "method_declaration", "class_declaration"
    size_t start = 0;
    size_t end = 0;

    std::string reason;
    std::string text;
};

// given a file and a byte offset (from rg), return the enclosing snippet.
// Prefers method/constructor nodes; falls back to class, interface, etc.
HitSnippet snippet_from_hit(const std::string &abs_path,
                            const std::string &rel_path,
                            uint64_t hit_byte_offset);

