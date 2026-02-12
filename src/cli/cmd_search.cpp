
#include "cli/commands.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "workspace/search_rg.h"


namespace cli
{


static void usage_search(const char *argv0)
{
    std::fprintf(stderr,
                 "Usage: %s search --pattern <regex> [--repo-root <path>] [--glob <glob>]... [--exclude <glob>]... [--limit <N>] [--fixed] [--verbose]\n"
                 "Defaults: --repo-root .. --glob *.java --exclude codegen/** --limit 50\n"
                 "Example:  %s search --repo-root .. --pattern \"charge\\\\(\" --glob \"*.java\" --limit 20\n",
                 argv0, argv0);
}


int cmd_search(int argc, char **argv)
{
    const char *repo_root = "..";
    const char *pattern = nullptr;
    bool fixed = false;
    bool verbose = false;
    int limit = 50;

    RgQuery q;
    q.globs.push_back("*.java");
    q.excludes.push_back("codegen/**");

    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--repo-root") == 0) {
            if (++i >= argc) { usage_search(argv[0]); return 2; }
            repo_root = argv[i];
        } else if (std::strcmp(argv[i], "--pattern") == 0) {
            if (++i >= argc) { usage_search(argv[0]); return 2; }
            pattern = argv[i];
        } else if (std::strcmp(argv[i], "--glob") == 0) {
            if (++i >= argc) { usage_search(argv[0]); return 2; }
            q.globs.push_back(argv[i]);
        } else if (std::strcmp(argv[i], "--exclude") == 0) {
            if (++i >= argc) { usage_search(argv[0]); return 2; }
            q.excludes.push_back(argv[i]);
        } else if (std::strcmp(argv[i], "--limit") == 0) {
            if (++i >= argc) { usage_search(argv[0]); return 2; }
            limit = std::atoi(argv[i]);
            if (limit < 0) limit = 0;
        } else if (std::strcmp(argv[i], "--fixed") == 0) {
            fixed = true;
        } else if (std::strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage_search(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage_search(argv[0]);
            return 2;
        }
    }

    if (!pattern) {
        std::fprintf(stderr, "Missing required flag: --pattern <regex>\n");
        usage_search(argv[0]);
        return 2;
    }

    q.pattern = pattern;
    q.fixed_string = fixed;

    RgResult res = rg_search_json(repo_root, q);

    if (!res.error.empty() && verbose) {
        std::fprintf(stderr, "rg error: %s\n", res.error.c_str());
    }

    std::printf("exit: %d\n", res.exit_code);
    std::printf("hits: %zu\n", res.hits.size());

    size_t n = res.hits.size();
    if (limit >= 0 && static_cast<size_t>(limit) < n) {
        n = static_cast<size_t>(limit);
    }

    for (size_t i = 0; i < n; i++) {
        const RgHit &h = res.hits[i];
        std::printf("%s:%llu byte=%llu len=%u\n",
                    h.rel_path.c_str(),
                    static_cast<unsigned long long>(h.line_number),
                    static_cast<unsigned long long>(h.match_byte_offset),
                    h.match_len);
    }

    // 0 not err, means no matches
    return (res.exit_code == 2) ? 1 : 0;
}

} // namespace cli
