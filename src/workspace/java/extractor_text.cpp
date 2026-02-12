
#include "workspace/java/extractor.h"

#include <cctype>
#include <fstream>
#include <string>


static bool is_ident_char(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
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

    out.assign(static_cast<size_t>(n), '\0');
    if (n == 0) {
        return true;
    }

    in.seekg(0, std::ios::beg);
    in.read(&out[0], n);
    return in.good() || in.eof();
}


static bool match_word_before_paren(const std::string &s, size_t pos, const std::string &name)
{
    // Check s[pos..pos+name.size()) == name AND next non-space is '('
    if (pos + name.size() > s.size()) {
        return false;
    }

    if (s.compare(pos, name.size(), name) != 0) {
        return false;
    }

    // Word boundary before
    if (pos > 0 && is_ident_char(s[pos - 1])) {
        return false;
    }

    // Word boundary after name
    size_t after = pos + name.size();
    if (after < s.size() && is_ident_char(s[after])) {
        return false;
    }

    // Skip whitespace to find '('
    size_t i = after;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        i++;
    }

    return (i < s.size() && s[i] == '(');
}


static bool find_method_header(const std::string &src, 
                               const std::string &method_name, 
                               size_t *out_name_pos)
{
    // v1: find the first occurrence of "methodName (" with word boundaries.
    // This may match calls inside other methods; later we will refine.
    for (size_t i = 0; i < src.size(); i++) {
        if (src[i] != method_name[0]) {
            continue;
        }
        if (match_word_before_paren(src, i, method_name)) {
            *out_name_pos = i;
            return true;
        }
    }
    return false;
}


static bool find_open_brace_after_paren_list(const std::string &src, size_t name_pos, size_t *out_brace_pos)
{
    // Starting from name_pos, find the parameter list (...) then the opening '{' of the body.
    // We need to skip nested parentheses in generics/annotations? v1 keeps it simple:
    // - find the first '(' after name
    // - match until its corresponding ')'
    // - then skip whitespace/throws/annotations and find '{'

    size_t i = name_pos;

    while (i < src.size() && src[i] != '(') {
        i++;
    }
    if (i >= src.size()) {
        return false;
    }

    int paren = 0;
    for (; i < src.size(); i++) {
        if (src[i] == '(') {
            paren++;
        } else if (src[i] == ')') {
            paren--;
            if (paren == 0) {
                i++; // move past ')'
                break;
            }
        }
    }
    if (paren != 0) {
        return false;
    }

    // Now find the next '{' that begins the method body.
    // v1: skip whitespace and allow "throws ..." until '{'
    for (; i < src.size(); i++) {
        char c = src[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }
        if (c == '{') {
            *out_brace_pos = i;
            return true;
        }
        // If we hit ';' before '{', it might be an abstract method/interface method.
        if (c == ';') {
            return false;
        }
        // Otherwise keep scanning (handles "throws X, Y" etc.)
    }

    return false;
}

static bool brace_match_block(const std::string &src, size_t open_brace_pos, size_t *out_end_pos_exclusive)
{
    // v1 brace matcher (does not ignore braces in strings/comments yet).
    int depth = 0;

    for (size_t i = open_brace_pos; i < src.size(); i++) {
        if (src[i] == '{') {
            depth++;
        } else if (src[i] == '}') {
            depth--;
            if (depth == 0) {
                *out_end_pos_exclusive = i + 1; // include closing brace
                return true;
            }
        }
    }

    return false;
}


MethodSnippet extract_method_from_file(const std::string &abs_path,
                                       const std::string &rel_path,
                                       const std::string &method_name)
{
    MethodSnippet out;
    out.abs_path = abs_path;
    out.rel_path = rel_path;

    std::string src;
    if (!read_entire_file(abs_path, src)) {
        out.found = false;
        out.reason = "failed to read file";
        return out;
    }

    size_t name_pos = 0;
    if (!find_method_header(src, method_name, &name_pos)) {
        out.found = false;
        out.reason = "method name not found";
        return out;
    }

    size_t open_brace = 0;
    if (!find_open_brace_after_paren_list(src, name_pos, &open_brace)) {
        out.found = false;
        out.reason = "found name, but could not locate method body '{' (maybe abstract/interface?)";
        return out;
    }

    size_t end_excl = 0;
    if (!brace_match_block(src, open_brace, &end_excl)) {
        out.found = false;
        out.reason = "failed to brace-match method body";
        return out;
    }

    out.found = true;
    out.start = name_pos;
    out.end = end_excl;
    out.text = src.substr(name_pos, end_excl - name_pos);
    out.reason = "extracted by name+paren+brace match (v1)";
    return out;
}

