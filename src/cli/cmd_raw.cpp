

#include <cstdio>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include "sys/error.h"
#include "sys/fd.h"
#include "sys/io.h"
#include "sys/process.h"

namespace cli
{

static void usage_raw(const char *argv0)
{
    std::fprintf(stderr,
                 "Usage: %s raw --prompt <file> [--py <python>] [--script <llm.py>] [--out <answer.txt>]\n"
                 "Sends the entire prompt file to the LLM unchanged.\n"
                 "Defaults: --py python3 --script python/llm.py --out answer.txt\n",
                 argv0);
}

static bool read_file_to_string(const char *path, std::string *out)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    Fd f;
    f.reset(fd);

    std::string s;
    s.reserve(64 * 1024);

    uint8_t buf[64 * 1024];
    for (;;) {
        ssize_t r = read(f.get(), buf, sizeof(buf));
        if (r < 0) {
            return false;
        }
        if (r == 0) {
            break;
        }
        s.append(reinterpret_cast<const char *>(buf), static_cast<size_t>(r));
    }

    *out = std::move(s);
    return true;
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

    if (write_all(cp.stdin_w.get(),
                       reinterpret_cast<const uint8_t *>(prompt.data()),
                       prompt.size()) < 0) {
        die("write(child stdin)");
    }
    cp.stdin_w.close();

    int fd = open(answer_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        die("open(answer)");
    }

    Fd out_file;
    out_file.reset(fd);

    stream_to_parent(cp, out_file.get());

    ExitStatus es = wait_child(cp.pid);
    int rc = es.as_shell_code();
    if (rc != 0) {
        std::fprintf(stderr, "LLM process exited with code %d\n", rc);
    }
}

int cmd_raw(int argc, char **argv)
{
    const char *prompt_path = nullptr;
    const char *py = "python3";
    const char *script = "python/llm_adaptor.py";
    const char *out_path = "etc/answer.txt";

    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--prompt") == 0) {
            if (++i >= argc) { usage_raw(argv[0]); return 2; }
            prompt_path = argv[i];
        } else if (std::strcmp(argv[i], "--py") == 0) {
            if (++i >= argc) { usage_raw(argv[0]); return 2; }
            py = argv[i];
        } else if (std::strcmp(argv[i], "--script") == 0) {
            if (++i >= argc) { usage_raw(argv[0]); return 2; }
            script = argv[i];
        } else if (std::strcmp(argv[i], "--out") == 0) {
            if (++i >= argc) { usage_raw(argv[0]); return 2; }
            out_path = argv[i];
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage_raw(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage_raw(argv[0]);
            return 2;
        }
    }

    if (!prompt_path) {
        std::fprintf(stderr, "Missing required flag: --prompt <file>\n");
        usage_raw(argv[0]);
        return 2;
    }

    std::string prompt;
    if (!read_file_to_string(prompt_path, &prompt)) {
        die("read(prompt)");
    }

    run_python_llm(py, script, prompt, out_path);

    std::printf("Wrote %s\n", out_path);
    return 0;
}

} // namespace cli
