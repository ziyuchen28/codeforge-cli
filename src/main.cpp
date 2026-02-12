
#include <cstdio>
#include <cstring>
#include "cli/commands.h"
#include "app/codegen_runner.h"

static void usage(const char *argv0) 
{
  std::fprintf(stderr,
               "Usage: %s [--prompt <path>] [--py <python>] [--script <llm_adaptor.py>] [--out <path>]\n"
               "Defaults: --prompt prompt.txt --py python3 --script python/llm_adaptor.py --out answer.txt\n",
               argv0);
}


int main(int argc, char **argv) 
{
  int rc = 0;
  if (cli::handle(argc, argv, &rc)) return rc;
  const char *prompt = "prompt.txt";
  const char *py = "python3";
  const char *script = "python/llm_adaptor.py";
  const char *out_path = "generated.txt";

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--prompt") == 0) {
      if (++i >= argc) { usage(argv[0]); return 1; }
      prompt = argv[i];
    } else if (std::strcmp(argv[i], "--py") == 0) {
      if (++i >= argc) { usage(argv[0]); return 1; }
      py = argv[i];
    } else if (std::strcmp(argv[i], "--script") == 0) {
      if (++i >= argc) { usage(argv[0]); return 1; }
      script = argv[i];
    } else if (std::strcmp(argv[i], "--out") == 0) {
      if (++i >= argc) { usage(argv[0]); return 1; }
      out_path = argv[i];
    } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
      usage(argv[0]);
      return 1;
    }
  }

  return codegen(py, script, prompt, out_path);
}
