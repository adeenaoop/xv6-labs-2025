#include "kernel/types.h"
#include "kernel/stat.h"
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
    
    printf("PID\tNAME\t\tSTATE\tQUEUE\tQTICK\tTOTAL\n");
    
    for(int i = 0; i < count; i++) {
        // SAFE PRINTING - NO FORMAT STRINGS IN NAME FIELD
        printf("%d\t", procs[i].pid);
        
        // Print name character by character
        for(int j = 0; j < 16; j++) {
            char c = procs[i].name[j];
            if(c == 0) break;
            printf("%c", c);
        }
        
        printf("\t%d\t%d\t%d\t%d\n",
               procs[i].state,
               procs[i].cur_q,
               procs[i].qtick,
               procs[i].total_ticks);
    }
    exit(0);
}
