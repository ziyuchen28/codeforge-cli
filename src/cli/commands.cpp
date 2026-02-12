
#include "cli/commands.h"

#include <cstring>

namespace cli 
{

int cmd_scan(int argc, char **argv); 
int cmd_locate(int argc, char **argv); 
int cmd_extract(int argc, char **argv); 
int cmd_search(int argc, char **argv);
int cmd_snippets(int argc, char **argv);
int cmd_prompt(int argc, char **argv);
int cmd_context(int argc, char **argv);
int cmd_ask(int argc, char **argv);
int cmd_raw(int argc, char **argv);

bool handle(int argc, char **argv, int *out_rc) 
{
    if (argc < 2) return false;
    if (std::strcmp(argv[1], "scan") == 0) {
        *out_rc = cmd_scan(argc, argv);
        return true;
    }
    if (std::strcmp(argv[1], "locate") == 0) {
        *out_rc = cmd_locate(argc, argv);
        return true;
    }
    if (std::strcmp(argv[1], "extract") == 0) {
        *out_rc = cmd_extract(argc, argv);
        return true;
    }
    if (std::strcmp(argv[1], "search") == 0) {
        *out_rc = cmd_search(argc, argv);
        return true;
    }
    if (std::strcmp(argv[1], "snippets") == 0) {
        *out_rc = cmd_snippets(argc, argv);
        return true;
    }
    if (std::strcmp(argv[1], "prompt") == 0) {
        *out_rc = cmd_prompt(argc, argv);
        return true;
    }
    if (std::strcmp(argv[1], "context") == 0) {
        *out_rc = cmd_context(argc, argv);
        return true;
    }
    if (std::strcmp(argv[1], "ask") == 0) {
        *out_rc = cmd_ask(argc, argv);
        return true;
    }
    if (std::strcmp(argv[1], "raw") == 0) {
        *out_rc = cmd_raw(argc, argv);
        return true;
    }
    return false;
}

} // namespace cli
