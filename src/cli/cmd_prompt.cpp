

#include <cstdio>
#include <cstring>
#include <string>

#include "workspace/prompt_spec.h"

namespace cli
{

static void usage_prompt(const char *argv0)
{
    std::fprintf(stderr,
                 "Usage: %s prompt --file <prompt.txt>\n"
                 "Example: %s prompt --file prompt.txt\n",
                 argv0, argv0);
}


static const char *scope_to_cstr(Scope s)
{
    switch (s) {
        case Scope::Auto:  return "auto";
        case Scope::Local: return "local";
        case Scope::Deps:  return "deps";
        case Scope::Deep:  return "deep";
    }
    return "auto";
}


int cmd_prompt(int argc, char **argv)
{
    const char *file = nullptr;

    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--file") == 0) {
            if (++i >= argc) {
                usage_prompt(argv[0]);
                return 2;
            }
            file = argv[i];
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage_prompt(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage_prompt(argv[0]);
            return 2;
        }
    }

    if (!file) {
        std::fprintf(stderr, "Missing required flag: --file\n");
        usage_prompt(argv[0]);
        return 2;
    }

    PromptSpec spec = parse_prompt_file(file);

    if (!spec.ok) {
        std::printf("ok: 0\n");
        std::printf("error: %s\n", spec.error.c_str());
        return 1;
    }

    std::printf("ok: 1\n");
    std::printf("repo_root: %s\n", spec.repo_root.c_str());
    std::printf("anchor_class: %s\n", spec.anchor_class_fqcn.c_str());
    std::printf("anchor_method: %s\n", spec.anchor_method.c_str());
    std::printf("scope: %s\n", scope_to_cstr(spec.scope));

    if (!spec.task_text.empty()) {
        std::printf("task:\n");
        std::printf("----\n");
        std::printf("%s\n", spec.task_text.c_str());
        std::printf("----\n");
    }

    return 0;
}

} // namespace cli
