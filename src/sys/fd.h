#pragma once

void set_cloexec(int fd);

class Fd 
{
public:
  Fd() = default;
  explicit Fd(int fd) : fd_(fd) {}
 
  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;

  Fd(Fd &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
  Fd& operator=(Fd &&other) noexcept 
  {
    if (this != &other) {
      close();
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  ~Fd();

  int get() const { return fd_; }
  explicit operator bool() const { return fd_ >= 0; }

  int release() {
    int tmp = fd_;
    fd_ = -1;
    return tmp;
  }

  void close();
  void reset(int fd);

private:
  int fd_ = -1;
};


struct Pipe 
{
  Fd r;
  Fd w;
  static Pipe create(); 
};
