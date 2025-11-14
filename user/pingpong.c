#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int p1[2]; // pipe from parent to child
  int p2[2]; // pipe from child to parent
  char buf[1]; // buffer for one byte
  int i;

  // Create pipes
  if (pipe(p1) < 0 || pipe(p2) < 0) {
    fprintf(2, "pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "fork failed\n");
    exit(1);
  }

  // ---- CHILD PROCESS ----
  if (pid == 0) {
    // Close unused ends
    close(p1[1]); // child doesn't write to p1
    close(p2[0]); // child doesn't read from p2

    for (i = 0; i < 10; i++) {
      // Wait for a byte from the parent
      if (read(p1[0], buf, 1) != 1) {
        fprintf(2, "child read error\n");
        exit(1);
      }
      printf("Child: received ping %d\n", i + 1);

      // Send a byte back to parent
      if (write(p2[1], buf, 1) != 1) {
        fprintf(2, "child write error\n");
        exit(1);
      }
      printf("Child: sent pong %d\n", i + 1);
    }

    // Close used pipe ends
    close(p1[0]);
    close(p2[1]);
    exit(0);
  }

  // ---- PARENT PROCESS ----
  else {
    // Close unused ends
    close(p1[0]); // parent doesn't read from p1
    close(p2[1]); // parent doesn't write to p2

    for (i = 0; i < 10; i++) {
      // Send a byte to the child
      buf[0] = 'x';
      if (write(p1[1], buf, 1) != 1) {
        fprintf(2, "parent write error\n");
        exit(1);
      }
      printf("Parent: sent ping %d\n", i + 1);

      // Wait for a byte from child
      if (read(p2[0], buf, 1) != 1) {
        fprintf(2, "parent read error\n");
        exit(1);
      }
      printf("Parent: received pong %d\n", i + 1);
    }

    // Close used pipe ends
    close(p1[1]);
    close(p2[0]);

    wait(0);
    exit(0);
  }
}

