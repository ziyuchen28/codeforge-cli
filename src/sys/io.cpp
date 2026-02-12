
#include "sys/io.h"
#include "sys/fd.h"
#include "sys/error.h"
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h> 


ssize_t write_all(int fd, const void *buf, size_t n) 
{
    const uint8_t *p = static_cast<const uint8_t *>(buf);
    size_t off = 0;
    while (off < n) {
        ssize_t rc = write(fd, p + off, n - off);
        if (rc < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += static_cast<size_t>(rc);
    }
    return static_cast<ssize_t>(off);
}


void write_file_to_fd(const char *path, int out_fd) 
{
    int in = open(path, O_RDONLY);
    if (in < 0) die("open(prompt)");
    Fd in_file;
    in_file.reset(in);

    uint8_t buf[64 * 1024];
    for (;;) {
        ssize_t r = read(in, buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR) continue;
            close(in);
            die("read(prompt)");
        }
        if (r == 0) break;

        if (write_all(out_fd, buf, static_cast<size_t>(r)) < 0) {
            if (errno == EPIPE) break; // TODO: should we crash? 
            close(in);
            die("write(child_stdin)");
        }
    }
}

