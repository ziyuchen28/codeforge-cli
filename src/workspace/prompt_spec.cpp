
#include "workspace/prompt_spec.h"

#include <cctype>
#include <fstream>
#include <string>
#include <vector>


static std::string read_entire_file_text(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }

    in.seekg(0, std::ios::end);
    std::streamoff n = in.tellg();
    if (n < 0) {
        return {};
    }
    in.seekg(0, std::ios::beg);

    std::string out;
    out.assign(static_cast<size_t>(n), '\0');
    if (n == 0) {
        return out;
    }

    in.read(out.data(), n);
    if (!in.good() && !in.eof()) {
        return {};
    }

    return out;
}

static std::string trim_copy(const std::string &s)
{
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) {
        a++;
    }

    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
        b--;
    }

    return s.substr(a, b - a);
}

static std::string to_lower_copy(const std::string &s)
{
    std::string out = s;
    for (char &c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

static bool find_section(const std::string &src,
                         const std::string &open_tag,
                         const std::string &close_tag,
                         size_t *out_a,
                         size_t *out_b)
{
    size_t a = src.find(open_tag);
    if (a == std::string::npos) {
        return false;
    }
    a += open_tag.size();

    size_t b = src.find(close_tag, a);
    if (b == std::string::npos) {
        return false;
    }

    *out_a = a;
    *out_b = b;
    return true;
}

static std::vector<std::string> split_lines(const std::string &s)
{
    std::vector<std::string> out;

    size_t i = 0;
    while (i < s.size()) {
        size_t nl = s.find('\n', i);
        if (nl == std::string::npos) {
            out.push_back(s.substr(i));
            break;
        }
        out.push_back(s.substr(i, nl - i));
        i = nl + 1;
    }

    return out;
}

static Scope parse_scope(const std::string &v)
{
    std::string x = to_lower_copy(trim_copy(v));
    if (x == "local") {
        return Scope::Local;
    }
    if (x == "deps") {
        return Scope::Deps;
    }
    if (x == "deep") {
        return Scope::Deep;
    }
    return Scope::Auto;
}

PromptSpec parse_prompt_file(const std::string &path)
{
    PromptSpec spec;

    std::string src = read_entire_file_text(path);
    if (src.empty()) {
        spec.ok = false;
        spec.error = "failed to read prompt file";
        return spec;
    }

    size_t ha = 0;
    size_t hb = 0;
    if (!find_section(src, "[HINTS]", "[/HINTS]", &ha, &hb)) {
        spec.ok = false;
        spec.error = "missing [HINTS]...[/HINTS] section";
        return spec;
    }

    std::string hints_body = src.substr(ha, hb - ha);
    std::vector<std::string> lines = split_lines(hints_body);

    for (const std::string &raw : lines) {
        std::string line = trim_copy(raw);
        if (line.empty()) {
            continue;
        }
        if (line[0] == '#') {
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key = to_lower_copy(trim_copy(line.substr(0, eq)));
        std::string val = trim_copy(line.substr(eq + 1));

        if (key == "repo_root") {
            spec.repo_root = val;
        } else if (key == "anchor_class" || key == "class") {
            spec.anchor_class_fqcn = val;
        } else if (key == "anchor_method" || key == "method") {
            spec.anchor_method = val;
        } else if (key == "scope") {
            spec.scope = parse_scope(val);
        }
    }

    // TASK section is optional now (we'll use it later for ask)
    size_t ta = 0;
    size_t tb = 0;
    if (find_section(src, "[TASK]", "[/TASK]", &ta, &tb)) {
        spec.task_text = trim_copy(src.substr(ta, tb - ta));
    }

    // For now, we require anchor_class and anchor_method to be present.
    if (spec.anchor_class_fqcn.empty()) {
        spec.ok = false;
        spec.error = "missing anchor_class in [HINTS]";
        return spec;
    }
    if (spec.anchor_method.empty()) {
        spec.ok = false;
        spec.error = "missing anchor_method in [HINTS]";
        return spec;
    }

    spec.ok = true;
    return spec;
}

