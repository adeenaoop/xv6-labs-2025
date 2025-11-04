#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: xargs command ...\n");
    exit(1);
  }

  char buf[512];
  int n = 0;
  char c;
  while(read(0, &c, 1) > 0){
    if(c == '\n'){
      buf[n] = 0;
      char *args[argc + 1];
      for(int i = 1; i < argc; i++){
        args[i - 1] = argv[i];
      }
      args[argc - 1] = buf;
      args[argc] = 0;

      if(fork() == 0){
        exec(argv[1], args);
        fprintf(2, "exec %s failed\n", argv[1]);
        exit(1);
      } else {
        wait(0);
      }
      n = 0;
    } else {
      buf[n++] = c;
    }
  }

  exit(0);
}

