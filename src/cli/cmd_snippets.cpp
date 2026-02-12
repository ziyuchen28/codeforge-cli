#include "cli/commands.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "workspace/search_rg.h"
#include "workspace/java/snippet_from_hit.h"

#include "sys/error.h"
#include "sys/fd.h"
#include "sys/io.h"

#include <fcntl.h>
#include <unistd.h>

namespace cli
{

static void usage_snippets(const char *argv0)
{
    std::fprintf(stderr,
                 "Usage: %s snippets --pattern <regex> [--repo-root <path>] [--glob <glob>]... [--exclude <glob>]... [--limit <N>] [--out <path|->] [--fixed]\n"
                 "Defaults: --repo-root .. --glob *.java --exclude codegen/** --limit 20 --out snippets.txt\n"
                 "Example:  %s snippets --repo-root .. --pattern 'charge\\(' --glob '*.java' --out snippets.txt\n",
                 argv0, argv0);
}


static int open_out_fd(const char *out_path, Fd *out_file)
{
    if (!out_path || std::strcmp(out_path, "-") == 0) {
        return STDOUT_FILENO;
    }

    int fd = open(out_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        die("open(out_path)");
    }
    out_file->reset(fd);
    return out_file->get();
}


int cmd_snippets(int argc, char **argv)
{
    const char *repo_root = "..";
    const char *pattern = nullptr;
    const char *out_path = "etc/snippets.txt";
    bool fixed = false;
    int limit = 20;

    RgQuery q;
    q.globs.push_back("*.java");
    q.excludes.push_back("codegen/**");

    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--repo-root") == 0) {
            if (++i >= argc) { usage_snippets(argv[0]); return 2; }
            repo_root = argv[i];
        } else if (std::strcmp(argv[i], "--pattern") == 0) {
            if (++i >= argc) { usage_snippets(argv[0]); return 2; }
            pattern = argv[i];
        } else if (std::strcmp(argv[i], "--glob") == 0) {
            if (++i >= argc) { usage_snippets(argv[0]); return 2; }
            q.globs.push_back(argv[i]);
        } else if (std::strcmp(argv[i], "--exclude") == 0) {
            if (++i >= argc) { usage_snippets(argv[0]); return 2; }
            q.excludes.push_back(argv[i]);
        } else if (std::strcmp(argv[i], "--limit") == 0) {
            if (++i >= argc) { usage_snippets(argv[0]); return 2; }
            limit = std::atoi(argv[i]);
            if (limit < 0) limit = 0;
        } else if (std::strcmp(argv[i], "--out") == 0) {
            if (++i >= argc) { usage_snippets(argv[0]); return 2; }
            out_path = argv[i];
        } else if (std::strcmp(argv[i], "--fixed") == 0) {
            fixed = true;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage_snippets(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage_snippets(argv[0]);
            return 2;
        }
    }

    if (!pattern) {
        std::fprintf(stderr, "Missing required flag: --pattern <regex>\n");
        usage_snippets(argv[0]);
        return 2;
    }

    q.pattern = pattern;
    q.fixed_string = fixed;

    RgResult res = rg_search_json(repo_root, q);
    if (res.exit_code == 2) {
        std::fprintf(stderr, "rg failed: %s\n", res.error.c_str());
        return 1;
    }

    size_t n = res.hits.size();
    if (static_cast<size_t>(limit) < n) {
        n = static_cast<size_t>(limit);
    }

    Fd out_file;
    int out_fd = open_out_fd(out_path, &out_file);

    std::string header;
    header += "pattern: " + std::string(pattern) + "\n";
    header += "hits: " + std::to_string(res.hits.size()) + "\n";
    header += "showing: " + std::to_string(n) + "\n";
    header += "====\n";

    if (write_all(out_fd, header.data(), header.size()) < 0) {
        die("write(out)");
    }

    for (size_t i = 0; i < n; i++) {
        const RgHit &h = res.hits[i];

        HitSnippet snip = snippet_from_hit(h.abs_path, h.rel_path, h.match_byte_offset);

        std::string block;
        block += "\n[SNIPPET]\n";
        block += "file: " + h.rel_path + "\n";
        block += "line: " + std::to_string(h.line_number) + "\n";
        block += "hit_byte: " + std::to_string(h.match_byte_offset) + "\n";

        if (!snip.found) {
            block += "found: 0\n";
            block += "reason: " + snip.reason + "\n";
            block += "[/SNIPPET]\n";
            if (write_all(out_fd, block.data(), block.size()) < 0) {
                die("write(out)");
            }
            continue;
        }

        block += "found: 1\n";
        block += "kind: " + snip.kind + "\n";
        block += "range: " + std::to_string(snip.start) + ".." + std::to_string(snip.end) + "\n";
        block += "----\n";
        block += snip.text;
        block += "\n[/SNIPPET]\n";

        if (write_all(out_fd, block.data(), block.size()) < 0) {
            die("write(out)");
        }
    }

    return 0;
}

} // namespace cli
