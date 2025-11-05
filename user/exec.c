#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

void
search(char *dirpath, char *target, int runexec, char *execargs[], int execargc)
{
  char pathbuf[512], *ptr;
  int fd;
  struct dirent entry;
  struct stat st;

  if ((fd = open(dirpath, 0)) < 0) {
    fprintf(2, "search: cannot open %s\n", dirpath);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "search: cannot stat %s\n", dirpath);
    close(fd);
    return;
  }

  switch (st.type) {
  case T_FILE:
    if (strlen(dirpath) >= strlen(target) &&
        strcmp(dirpath + strlen(dirpath) - strlen(target), target) == 0) {
      if (runexec) {
        // construct argv
        char *argv[MAXARG];
        if (execargc + 2 > MAXARG) {
          fprintf(2, "too many args for -exec\n");
          return;
        }
        for (int i = 0; i < execargc; i++)
          argv[i] = execargs[i];
        argv[execargc] = dirpath;
        argv[execargc + 1] = 0;

        int pid = fork();
        if (pid < 0) {
          fprintf(2, "fork failed\n");
          exit(1);
        } else if (pid == 0) {
          exec(argv[0], argv);
          fprintf(2, "exec %s failed\n", argv[0]);
          exit(1);
        } else {
          wait(0);
        }
      } else {
        printf("%s\n", dirpath);
      }
    }
    break;

  case T_DIR:
    if (strlen(dirpath) + 1 + DIRSIZ + 1 > sizeof pathbuf) {
      printf("search: path too long\n");
      break;
    }
    strcpy(pathbuf, dirpath);
    ptr = pathbuf + strlen(pathbuf);
    *ptr++ = '/';
    while (read(fd, &entry, sizeof(entry)) == sizeof(entry)) {
      if (entry.inum == 0)
        continue;
      if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0)
        continue;

      int nlen = 0;
      while (nlen < DIRSIZ && entry.name[nlen] != '\0') {
        nlen++;
      }
      memmove(ptr, entry.name, nlen);
      ptr[nlen] = 0;

      search(pathbuf, target, runexec, execargs, execargc);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(2, "Usage: search <path> <filename> [-exec command ...]\n");
    exit(1);
  }

  int exec_enabled = 0;
  char *execargs[MAXARG];
  int execargc = 0;

  // look for "-exec"
  for (int i = 3; i < argc; i++) {
    if (strcmp(argv[i], "-exec") == 0) {
      exec_enabled = 1;
      for (int j = i + 1; j < argc; j++) {
        execargs[execargc++] = argv[j];
      }
      break;
    }
  }

  search(argv[1], argv[2], exec_enabled, execargs, execargc);
  exit(0);
}
