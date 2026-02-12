#include "workspace/java/locator.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <memory>
#include <string>
#include <vector>


static bool ends_with(const std::string &s, const std::string &suffix)
{
    return s.size() >= suffix.size() &&
           std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}


static std::string add_suffix(const std::string &fqcn)
{
    // a.b.c -> a.b.c.java
    std::string out;
    out.reserve(fqcn.size() + 5);
    for (char c : fqcn) {
        out.push_back(c == '.' ? '/' : c);
    }
    out += ".java";
    return out;
}


static std::string class_simple_name(const std::string &fqcn)
{

    // a.b.c -> c
    size_t pos = fqcn.find_last_of('.');
    return (pos == std::string::npos) ? fqcn : fqcn.substr(pos + 1);
}


static std::string class_package_name(const std::string &fqcn)
{

    // a.b.c -> a.b
    size_t pos = fqcn.find_last_of('.');
    return (pos == std::string::npos) ? "" : fqcn.substr(0, pos);
}


static std::string ltrim_copy(const std::string &s)
{
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        i++;
    }
    return s.substr(i);
}


static bool file_contains_package_line(const std::string &abs_path, const std::string &pkg)
{
    if (pkg.empty()) return false;
    std::ifstream in(abs_path);
    if (!in) { return false; }

    const std::string pkg_symbol = "package " + pkg + ";";
    std::string line;
    int nlines = 0;
    while (std::getline(in, line)) {
        nlines++;
        if (nlines > 256) { break; }
        std::string t = ltrim_copy(line);
        if (t.rfind(pkg_symbol, 0) == 0) { 
            return true; 
        }
        if (t.find("class ") != std::string::npos ||
            t.find("interface ") != std::string::npos ||
            t.find("enum ") != std::string::npos ||
            t.find("record ") != std::string::npos) {
            break;
        }
    }
    return false;
}


static bool file_contains_type_decl(const std::string &abs_path, const std::string &simple)
{
    std::ifstream in(abs_path);
    if (!in) {
        return false;
    }
    const std::string patterns[] = {
        "class " + simple,
        "interface " + simple,
        "enum " + simple,
        "record " + simple
    };
    std::string line;
    int nlines = 0;
    while (std::getline(in, line)) {
        nlines++;
        if (nlines > 2048) {
            break;
        }
        for (const std::string &p : patterns) {
            if (line.find(p) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}


static int score_path(const std::string &rel_path)
{
    int score = 0;

    if (rel_path.find("/src/main/java/") != std::string::npos) {
        score += 50;
    }
    if (rel_path.find("/src/test/java/") != std::string::npos) {
        score += 20;
    }

    if (rel_path.find("/target/") != std::string::npos) {
        score -= 80;
    }
    if (rel_path.find("/build/") != std::string::npos) {
        score -= 80;
    }

    return score;
}


class TextJavaLocator final : public JavaLocator
{

public:
    TextJavaLocator(const std::vector<FileEntry> &files)
        : files_(files) {}

    ClassLocation locate_class(const std::string &fqcn) override
    {
        ClassLocation out;
        const std::string suffix = add_suffix(fqcn);
        const std::string simple = class_simple_name(fqcn);
        const std::string pkg = class_package_name(fqcn);

        std::vector<const FileEntry *> candidates;
        candidates.reserve(8);

        for (const FileEntry &fe : files_) {
            if (ends_with(fe.rel_path, suffix)) {
                candidates.push_back(&fe);
            }
        }

        if (candidates.empty()) {
            const std::string file_name = simple + ".java";
            for (const FileEntry &fe : files_) {
                if (ends_with(fe.rel_path, file_name)) {
                    candidates.push_back(&fe);
                }
            }
        }

        if (candidates.empty()) {
            out.found = false;
            out.reason = "no candidates by path";
            return out;
        }

        struct Scored
        {
            const FileEntry *fe = nullptr;
            int score = 0;
            bool pkg_ok = false;
            bool decl_ok = false;
        };

        std::vector<Scored> scored;
        scored.reserve(candidates.size());

        for (const FileEntry *fe : candidates) {
            Scored s;
            s.fe = fe;
            s.pkg_ok = file_contains_package_line(fe->abs_path, pkg);
            s.decl_ok = file_contains_type_decl(fe->abs_path, simple);

            s.score = score_path(fe->rel_path);
            if (s.pkg_ok) {
                s.score += 30;
            }
            if (s.decl_ok) {
                s.score += 30;
            }

            scored.push_back(s);
        }

        std::sort(scored.begin(), scored.end(),
                  [](const Scored &a, const Scored &b)
                  {
                      return a.score > b.score;
                  });

        const Scored &best = scored.front();

        out.found = true;
        out.abs_path = best.fe->abs_path;
        out.rel_path = best.fe->rel_path;
        out.reason = "best score=" + std::to_string(best.score) +
                     " pkg_ok=" + (best.pkg_ok ? "1" : "0") +
                     " decl_ok=" + (best.decl_ok ? "1" : "0");

        return out;
    }

private:
    // const std::string &repo_root_;
    const std::vector<FileEntry> &files_;
};


std::unique_ptr<JavaLocator> make_text_java_locator(const std::vector<FileEntry> &files)
{
    return std::make_unique<TextJavaLocator>(files);
}


