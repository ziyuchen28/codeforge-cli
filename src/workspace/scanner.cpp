
#include "workspace/scanner.h"

#include <algorithm>
// #include <chrono>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>


namespace fs = std::filesystem;


static bool ends_with(const std::string &s, const std::string &suffix) 
{
    return s.size() >= suffix.size() && 
           std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}


// static int64_t file_time_to_epoch_seconds(const fs::file_time_type &ft) 
// {
//     using namespace std::chrono;
//     auto now_ft = fs::file_time_type::clock::now();
//     auto now_sys = system_clock::now();
//     auto sys_tp = time_point_cast<system_clock::duration>(ft - now_ft + now_sys);
//     return duration_cast<seconds>(sys_tp.time_since_epoch()).count();
// }
//

static bool has_included_ext(const std::vector<std::string> &exts,
                             const fs::path &p) 
{
    std::string s = p.string();
    for (const std::string &e : exts) {
        if (ends_with(s, e)) return true;
    }
    return false;
}


std::vector<FileEntry> scan_workspace(const std::string &root_dir,
                                      const ScanOptions &opt)
{
    std::error_code ec;

    // root
    fs::path root = fs::absolute(fs::path(root_dir), ec);
    if (ec) {
        root = fs::path(root_dir);
        ec.clear();
    }
    root = root.lexically_normal();

    // exclude
    std::unordered_set<std::string> skip;
    skip.reserve(opt.exclude_dir_names.size() * 2);
    for (const std::string &s : opt.exclude_dir_names) skip.insert(s);

    fs::directory_options dopts = fs::directory_options::skip_permission_denied;

    std::vector<FileEntry> out;
    out.reserve(4096);

    fs::recursive_directory_iterator it(root, dopts, ec);
    fs::recursive_directory_iterator end;

    while (!ec && it != end) {
        const fs::directory_entry &ent = *it;
        const fs::path p = ent.path();

        if (ent.is_directory(ec)) {
            ec.clear();
            std::string name = p.filename().string();
            if (skip.find(name) != skip.end()) {
                it.disable_recursion_pending();
            }
            ++it;
            continue;
        }
        ec.clear();

        if (!ent.is_regular_file(ec)) {
            ec.clear();
            ++it;
            continue;
        }
        ec.clear();

        if (!has_included_ext(opt.include_exts, p)) {
            ++it;
            continue;
        }

        // to do - fine tune
        uintmax_t sz = ent.file_size(ec);
        if (ec) {
            ec.clear();
            ++it;
            continue;
        }
        if (sz > opt.max_file_size_bytes) {
            ++it;
            continue;
        }

        FileEntry fe;
        // abs path
        fs::path abs = fs::absolute(p, ec);
        if (ec) {
            ec.clear();
            abs = p;
        }
        abs = abs.lexically_normal();
        fe.abs_path = abs.string();
        // rel path
        fs::path rel = abs.lexically_relative(root);
        fe.rel_path = rel.empty() ? fe.abs_path : rel.string();
        // size
        fe.size_bytes = static_cast<uint64_t>(sz);
        // // last modified
        // auto ft = ent.last_write_time(ec);
        // if (ec) {
        //     ec.clear();
        //     fe.mdftime_sec = 0;
        // } else {
        //     fe.mdftime_sec = file_time_to_epoch_seconds(ft);
        // }

        out.push_back(std::move(fe));
        ++it;
    }

    std::sort(out.begin(), 
              out.end(),
              [](const FileEntry &a, const FileEntry &b) {
                return a.rel_path < b.rel_path;
              });

    return out;
}

