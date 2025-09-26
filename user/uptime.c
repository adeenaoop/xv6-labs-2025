#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int t = uptime();   // call system call
  printf("Uptime in ticks: %d\n", t);
  exit(0);
}

