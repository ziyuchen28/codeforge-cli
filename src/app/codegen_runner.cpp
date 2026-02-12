
#include "app/codegen_runner.h"

#include <string>
#include <cstring>
#include <vector>
#include <fcntl.h>  
#include <unistd.h>  
#include "sys/io.h"
#include "sys/process.h"
#include "sys/error.h"


int codegen(const char *python_exe,
            const char *script_path,
            const char *prompt_path, 
            const char *out_path)
{
  SpawnSpec spec;
  spec.exe = python_exe;
  spec.argv = { python_exe, script_path };

  ChildProcess cp = spawn(spec);

  // pipe prompt to child
  write_file_to_fd(prompt_path, cp.stdin_w.get());
  // signal EOF
  cp.stdin_w.close();

  int out_fd = STDOUT_FILENO; 
  Fd out_file;
  if (out_path && std::strcmp(out_path, "-") != 0) { // - default stdout
    int fd = open(out_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) die("open(out_path)");
    out_file.reset(fd);
    out_fd = out_file.get();
  }

  // read results from child
  // stream_to_parent(cp);
  stream_to_parent(cp, out_fd);

  ExitStatus es = wait_child(cp.pid);
  return es.as_shell_code();
}

