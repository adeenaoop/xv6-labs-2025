#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"   
#include "user/user.h" 

static int has_exec;
static char *execv_base[MAXARG];
static int execv_basec;

static void run_exec_on(const char *filepath)
{
  // Build argv = execv_base... + filepath + NULL
  char *argv[MAXARG];
  int ac = 0;

  for (int i = 0; i < execv_basec && ac < MAXARG - 1; i++)
    argv[ac++] = execv_base[i];

  if (ac < MAXARG - 1)
    argv[ac++] = (char *)filepath;   // append matched path as final arg
  argv[ac] = 0;

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "find: fork failed\n");
    return;
  }
  if (pid == 0) {
    // child: exec
    exec(argv[0], argv);
    // if exec returns error
    fprintf(2, "find: exec %s failed\n", argv[0]);
    exit(1);
  }
  //wait for child
  wait(0);
}

static void find(const char *path, const char *target)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0) { 
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type == T_FILE) {
    // compare basename(path) with target
    const char *base = path;
    for (const char *p = path; *p; p++) if (*p == '/') base = p + 1;

    if (strcmp((char *)base, (char *)target) == 0) {
      if (has_exec) {
        run_exec_on(path);
      } else {
        printf("%s\n", path);
      }
    }
  } else if (st.type == T_DIR) {
    // iterate directory
    char buf[512];
    int n = strlen((char *)path);
    if (n + 1 + DIRSIZ + 1 > sizeof(buf)) {
      fprintf(2, "find: path too long: %s\n", path);
      close(fd);
      return;
    }

    struct dirent de;
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0) continue;

      char name[DIRSIZ + 1];
      memmove(name, de.name, DIRSIZ);
      name[DIRSIZ] = 0;

      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        continue;

      // build child path
      strcpy(buf, (char *)path);
      buf[n] = '/';
      buf[n + 1] = '\0';
      strcpy(buf + n + 1, name);

      find(buf, target);
    }
  }

  close(fd);
}


int main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(2, "usage: find <start-path> <name> [-exec <cmd> [args...]]\n");
    exit(1);
  }

  has_exec = 0;
  execv_basec = 0;

  if (argc >= 4 && strcmp(argv[3], "-exec") == 0) {
    has_exec = 1;
    if (argc < 5) {
      fprintf(2, "find: -exec requires a command\n");
      exit(1);
    }

    for (int i = 4; i < argc && execv_basec < MAXARG - 1; i++) {
      execv_base[execv_basec++] = argv[i];
    }
    execv_base[execv_basec] = 0;
  }

  find(argv[1], argv[2]);
  exit(0);
}
