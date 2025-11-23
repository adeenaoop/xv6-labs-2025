#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    unsigned int i = 0x00646c72;
    printf("H%x Wo%s\n", 57616, (char *) &i);   
    exit(0);
}
