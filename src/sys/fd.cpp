
#include "sys/fd.h"
#include "sys/error.h"
#include <fcntl.h>
#include <unistd.h>

void set_cloexec(int fd) 
{
  int flags = fcntl(fd, F_GETFD);
  if (flags < 0) die("fcntl(F_GETFD)");
  if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) die("fcntl(F_SETFD)");
}

Fd::~Fd() { close(); }

void Fd::close() 
{ 
    if (fd_ >= 0) { ::close(fd_); } 
}


void Fd::reset(int fd) 
{ 
    if (fd_ >= 0) { ::close(fd_); } 
    fd_ = fd;
}

Pipe Pipe::create() 
{
  int fds[2];
  if (pipe(fds) < 0) die("pipe");
  Pipe p{Fd(fds[0]), Fd(fds[1])};
  set_cloexec(p.r.get());
  set_cloexec(p.w.get());
  return p;
}

