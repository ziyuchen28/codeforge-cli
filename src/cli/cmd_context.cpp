#include "cli/commands.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include "sys/error.h"
#include "sys/fd.h"
#include "sys/io.h"

#include "workspace/context_builder.h"
#include "workspace/prompt_spec.h"
#include "workspace/scanner.h"

namespace cli
{

static void usage_context(const char *argv0)
{
    std::fprintf(stderr,
                 "Usage: %s context [--prompt <file>] [--repo-root <path>] [--class <FQCN>] [--method <name>] [--out <path|->]\n"
                 "                 [--max-hops N] [--max-snippets N] [--max-bytes N]\n"
                 "                 [--max-symbols-per-method N] [--max-rg-hits-per-symbol N] [--max-snippets-per-symbol N]\n"
                 "\n"
                 "Prompt format:\n"
                 "  [HINTS]\n"
                 "  repo_root=..\n"
                 "  anchor_class=com.foo.Bar\n"
                 "  anchor_method=baz\n"
                 "  scope=local|deps|deep|auto\n"
                 "  [/HINTS]\n"
                 "  [TASK] ... [/TASK] (optional)\n"
                 "\n"
                 "Defaults: --repo-root .. --out context.txt\n",
                 argv0);
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

static void write_str(int out_fd, const std::string &s)
{
    if (write_all(out_fd, reinterpret_cast<const uint8_t *>(s.data()), s.size()) < 0) {
        die("write(out)");
    }
}

static int scope_to_hops(Scope s)
{
    if (s == Scope::Local) {
        return 0;
    }
    if (s == Scope::Deps) {
        return 1;
    }
    if (s == Scope::Deep) {
        return 3;
    }
    return -1; // Auto: keep default unless overridden
}

static void write_pack(int out_fd,
                       const ContextRequest &req,
                       const ContextOptions &opt,
                       const ContextPack &pack)
{
    std::string head;
    head += "[CONTEXT]\n";
    head += "repo_root: " + req.repo_root + "\n";
    head += "anchor_class: " + req.anchor_class_fqcn + "\n";
    head += "anchor_method: " + req.anchor_method + "\n";
    head += "max_hops: " + std::to_string(opt.max_hops) + "\n";
    head += "max_snippets: " + std::to_string(opt.max_snippets) + "\n";
    head += "max_bytes: " + std::to_string(opt.max_bytes) + "\n";
    head += "====\n";
    write_str(out_fd, head);

    for (const ContextSnippet &s : pack.snippets) {
        std::string block;
        block += "\n[SNIPPET]\n";
        block += "hop: " + std::to_string(s.hop) + "\n";
        block += "score: " + std::to_string(s.score) + "\n";
        block += "symbol: " + s.symbol + "\n";
        block += "file: " + s.rel_path + "\n";
        block += "kind: " + s.kind + "\n";
        block += "range: " + std::to_string(s.start) + ".." + std::to_string(s.end) + "\n";
        block += "----\n";
        block += s.text;
        block += "\n[/SNIPPET]\n";
        write_str(out_fd, block);
    }

    std::string tail;
    tail += "\n[STATS]\n";
    tail += "hops_used: " + std::to_string(pack.stats.hops_used) + "\n";
    tail += "snippets_written: " + std::to_string(pack.stats.snippets_written) + "\n";
    tail += "bytes_written: " + std::to_string(pack.stats.bytes_written) + "\n";
    tail += "symbols_seen: " + std::to_string(pack.stats.symbols_seen) + "\n";
    tail += "rg_queries: " + std::to_string(pack.stats.rg_queries) + "\n";
    tail += "rg_hits_total: " + std::to_string(pack.stats.rg_hits_total) + "\n";
    tail += "[/STATS]\n";
    tail += "[/CONTEXT]\n";
    write_str(out_fd, tail);
}

int cmd_context(int argc, char **argv)
{
    const char *prompt_path = nullptr;

    const char *repo_root = nullptr;
    const char *fqcn = nullptr;
    const char *method = nullptr;
    const char *out_path = "context.txt";

    ContextOptions opt;

    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--prompt") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            prompt_path = argv[i];
        } else if (std::strcmp(argv[i], "--repo-root") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            repo_root = argv[i];
        } else if (std::strcmp(argv[i], "--class") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            fqcn = argv[i];
        } else if (std::strcmp(argv[i], "--method") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            method = argv[i];
        } else if (std::strcmp(argv[i], "--out") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            out_path = argv[i];
        } else if (std::strcmp(argv[i], "--max-hops") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            opt.max_hops = std::atoi(argv[i]);
        } else if (std::strcmp(argv[i], "--max-snippets") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            opt.max_snippets = std::atoi(argv[i]);
        } else if (std::strcmp(argv[i], "--max-bytes") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            opt.max_bytes = std::atoi(argv[i]);
        } else if (std::strcmp(argv[i], "--max-symbols-per-method") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            opt.max_symbols_per_method = std::atoi(argv[i]);
        } else if (std::strcmp(argv[i], "--max-rg-hits-per-symbol") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            opt.max_rg_hits_per_symbol = std::atoi(argv[i]);
        } else if (std::strcmp(argv[i], "--max-snippets-per-symbol") == 0) {
            if (++i >= argc) { usage_context(argv[0]); return 2; }
            opt.max_snippets_per_symbol = std::atoi(argv[i]);
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage_context(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage_context(argv[0]);
            return 2;
        }
    }

    // Defaults
    std::string repo_root_str = "..";
    std::string fqcn_str;
    std::string method_str;

    if (prompt_path) {
        PromptSpec spec = parse_prompt_file(prompt_path);
        if (!spec.ok) {
            std::fprintf(stderr, "prompt parse error: %s\n", spec.error.c_str());
            return 2;
        }

        if (!spec.repo_root.empty()) {
            repo_root_str = spec.repo_root;
        }
        fqcn_str = spec.anchor_class_fqcn;
        method_str = spec.anchor_method;

        int hops = scope_to_hops(spec.scope);
        if (hops >= 0) {
            opt.max_hops = hops;
        }
    }

    // CLI overrides prompt
    if (repo_root) {
        repo_root_str = repo_root;
    }
    if (fqcn) {
        fqcn_str = fqcn;
    }
    if (method) {
        method_str = method;
    }

    if (fqcn_str.empty() || method_str.empty()) {
        std::fprintf(stderr, "Missing anchor. Provide --prompt or both --class and --method.\n");
        usage_context(argv[0]);
        return 2;
    }

    ScanOptions scan_opt;
    std::vector<FileEntry> files = scan_workspace(repo_root_str, scan_opt);

    ContextRequest req;
    req.repo_root = repo_root_str;
    req.anchor_class_fqcn = fqcn_str;
    req.anchor_method = method_str;

    ContextPack pack = build_context_pack(req, opt, files);

    Fd out_file;
    int out_fd = open_out_fd(out_path, &out_file);

    write_pack(out_fd, req, opt, pack);

    if (pack.snippets.empty()) {
        std::fprintf(stderr, "context: no snippets produced (anchor not found or extraction failed)\n");
        return 1;
    }

    return 0;
}

} // namespace cli
