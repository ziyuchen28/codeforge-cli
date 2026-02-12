

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

#include "sys/error.h"
#include "sys/fd.h"
#include "sys/io.h"
#include "workspace/java/extractor.h"
#include "workspace/java/locator.h"
#include "workspace/scanner.h"

namespace cli
{


static void usage_extract(const char *argv0)
{
    std::fprintf(stderr,
                 "Usage: %s extract --class <FQCN> --method <name> [--repo-root <path>] [--out <path|->] [--verbose]\n"
                 "Defaults: --repo-root .. --out answer.txt\n"
                 "Example:  %s extract --repo-root .. --class com.foo.Bar --method baz --out answer.txt\n",
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


int cmd_extract(int argc, char **argv)
{
    const char *repo_root = "..";
    const char *fqcn = nullptr;
    const char *method = nullptr;
    const char *out_path = "src/cli/test.txt";
    bool verbose = false;

    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--repo-root") == 0) {
            if (++i >= argc) { usage_extract(argv[0]); return 2; }
            repo_root = argv[i];
        } else if (std::strcmp(argv[i], "--class") == 0) {
            if (++i >= argc) { usage_extract(argv[0]); return 2; }
            fqcn = argv[i];
        } else if (std::strcmp(argv[i], "--method") == 0) {
            if (++i >= argc) { usage_extract(argv[0]); return 2; }
            method = argv[i];
        } else if (std::strcmp(argv[i], "--out") == 0) {
            if (++i >= argc) { usage_extract(argv[0]); return 2; }
            out_path = argv[i];
        } else if (std::strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage_extract(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage_extract(argv[0]);
            return 2;
        }
    }

    if (!fqcn || !method) {
        std::fprintf(stderr, "Missing required flags: --class and/or --method\n");
        usage_extract(argv[0]);
        return 2;
    }

    ScanOptions opt;
    std::vector<FileEntry> files = scan_workspace(repo_root, opt);

    std::error_code ec;
    std::string abs_root = std::filesystem::absolute(repo_root, ec).string();
    if (ec) {
        abs_root = repo_root;
    }

    std::unique_ptr<JavaLocator> locator = make_text_java_locator(files);

    ClassLocation loc = locator->locate_class(fqcn);
    if (!loc.found) {
        std::fprintf(stderr, "locate failed: %s\n", loc.reason.c_str());
        return 1;
    }

    Method snip =
        extract_method_from_file(loc.abs_path, loc.rel_path, method);

    if (!snip.found) {
        std::fprintf(stderr, "extract failed: %s\n", snip.reason.c_str());
        return 1;
    }

    Fd out_file;
    int out_fd = open_out_fd(out_path, &out_file);

    std::string header;
    header += "FILE: " + snip.rel_path + "\n";
    header += "METHOD: " + std::string(method) + "\n";
    header += "REASON: " + snip.reason + "\n";
    header += "BYTE_RANGE: " + std::to_string(snip.start) + ".." + std::to_string(snip.end) + "\n";
    header += "----\n";

    if (write_all(out_fd, header.data(), header.size()) < 0) {
        die("write(out)");
    }
    if (write_all(out_fd, snip.text.data(), snip.text.size()) < 0) {
        die("write(out)");
    }
    if (write_all(out_fd, "\n", 1) < 0) {
        die("write(out)");
    }

    if (verbose) {
        std::printf("found: 1\n");
        std::printf("class: %s\n", fqcn);
        std::printf("file: %s\n", loc.rel_path.c_str());
        std::printf("method: %s\n", method);
        std::printf("out: %s\n", out_path);
    }

    return 0;
}

} // namespace cli
