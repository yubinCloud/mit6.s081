#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


/**
 * 获取一个 path 的名称，比如 `./a/b` 将返回 `b`
*/
char* fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = '\0';
  return buf;
}

void find(char *path, char const * const target) {
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
    case T_FILE: {
        char *fname = fmtname(path);
        if (strcmp(fname, target) == 0) {
            printf("%s\n", path);
        }
        break;
    }
    case T_DIR: {
        char buf[512], *p;
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0) {
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            char* dir_name = fmtname(buf);
            if (strcmp(dir_name, ".") != 0 && strcmp(dir_name, "..") != 0) {
                find(buf, target);
            }
        }
        break;
    }}
    close(fd);
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(2, "Usage: find path file\n");
        exit(1);
    }
    char *path = argv[1];
    char const *target = argv[2];
    find(path, target);

    exit(0);
}
