
#pragma once

#include <cstddef>
// #include <cstdint>
#include <sys/types.h>

ssize_t write_all(int fd, const void *buf, size_t n);

void write_file_to_fd(const char *path, int out_fd);

