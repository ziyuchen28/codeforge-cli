
#pragma once

#include <string>


enum class Scope
{
    Auto,
    Local,
    Deps,
    Deep
};


struct PromptSpec
{
    bool ok = false;
    std::string error;

    std::string repo_root;            // optional; can default elsewhere
    std::string anchor_class_fqcn;    // required for context
    std::string anchor_method;        // required for context
    Scope scope = Scope::Auto;

    std::string task_text;            // optional for now; useful later for ask
};

PromptSpec parse_prompt_file(const std::string &path);

