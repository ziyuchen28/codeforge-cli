
#include "workspace/scanner.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>


namespace cli {

static void usage_scan(const char *argv0) 
{
  std::fprintf(stderr,
               "Usage: %s scan [--repo-root <path>] [--limit <N>]\n"
               "Defaults: --repo-root .. --limit 20\n",
               argv0);
}


int cmd_scan(int argc, char **argv) 
{
    const char *repo_root; 
    int limit = 1024;
    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--repo-root") == 0) {
            if (++i >= argc) { usage_scan(argv[0]); return 1; }
            repo_root = argv[i];
        } else if (std::strcmp(argv[i], "--limit") == 0) {
            if (++i >= argc) { usage_scan(argv[0]); return 1; }
            limit = std::atoi(argv[i]);
            if (limit < 0) limit = 0;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage_scan(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage_scan(argv[0]);
            return 1;
        }
    }

    ScanOptions opt;
    std::vector<FileEntry> files = scan_workspace(repo_root, opt);

    std::error_code ec;
    std::string abs_root = std::filesystem::absolute(repo_root, ec).string();
    if (ec) abs_root = repo_root;

    std::printf("repo_root: %s\n", abs_root.c_str());
    std::printf("java_files: %zu\n", files.size());

    int n = limit;
    if (n > static_cast<int>(files.size())) n = static_cast<int>(files.size());
    for (int i = 0; i < n; i++) {
        std::printf("%s\n", files[i].rel_path.c_str());
    }

    return 0;
}

} // namespace cli
