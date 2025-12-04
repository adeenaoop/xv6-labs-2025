#include "kernel/types.h"
#include "user/user.h"

struct uprocinfo {
  int pid;
  int state;
  int cur_q;
  int qtick;
  int total_ticks;
  char name[16];
};

int main() {
    struct uprocinfo procs[64];
    int count = getprocinfo(procs);
    
    printf("Total processes: %d\n", count);
    
    for(int i = 0; i < count; i++) {
        printf("[%d] PID=%d, state=%d, cur_q=%d\n", 
               i, procs[i].pid, procs[i].state, procs[i].cur_q);
    }
    
    exit(0);
}
