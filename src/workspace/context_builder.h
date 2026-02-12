
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "workspace/scanner.h"


struct ContextSnippet
{
    std::string rel_path;
    std::string abs_path;

    std::string kind;   // method_declaration, class_declaration, etc.
    size_t start = 0;
    size_t end = 0;

    int score = 0;
    int hop = 0;

    std::string symbol; // which symbol caused this snippet (callee name)
    std::string text;
};

struct ContextStats
{
    int hops_used = 0;
    int snippets_written = 0;
    int bytes_written = 0;

    int symbols_seen = 0;
    int rg_queries = 0;
    int rg_hits_total = 0;
};

struct ContextRequest
{
    std::string repo_root;

    std::string anchor_class_fqcn;
    std::string anchor_method;

    // Search config
    std::vector<std::string> globs = {"*.java"};
    std::vector<std::string> excludes = {"codegen/**"};
};

struct ContextOptions
{
    int max_hops = 2;
    int max_snippets = 20;
    int max_bytes = 120000;

    int max_symbols_per_method = 12;
    int max_rg_hits_per_symbol = 6;
    int max_snippets_per_symbol = 1;

    bool include_anchor_in_snippets = true;
};

struct ContextPack
{
    std::vector<ContextSnippet> snippets;
    ContextStats stats;
};

ContextPack build_context_pack(const ContextRequest &req,
                               const ContextOptions &opt,
                               const std::vector<FileEntry> &files);

