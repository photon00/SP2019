#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SIGUSR3 SIGWINCH

int pipe_fd[2];
int P, Q, R, num_bytes;
int signals[10];
pid_t child_pid;
char buf[512];

int main(int argc, char* argv[]){
    char P_chr[8], Q_chr[8];
    pipe(pipe_fd);
    scanf("%d%d", &P, &Q);
    if ((child_pid = fork()) == 0){
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[0]);
        sprintf(P_chr, "%d", P);
        sprintf(Q_chr, "%d", Q);
        if (execlp("./hw3", "./hw3", P_chr, Q_chr, "3", "0", NULL) < 0){
            perror("exec");
            exit(1);
        }
    }
    close(pipe_fd[1]);
    scanf("%d", &R);
    for (int i=0; i<R; ++i){
        scanf("%d", &signals[i]);
    }

    for (int i=0; i<R; ++i){
        sleep(5);
        fprintf(stderr, "[SEND] SIGUSR%d\n", signals[i]);
        switch(signals[i]){
            case 1: kill(child_pid, SIGUSR1); break;
            case 2: kill(child_pid, SIGUSR2); break;
            case 3: kill(child_pid, SIGUSR3); break;
            default: fprintf(stderr, "invalid signal type %d\n", signals[i]); exit(1);
        }
        read(pipe_fd[0], buf, 1);
        if (signals[i] == 3){
            num_bytes = read(pipe_fd[0], buf, 512);
            write(STDOUT_FILENO, buf, num_bytes);
        }
    }

    num_bytes = read(pipe_fd[0], buf, 512);
    write(STDOUT_FILENO, buf, num_bytes);
    wait(NULL);
    close(pipe_fd[0]);
    return 0;
}