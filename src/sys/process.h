
#pragma once

#include <string>
#include <vector>
#include <sys/types.h>
#include "sys/fd.h"


struct SpawnSpec 
{
  std::string exe; // e.g. "python3" or "/path/to/python"
  std::vector<std::string> argv; 
};

// used by parent, child no knowledge of this 
struct ChildProcess 
{
  pid_t pid = -1;
  Fd stdin_w;   
  Fd stdout_r;  
  Fd stderr_r;  
};


struct ExitStatus 
{
  bool exited = false;
  int  exit_code = 1;
  bool signaled = false;
  int  term_signal = 0;
  int as_shell_code() const 
  {
    if (exited) return exit_code;
    if (signaled) return 128 + term_signal;
    return 1;
  }
};

ChildProcess spawn(const SpawnSpec &spec);

// Streams child's stdout -> parent stdout n child's stderr -> parent stderr.
void stream_to_parent(ChildProcess &cp);
void stream_to_parent(ChildProcess &cp, int fd_out);

ExitStatus wait_child(pid_t pid);

