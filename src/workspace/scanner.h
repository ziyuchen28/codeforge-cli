
#pragma once

#include <cstdint>
#include <string>
#include <vector>


struct FileEntry 
{
    std::string rel_path;   // relative to scan root
    std::string abs_path;   
    uint64_t    size_bytes;
};


struct ScanOptions 
{
    std::vector<std::string> exclude_dir_names = {
        ".git", "build", "build_config", "target", "out", ".idea", ".venv", "node_modules",
        "codegen", "resources", "environment-config", "config", ".run", ".oca" 
    };
    std::vector<std::string> include_exts = {".java"};
    // to do - fine tune
    uint64_t max_file_size_bytes = 2ull * 1024 * 1024; // 2 MB
};


std::vector<FileEntry> scan_workspace(const std::string &root_dir,
                                      const ScanOptions &opt);

