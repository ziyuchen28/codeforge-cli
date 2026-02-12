
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "workspace/java/locator.h"
#include "workspace/scanner.h"

namespace cli
{

static void usage_locate(const char *argv0)
{
    std::fprintf(stderr,
                 "Usage: %s locate --class <FQCN> [--repo-root <path>] [--verbose]\n"
                 "Defaults: --repo-root ..\n"
                 "Example:  %s locate --class com.foo.Bar --repo-root ..\n",
                 argv0, argv0);
}


int cmd_locate(int argc, char **argv)
{
    const char *repo_root = "..";
    const char *fqcn = nullptr;
    bool verbose = false;

    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--repo-root") == 0) {
            if (++i >= argc) {
                usage_locate(argv[0]);
                return 2;
            }
            repo_root = argv[i];
        } else if (std::strcmp(argv[i], "--class") == 0) {
            if (++i >= argc) {
                usage_locate(argv[0]);
                return 2;
            }
            fqcn = argv[i];
        } else if (std::strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage_locate(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage_locate(argv[0]);
            return 2;
        }
    }

    if (!fqcn) {
        std::fprintf(stderr, "Missing required flag: --class <FQCN>\n");
        usage_locate(argv[0]);
        return 2;
    }

    ScanOptions opt;
    std::vector<FileEntry> files = scan_workspace(repo_root, opt);

    std::error_code ec;
    std::string abs_root = std::filesystem::absolute(repo_root, ec).string();
    if (ec) { abs_root = repo_root; }

    std::unique_ptr<JavaLocator> locator = make_text_java_locator(files);

    ClassLocation loc = locator->locate_class(fqcn);

    if (verbose) {
        std::printf("repo_root: %s\n", abs_root.c_str());
        std::printf("java_files: %zu\n", files.size());
    }

    if (!loc.found) {
        std::printf("found: 0\n");
        std::printf("class: %s\n", fqcn);
        std::printf("reason: %s\n", loc.reason.c_str());
        return 1;
    }

    std::printf("found: 1\n");
    std::printf("class: %s\n", fqcn);
    std::printf("file: %s\n", loc.rel_path.c_str());
    if (verbose) {
        std::printf("abs_file: %s\n", loc.abs_path.c_str());
    }
    std::printf("reason: %s\n", loc.reason.c_str());
    return 0;
}

} // namespace cli
