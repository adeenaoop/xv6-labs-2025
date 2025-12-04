#include "kernel/types.h"
#include "user/user.h"

int main() {
    printf("Infinite CPU process PID: %d\n", getpid());
    
    long counter = 0;
    while(1) {
        // Much heavier CPU load
        for(long i = 0; i < 1000000; i++) {
            asm volatile("nop");
        }
        counter++;
        if(counter % 100 == 0) {
            printf("CPU still running... (%ld iterations)\n", counter);
        }
    }
}
