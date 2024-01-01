#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
    int fds[2], pid, n, xstatus;
    enum { BYTE_SZ=1 };
    if (pipe(fds) != 0) {
        printf("pipe() failed\n");
        exit(1);
    }
    pid = fork();
    if (pid > 0) {  // parent proc
        char parent_buf[BYTE_SZ];
        if (write(fds[1], "x", BYTE_SZ) != BYTE_SZ) {
            printf("pingpong oops 1\n");
            exit(1);
        }
        wait(&xstatus);
        if (xstatus != 0) {
            exit(xstatus);
        }
        while ((n = read(fds[0], parent_buf, BYTE_SZ)) > 0) {
            if (parent_buf[0] != 'x') {
                printf("pingpong oops 2\n");
                exit(1);
            }
            printf("%d: received pong\n", getpid());
            close(fds[0]);
            close(fds[1]);
            exit(0);
        }
    } else if (pid == 0) {  // child proc
        char child_buf[BYTE_SZ];
        while ((n = read(fds[0], child_buf, BYTE_SZ)) > 0) {
            if (child_buf[0] != 'x') {
                printf("pingpong oops 2\n");
                exit(1);
            }
            printf("%d: received ping\n", getpid());
            if (write(fds[1], "x", BYTE_SZ) != BYTE_SZ) {
                printf("pingpong oops 2\n");
                exit(1);
            }
            close(fds[0]);
            close(fds[1]);
            exit(0);
        }
    } else {
        printf("fork() failed\n");
        exit(1);
    }
    exit(0);
}