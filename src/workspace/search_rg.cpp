
#include "workspace/search_rg.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <poll.h>
#include <string>
#include <vector>

#include <unistd.h>

#include "sys/error.h"
#include "sys/io.h"
#include "sys/process.h"


namespace fs = std::filesystem;


static bool extract_json_u64(const std::string &line, const char *key, uint64_t *out)
{
    size_t p = line.find(key);
    if (p == std::string::npos) {
        return false;
    }

    p += std::strlen(key);
    while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) {
        p++;
    }
    uint64_t v = 0;
    bool any = false;
    while (p < line.size() && line[p] >= '0' && line[p] <= '9') {
        any = true;
        v = (v * 10) + static_cast<uint64_t>(line[p] - '0');
        p++;
    }
    if (!any) {
        return false;
    }
    *out = v;
    return true;
}


static bool extract_json_string_after(const std::string &line, const char *prefix, std::string *out)
{
    size_t p = line.find(prefix);
    if (p == std::string::npos) {
        return false;
    }

    p += std::strlen(prefix);

    std::string s;
    s.reserve(256);

    bool esc = false;

    while (p < line.size()) {
        char c = line[p++];
        if (!esc) {
            // should not expect thing like \n \t
            if (c == '\\') {
                esc = true;
                continue;
            }
            if (c == '"') {
                *out = s;
                return true;
            }
            s.push_back(c);
        } else {
            s.push_back(c);
            esc = false;
        }
    }
    return false;
}


static bool parse_match_line(const std::string &line, const std::string &repo_abs, RgHit *out)
{

    // {"type":"match",...}
    if (line.find("\"type\":\"match\"") == std::string::npos) {
        return false;
    }

    //  "path":{"text":"..."}
    std::string path_text;
    if (!extract_json_string_after(line, "\"path\":{\"text\":\"", &path_text)) {
        return false;
    }

    uint64_t line_number = 0;
    (void)extract_json_u64(line, "\"line_number\":", &line_number);

    // absolute_offset (start byte of the "lines.text" chunk)
    uint64_t abs_off = 0;
    if (!extract_json_u64(line, "\"absolute_offset\":", &abs_off)) {
        // Some rg builds may omit this; without it we can't compute global offsets.
        return false;
    }

    // submatch start/end (first one)
    uint64_t sub_start = 0;
    uint64_t sub_end = 0;

    // Find first "start": after "submatches"
    size_t sm = line.find("\"submatches\":[");
    if (sm == std::string::npos) {
        return false;
    }

    // Parse first start/end after that.
    std::string tail = line.substr(sm);

    if (!extract_json_u64(tail, "\"start\":", &sub_start)) {
        return false;
    }
    if (!extract_json_u64(tail, "\"end\":", &sub_end)) {
        return false;
    }
    if (sub_end < sub_start) {
        return false;
    }

    //  Normalize relative to repo_abs.
    fs::path p = fs::path(path_text);
    fs::path abs_path;

    if (p.is_absolute()) {
        abs_path = p;
    } else {
        abs_path = fs::path(repo_abs) / p;
    }

    abs_path = abs_path.lexically_normal();

    fs::path rel_path = abs_path.lexically_relative(fs::path(repo_abs));
    std::string rel = rel_path.empty() ? abs_path.string() : rel_path.string();

    out->abs_path = abs_path.string();
    out->rel_path = rel;
    out->line_number = line_number;
    out->match_byte_offset = abs_off + sub_start;
    out->match_len = static_cast<uint32_t>(sub_end - sub_start);
    return true;
}


RgResult rg_search_json(const std::string &repo_root, const RgQuery &q)
{
    RgResult res;

    if (q.pattern.empty()) {
        res.exit_code = 2;
        res.error = "empty pattern";
        return res;
    }

    std::error_code ec;
    std::string repo_abs = fs::absolute(repo_root, ec).string();
    if (ec) {
        repo_abs = repo_root;
    }

    SpawnSpec spec;
    spec.exe = "rg";

    spec.argv.push_back("rg");
    spec.argv.push_back("--json");

    if (q.fixed_string) {
        spec.argv.push_back("-F");
    }

    for (const std::string &g : q.globs) {
        spec.argv.push_back("-g");
        spec.argv.push_back(g);
    }

    for (const std::string &x : q.excludes) {
        // ripgrep exclude glob: -g '!pattern'
        spec.argv.push_back("-g");
        spec.argv.push_back("!" + x);
    }

    spec.argv.push_back(q.pattern);
    spec.argv.push_back(repo_root);

    ChildProcess cp = spawn(spec);
    // not needed
    cp.stdin_w.close();

    pollfd fds[2];
    fds[0].fd = cp.stdout_r.get();
    fds[0].events = POLLIN;
    fds[1].fd = cp.stderr_r.get();
    fds[1].events = POLLIN;

    bool out_open = true;
    bool err_open = true;

    std::string line_buf;
    line_buf.reserve(64 * 1024);

    char buf[64 * 1024];

    while (out_open || err_open) {
        fds[0].events = out_open ? POLLIN : 0;
        fds[1].events = err_open ? POLLIN : 0;

        int rc = poll(fds, 2, -1);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            die("poll");
        }

        if (out_open && (fds[0].revents & (POLLIN | POLLHUP))) {
            ssize_t r = read(cp.stdout_r.get(), buf, sizeof(buf));
            if (r < 0) {
                if (errno != EINTR) {
                    out_open = false;
                    cp.stdout_r.close();
                }
            } else if (r == 0) {
                out_open = false;
                cp.stdout_r.close();
            } else {
                line_buf.append((buf), static_cast<size_t>(r));

                // read could return partial lines
                for (;;) {
                    size_t nl = line_buf.find('\n');
                    if (nl == std::string::npos) {
                        break;
                    }

                    std::string line = line_buf.substr(0, nl);
                    line_buf.erase(0, nl + 1);

                    RgHit hit;
                    if (parse_match_line(line, repo_abs, &hit)) {
                        res.hits.push_back(std::move(hit));
                    }
                }
            }
        }

        if (err_open && (fds[1].revents & (POLLIN | POLLHUP))) {
            ssize_t r = read(cp.stderr_r.get(), buf, sizeof(buf));
            if (r < 0) {
                if (errno != EINTR) {
                    err_open = false;
                    cp.stderr_r.close();
                }
            } else if (r == 0) {
                err_open = false;
                cp.stderr_r.close();
            } else {
                if (write_all(STDERR_FILENO, buf, static_cast<size_t>(r)) < 0) {
                    die("write(stderr)");
                }
            }
        }
    }

    ExitStatus es = wait_child(cp.pid);
    res.exit_code = es.as_shell_code();

    // ripgrep conventions: exit code 0 = match found, 1 = no match, 2 = error.
    if (res.exit_code == 2) {
        res.error = "rg failed (exit=2)";
    }

    return res;
}

