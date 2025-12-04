#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  for(int i = 0; i < 20; i++){
    sleep(1);
    printf("io tick %d\n", i);
  }
  exit(0);
}

