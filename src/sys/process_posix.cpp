
#include "sys/process.h"
#include "sys/io.h"
#include "sys/error.h"
#include <cerrno>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILD 0


static std::vector<char*> build_exec_argv(const SpawnSpec &spec) 
{
  std::vector<char*> cargv;
  cargv.reserve(spec.argv.size() + 1);
  for (const auto &s : spec.argv) {
    cargv.push_back(const_cast<char*>(s.c_str()));
  }
  cargv.push_back(nullptr);
  return cargv;
}


ChildProcess spawn(const SpawnSpec &spec) 
{
  Pipe in  = Pipe::create(); // parent -> child stdin
  Pipe out = Pipe::create(); // child stdout -> parent
  Pipe err = Pipe::create(); // child stderr -> parent
                             
  std::vector<char*> cargv = build_exec_argv(spec);

  pid_t pid = fork();

  if (pid < 0) die("fork");

  if (pid == CHILD) {
    if (dup2(in.r.get(), STDIN_FILENO) < 0) _exit(127);
    if (dup2(out.w.get(), STDOUT_FILENO) < 0) _exit(127);
    if (dup2(err.w.get(), STDERR_FILENO) < 0) _exit(127);
    in.r.close();  
    in.w.close();
    out.r.close(); 
    out.w.close();
    err.r.close(); 
    err.w.close();
    execvp(spec.exe.c_str(), cargv.data());
    _exit(127);
  }

  ChildProcess cp;
  cp.pid = pid;
  cp.stdin_w  = std::move(in.w);
  cp.stdout_r = std::move(out.r);
  cp.stderr_r = std::move(err.r);
  in.r.close();
  out.w.close();
  err.w.close();
  return cp;
}


void stream_to_parent(ChildProcess &cp)
{
    stream_to_parent(cp, STDOUT_FILENO);
}


void stream_to_parent(ChildProcess &cp, int fd_out) 
{
  pollfd fds[2];
  fds[0].fd = cp.stdout_r.get(); 
  fds[0].events = POLLIN;
  fds[1].fd = cp.stderr_r.get(); 
  fds[1].events = POLLIN;

  bool out_open = true;
  bool err_open = true;
  uint8_t buf[64 * 1024];

  while (out_open || err_open) {
    fds[0].events = out_open ? POLLIN : 0;
    fds[1].events = err_open ? POLLIN : 0;

    int rc = poll(fds, 2, -1);
    if (rc < 0) {
      if (errno == EINTR) continue;
      die("poll");
    }

    if (out_open && (fds[0].revents & (POLLIN | POLLHUP))) {
      ssize_t r = read(cp.stdout_r.get(), buf, sizeof(buf));
      if (r < 0) {
        if (errno != EINTR) { 
            out_open = false; 
            cp.stdout_r.close(); 
        }
      } else if (r == 0) {
        out_open = false;
        cp.stdout_r.close();
      } else {
        if (write_all(fd_out, buf, static_cast<size_t>(r)) < 0) die("write(stdout)");
      }
    }

    if (err_open && (fds[1].revents & (POLLIN | POLLHUP))) {
      ssize_t r = read(cp.stderr_r.get(), buf, sizeof(buf));
      if (r < 0) {
        if (errno != EINTR) { err_open = false; cp.stderr_r.close(); }
      } else if (r == 0) {
        err_open = false;
        cp.stderr_r.close();
      } else {
        if (write_all(STDERR_FILENO, buf, static_cast<size_t>(r)) < 0) die("write(stderr)");
      }
    }
  }
}


ExitStatus wait_child(pid_t pid) 
{
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) die("waitpid");
  ExitStatus es;
  if (WIFEXITED(status)) {
    es.exited = true;
    es.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    es.signaled = true;
    es.term_signal = WTERMSIG(status);
  }
  return es;
}

