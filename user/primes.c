#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


#define INT_SIZE (sizeof(int))
#define MAX_LIMIT  35
#define READ_END 0
#define WRITE_END 1


int
main(int argc, char *argv[])
{
    int fds1[2], fds2[2], n, p, xstatus, pid;
    int *left_fds = fds1;  // 左通道
    int *right_fds = fds2; // 右通道
    if (pipe(left_fds) != 0 || pipe(right_fds) != 0) {
        printf("pipe() failed\n");
        exit(1);
    }
    // feed numbers
    for (int i = 2; i < MAX_LIMIT; ++i) {
        write(left_fds[WRITE_END], &i, INT_SIZE);
    }
    close(left_fds[WRITE_END]);
    int first_loop = 1;
    while (1) {
        int is_success = read(left_fds[READ_END], &n, INT_SIZE);  // read from left
        if (is_success == 0) {   // if read end
            close(left_fds[READ_END]);
            close(right_fds[READ_END]);
            close(right_fds[WRITE_END]);
            if (first_loop != 1) {
                wait(&xstatus);
                exit(xstatus);
            } else {
                exit(0);
            }
        }
        // 如果是第一次进入 Loop，则需要打印 prime 并创建一个子进程
        if (first_loop == 1) {
            pid = fork();
            if (pid == 0) {  // child proc
                first_loop = 1;
                close(left_fds[READ_END]);
                close(left_fds[WRITE_END]);
                int *temp = left_fds;
                left_fds = right_fds;
                right_fds = temp;
                if (pipe(right_fds) != 0) {
                    printf("pipe() failed\n");
                    exit(1);
                }
                close(left_fds[WRITE_END]);
                continue;
            } else if (pid > 0) {  // parent proc
                first_loop = 0;
                p = n;
                printf("prime %d\n", p);
                continue;
            } else {
                printf("fork() failed\n");
                exit(1);
            }
        } else {
            if ((n % p) != 0) {
                write(right_fds[WRITE_END], &n, INT_SIZE);
            }
        }
    }
}

