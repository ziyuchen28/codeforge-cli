
#pragma once

#include <cstdint>
#include <string>
#include <vector>


struct RgHit
{
    std::string abs_path;
    std::string rel_path;

    uint64_t line_number = 0;
    // byte offset into the file for the match start
    uint64_t match_byte_offset = 0;
    // submatch end-start
    uint32_t match_len = 0;
};

struct RgQuery
{
    std::string pattern; // regex if not -F
    std::vector<std::string> globs;      
    std::vector<std::string> excludes;  

    bool fixed_string = false; // -F 
};

struct RgResult
{
    int exit_code = -1;                 
    std::string error;                   
    std::vector<RgHit> hits;
};

// run rg --json and parses "match" events.
// repo_root can be relative or absolute.
RgResult rg_search_json(const std::string &repo_root, const RgQuery &q);

