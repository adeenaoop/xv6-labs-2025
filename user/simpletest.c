#include "kernel/types.h"
#include "user/user.h"

int main() {
    printf("Simple test - PID: %d\n", getpid());
    
    // Create child that runs CPU intensive
    if(fork() == 0) {
        printf("Child starting (PID: %d)\n", getpid());
        for(int i=0;i<10000000;i++); // CPU work
        printf("Child done\n");
        exit(0);
    }
    
    // Parent does I/O
    for(int i=0;i<5;i++) {
        sleep(10);
        printf("Parent tick %d\n", i);
    }
    
    wait(0);
    printf("Test complete\n");
    exit(0);
}
