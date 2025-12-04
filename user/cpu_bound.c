#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i, j;
  for(i = 0; i < 500000; i++){
    for(j = 0; j < 1000; j++){
      asm volatile("nop");
    }
    if(i % 10000 == 0) printf(".");
  }
  printf("\nCPU-bound done\n");
  exit(0);
}

