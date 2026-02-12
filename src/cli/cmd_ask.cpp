

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
#include "sys/process.h"

#include "workspace/context_builder.h"
#include "workspace/prompt_spec.h"
#include "workspace/scanner.h"

namespace cli
{

static void usage_ask(const char *argv0)
{
    std::fprintf(stderr,
                 "Usage: %s ask --prompt <prompt.txt> [--py <python>] [--script <llm_adaptor.py>]\n"
                 "Writes: context.txt and answer.txt in current directory.\n"
                 "Defaults: --py python3 --script python/llm_adaptor.py\n",
                 argv0);
}

static int scope_to_hops(Scope s)
{
    switch (s) {
        case Scope::Local: return 0;
        case Scope::Deps:  return 1;
        case Scope::Deep:  return 3;
        case Scope::Auto:  return -1;
    }
    return -1;
}

static void write_str(int fd, const std::string &s)
{
    if (write_all(fd, reinterpret_cast<const uint8_t *>(s.data()), s.size()) < 0) {
        die("write");
    }
}

static void write_context_file(const ContextRequest &req,
                               const ContextOptions &opt,
                               const ContextPack &pack,
                               const char *path)
{
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        die("open(context.txt)");
    }

    Fd f;
    f.reset(fd);

    std::string head;
    head += "[CONTEXT]\n";
    head += "repo_root: " + req.repo_root + "\n";
    head += "anchor_class: " + req.anchor_class_fqcn + "\n";
    head += "anchor_method: " + req.anchor_method + "\n";
    head += "max_hops: " + std::to_string(opt.max_hops) + "\n";
    head += "max_snippets: " + std::to_string(opt.max_snippets) + "\n";
    head += "max_bytes: " + std::to_string(opt.max_bytes) + "\n";
    head += "====\n";
    write_str(f.get(), head);

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
        write_str(f.get(), block);
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
    write_str(f.get(), tail);
}

static std::string build_final_prompt(const PromptSpec &spec,
                                      const ContextRequest &req,
                                      const ContextOptions &opt,
                                      const ContextPack &pack)
{
    // v1 prompt: general-purpose one-shot codegen.
    // Output contract: plain text. If multiple files, use FILE: <path> headers and full file contents.
    std::string p;

    p += "You are a senior software engineer. Follow instructions carefully.\n";
    p += "\n";
    p += "TASK:\n";
    p += spec.task_text.empty() ? "(no task text provided)\n" : (spec.task_text + "\n");
    p += "\n";
    p += "OUTPUT FORMAT:\n";
    p += "- Output plain text only (no markdown fences).\n";
    p += "- If you propose changes to files, output the COMPLETE contents of each file.\n";
    p += "- Use this exact header before each file:\n";
    p += "  FILE: <relative/path>\n";
    p += "- If only one file is involved, output that file only.\n";
    p += "- Do not include explanations unless explicitly asked in TASK.\n";
    p += "\n";
    p += "CONTEXT:\n";
    p += "repo_root: " + req.repo_root + "\n";
    p += "anchor_class: " + req.anchor_class_fqcn + "\n";
    p += "anchor_method: " + req.anchor_method + "\n";
    p += "max_hops: " + std::to_string(opt.max_hops) + "\n";
    p += "\n";

    for (const ContextSnippet &s : pack.snippets) {
        p += "\n";
        p += "[SNIPPET]\n";
        p += "hop: " + std::to_string(s.hop) + "\n";
        p += "symbol: " + s.symbol + "\n";
        p += "file: " + s.rel_path + "\n";
        p += "kind: " + s.kind + "\n";
        p += "----\n";
        p += s.text;
        p += "\n[/SNIPPET]\n";
    }

    p += "\nEND.\n";
    return p;
}

static void run_python_llm(const char *py,
                           const char *script,
                           const std::string &prompt,
                           const char *answer_path)
{
    SpawnSpec spec;
    spec.exe = py;
    spec.argv = {py, script};

    ChildProcess cp = spawn(spec);

    // Write prompt to child stdin
    if (write_all(cp.stdin_w.get(),
                       reinterpret_cast<const uint8_t *>(prompt.data()),
                       prompt.size()) < 0) {
        die("write(child stdin)");
    }
    cp.stdin_w.close(); // EOF

    int fd = open(answer_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        die("open(answer.txt)");
    }

    Fd out_file;
    out_file.reset(fd);

    // Stream child stdout -> answer.txt, child stderr -> terminal stderr
    stream_to_parent(cp, out_file.get());

    ExitStatus es = wait_child(cp.pid);
    int rc = es.as_shell_code();
    if (rc != 0) {
        std::fprintf(stderr, "LLM process exited with code %d\n", rc);
    }
}

int cmd_ask(int argc, char **argv)
{
    const char *prompt_path = nullptr;
    const char *py = "python3";
    const char *script = "python/llm_adaptor.py";

    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--prompt") == 0) {
            if (++i >= argc) { usage_ask(argv[0]); return 2; }
            prompt_path = argv[i];
        } else if (std::strcmp(argv[i], "--py") == 0) {
            if (++i >= argc) { usage_ask(argv[0]); return 2; }
            py = argv[i];
        } else if (std::strcmp(argv[i], "--script") == 0) {
            if (++i >= argc) { usage_ask(argv[0]); return 2; }
            script = argv[i];
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage_ask(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage_ask(argv[0]);
            return 2;
        }
    }

    if (!prompt_path) {
        std::fprintf(stderr, "Missing required flag: --prompt <file>\n");
        usage_ask(argv[0]);
        return 2;
    }

    PromptSpec spec = parse_prompt_file(prompt_path);
    if (!spec.ok) {
        std::fprintf(stderr, "prompt parse error: %s\n", spec.error.c_str());
        return 2;
    }

    // Defaults
    ContextRequest req;
    req.repo_root = spec.repo_root.empty() ? ".." : spec.repo_root;
    req.anchor_class_fqcn = spec.anchor_class_fqcn;
    req.anchor_method = spec.anchor_method;

    ContextOptions opt;
    int hops = scope_to_hops(spec.scope);
    if (hops >= 0) {
        opt.max_hops = hops;
    }

    // Scan workspace and build context pack
    ScanOptions scan_opt;
    std::vector<FileEntry> files = scan_workspace(req.repo_root, scan_opt);

    ContextPack pack = build_context_pack(req, opt, files);
    if (pack.snippets.empty()) {
        std::fprintf(stderr, "ask: context pack is empty (anchor not found or extraction failed)\n");
        return 1;
    }

    // intermediate context file for debugging
    write_context_file(req, opt, pack, "etc/context.txt");

    std::string final_prompt = build_final_prompt(spec, req, opt, pack);

    // write to gen.txt
    run_python_llm(py, script, final_prompt, "etc/gen.txt");

    std::printf("Wrote etc/context.txt and generated code in etc/gen.txt\n");
    return 0;
}

} // namespace cli
