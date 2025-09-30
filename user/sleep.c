#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: sleep <ticks>\n");
    exit(1);
  }

  int ticks = atoi(argv[1]);
  if (ticks <= 0) {
    fprintf(2, "sleep: ticks must be > 0\n");
    exit(1);
  }

  // pause is the syscall provided by xv6
  if (pause(ticks) < 0) {
    fprintf(2, "sleep: pause system call failed\n");
    exit(1);
  }

  exit(0);
}

