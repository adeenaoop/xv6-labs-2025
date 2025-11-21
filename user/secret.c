#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: secret <secret-string>\n");
    exit(1);
  }
  
  // The secret is stored in this string
  char *secret = argv[1];
  
  // Secret stays in memory until process exits
  printf("Secret stored: %s\n", secret);
  
  exit(0);
}
