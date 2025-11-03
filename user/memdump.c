#include "kernel/types.h"
#include "user/user.h"

void memdump(char *fmt, char *data) {
  for (int i = 0; fmt[i] != '\0'; i++) {
    switch (fmt[i]) {
    case 'i': {
      int val;
      memmove(&val, data, sizeof(int));
      printf("%d\n", val);
      data += 4;
      break;
    }
    case 'p': {
      uint64 val;
      memmove(&val, data, sizeof(uint64));
      printf("%lx\n", val);
      data += 8;
      break;
    }
    case 'h': {
      short val;
      memmove(&val, data, sizeof(short));
      printf("%d\n", val);
      data += 2;
      break;
    }
    case 'c': {
      char val;
      memmove(&val, data, sizeof(char));
      printf("%c\n", val);
      data += 1;
      break;
    }
    case 's': {
      uint64 ptr;
      memmove(&ptr, data, sizeof(uint64));
      if ((char *)ptr != 0)
        printf("%s\n", (char *)ptr);
      data += 8;
      break;
    }
    case 'S': {
      printf("%s\n", data);
      while (*data != '\0')
        data++;
      data++;
      break;
    }
    default:
      printf("Unknown format: %c\n", fmt[i]);
      return;
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc == 1) {
    printf("Example 1:\n");
    int a[2] = {61810, 2025};
    memdump("ii", (char *)a);

    printf("Example 2:\n");
    char *s1 = "a string";
    memdump("S", s1);

    printf("Example 3:\n");
    char *s2 = "another";
    memdump("S", s2);

    printf("Example 4:\n");
    struct __attribute__((packed)) {
      char c;
      int i;
      short h;
      char d;
      char *s;
    } ex4 = {'B', 1819438967, 100, 'z', "xyzzy"};
    memdump("cihcs", (char *)&ex4);

    printf("Example 5:\n");
    char *words[] = {"hello", "w", "o", "r", "l", "d"};
    for (int i = 0; i < 6; i++)
      memdump("S", words[i]);

  } else {
    char buf[512];
    int n = read(0, buf, sizeof(buf));
    if (n > 0)
      memdump(argv[1], buf);
  }
  exit(0);
}

