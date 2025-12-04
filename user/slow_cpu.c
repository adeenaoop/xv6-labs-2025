#include "kernel/types.h"
#include "user/user.h"

int main() {
    printf("Slow CPU PID: %d\n", getpid());
    
    for(int outer = 0; outer < 100; outer++) {
        for(int i = 0; i < 1000000; i++) {
            asm volatile("nop");
        }
        if(outer % 10 == 0) {
            printf("Progress: %d/100\n", outer);
        }
    }
    printf("Slow CPU done\n");
    exit(0);
}
