#include "sys/error.h"
#include <cstdio>   
#include <cstdlib>  

[[noreturn]] void die(const char *msg) 
{
  std::perror(msg);
  std::exit(1);
}

