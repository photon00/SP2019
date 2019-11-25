#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define ERR_EXIT(s) { perror(s); exit(1); }
#define BUF_SIZE 32
#ifndef ROUNDS
    #define ROUNDS 1
#endif
#define perr(args...) fprintf(stderr, args);

void send_rank();

int host_id, key, depth;
char buf[BUF_SIZE];

char RDFIFO_path[64];
FILE *RDFIFO_fd, *WRFIFO_fd;
int pipe_fd1[2][2];  // pipe_fdn[0]  parent <- child
int pipe_fd2[2][2];  // pipe_fdn[1]  parent -> child

FILE *fp1_r, *fp1_w;
FILE *fp2_r, *fp2_w;

int player_id[8];
int ranking[8];
int p1_id, p2_id, winner_id;
int p1_money, p2_money, winner_money;

int main(int argc, char *argv[]){
    host_id = atoi(argv[1]);
    key = atoi(argv[2]);
    depth = atoi(argv[3]);

    if (depth == 0){  // root_host
        // communication with bidding system usgin FIFO
        WRFIFO_fd = fopen("./Host.FIFO", "w");
        sprintf(RDFIFO_path, "./Host%d.FIFO", host_id);
        RDFIFO_fd = fopen(RDFIFO_path, "r");
        
        // generate child_host 1
        pipe(pipe_fd1[0]); pipe(pipe_fd1[1]);
        if (fork() == 0){
            dup2(pipe_fd1[0][1], STDOUT_FILENO);
            dup2(pipe_fd1[1][0], STDIN_FILENO);
            close(pipe_fd1[0][0]); close(pipe_fd1[0][1]);
            close(pipe_fd1[1][0]); close(pipe_fd1[1][1]);
            if (execlp("./host", "./host", argv[1], argv[2], "1", (char *)NULL) < 0)  // run root_host
                ERR_EXIT("execlp");
        }
        else {
            close(pipe_fd1[0][1]);
            close(pipe_fd1[1][0]);
            fp1_r = fdopen(pipe_fd1[0][0], "r");
            fp1_w = fdopen(pipe_fd1[1][1], "w");
        }

        // generate child_host 2
        pipe(pipe_fd2[0]); pipe(pipe_fd2[1]);
        if (fork() == 0){
            dup2(pipe_fd2[0][1], STDOUT_FILENO);
            dup2(pipe_fd2[1][0], STDIN_FILENO);
            close(pipe_fd2[0][0]); close(pipe_fd2[0][1]);
            close(pipe_fd2[1][0]); close(pipe_fd2[1][1]);
            if (execlp("./host", "./host", argv[1], argv[2], "1", (char *)NULL) < 0)  // run root_host
                ERR_EXIT("execlp");
        }
        else {
            close(pipe_fd2[0][1]);
            close(pipe_fd2[1][0]);
            fp2_r = fdopen(pipe_fd2[0][0], "r");
            fp2_w = fdopen(pipe_fd2[1][1], "w");
        }

        // read 8 player_ids from bidding system
        while (1){
            for (int i=0; i<8; ++i){
                fscanf(RDFIFO_fd, "%d", &player_id[i]);
            }
            if (player_id[0] < 0) break;
            #ifdef DEBUG
            perr("root receive ids: %d %d %d %d %d %d %d %d\n", player_id[0], player_id[1],
                player_id[2], player_id[3], player_id[4], player_id[5], player_id[6], player_id[7]);
            #endif
            
            // send player_ids to child_hosts
            // NOTE: without \n
            for (int i=0; i<4; ++i){
                fprintf(fp1_w, "%d ", player_id[i]);
                fprintf(fp2_w, "%d ", player_id[i+4]);
            }
            fflush(fp1_w); fflush(fp2_w);
            
            // receive player_id and money from child_host 10 times
            for (int i=0; i<ROUNDS; ++i){
                fscanf(fp1_r, "%d%d", &p1_id, &p1_money);
                fscanf(fp2_r, "%d%d", &p2_id, &p2_money);
                winner_id = (p1_money > p2_money)?p1_id:p2_id;
                #ifdef DEBUG
                perr("root gets %d:%d, %d:%d\n", p1_id, p1_money, p2_id, p2_money);
                #endif
                
                // send winner_id to childs
                fprintf(fp1_w, "%d\n", winner_id); fflush(fp1_w);
                fprintf(fp2_w, "%d\n", winner_id); fflush(fp2_w);
            }
            // update ranking
            for (int i=0; i<8; ++i){
                ranking[i] = 2;
                if (player_id[i] == winner_id)
                    ranking[i] = 1;
            }
            send_rank();
            #ifdef DEBUG
            perr("===========================\n");
            #endif
        }
        // send terminate action to child_host
        fprintf(fp1_w, "-1 -1 -1 -1\n"); fflush(fp1_w);
        fprintf(fp2_w, "-1 -1 -1 -1\n"); fflush(fp2_w);
    }

    else if (depth == 1){  // child host
        // generate leaf_host 1
        pipe(pipe_fd1[0]); pipe(pipe_fd1[1]);
        if (fork() == 0){
            dup2(pipe_fd1[0][1], STDOUT_FILENO);
            dup2(pipe_fd1[1][0], STDIN_FILENO);
            close(pipe_fd1[0][0]); close(pipe_fd1[0][1]);
            close(pipe_fd1[1][0]); close(pipe_fd1[1][1]);
            if (execlp("./host", "./host", argv[1], argv[2], "2", (char *)NULL) < 0)
                ERR_EXIT("execlp");
        }
        else {
            close(pipe_fd1[0][1]);
            close(pipe_fd1[1][0]);
            fp1_r = fdopen(pipe_fd1[0][0], "r");
            fp1_w = fdopen(pipe_fd1[1][1], "w");
        }

        // generate leaf_host 2
        pipe(pipe_fd2[0]); pipe(pipe_fd2[1]);
        if (fork() == 0){
            dup2(pipe_fd2[0][1], STDOUT_FILENO);
            dup2(pipe_fd2[1][0], STDIN_FILENO);
            close(pipe_fd2[0][0]); close(pipe_fd2[0][1]);
            close(pipe_fd2[1][0]); close(pipe_fd2[1][1]);
            if (execlp("./host", "./host", argv[1], argv[2], "2", (char *)NULL) < 0)
                ERR_EXIT("execlp");
        }
        else {
            close(pipe_fd2[0][1]);
            close(pipe_fd2[1][0]);
            fp2_r = fdopen(pipe_fd2[0][0], "r");
            fp2_w = fdopen(pipe_fd2[1][1], "w");
        }

        // read 4 player_id from root_host
        while (1){
            for (int i=0; i<4; ++i){
                scanf("%d", &player_id[i]);
            }
            if (player_id[0] < 0) break;
            #ifdef DEBUG
            perr("child receive ids: %d %d %d %d\n", player_id[0], player_id[1],
                player_id[2], player_id[3]);
            #endif
            
            // send player_ids to child_hosts
            // NOTE: without \n
            for (int i=0; i<2; ++i){
                fprintf(fp1_w, "%d ", player_id[i]);
                fprintf(fp2_w, "%d ", player_id[i+2]);
            }
            fflush(fp1_w); fflush(fp2_w);
            
            // 10 rounds for a competition
            for (int i=0; i<ROUNDS; ++i){
                fscanf(fp1_r, "%d%d", &p1_id, &p1_money);
                fscanf(fp2_r, "%d%d", &p2_id, &p2_money);
                winner_id = (p1_money > p2_money)?p1_id:p2_id;
                winner_money = (winner_id == p1_id)?p1_money:p2_money;
                #ifdef DEBUG
                perr("child gets %d:%d, %d:%d\n", p1_id, p1_money, p2_id, p2_money);
                #endif

                // send winner_id, money to root_host
                printf("%d %d\n", winner_id, winner_money);
                fflush(stdout);

                // read from root_host the winner_id of the round
                // and send to the child_host
                scanf("%d", &winner_id);
                fprintf(fp1_w, "%d\n", winner_id); fflush(fp1_w);
                fprintf(fp2_w, "%d\n", winner_id); fflush(fp2_w);
            }
        }
        // send terminate action to leaf_host
        fprintf(fp1_w, "-1 -1\n"); fflush(fp1_w);
        fprintf(fp2_w, "-1 -1\n"); fflush(fp2_w);
    }

    else { // leaf_host
        pipe(pipe_fd1[0]); pipe(pipe_fd1[1]);
        pipe(pipe_fd2[0]); pipe(pipe_fd2[1]);
        char id_buf[16];
        // receive player_id from child_host
        while (1){
            for (int i=0; i<2; ++i){
                scanf("%d", &player_id[i]);
            }
            if (player_id[0] < 0) break;
            #ifdef DEBUG
            perr("leaf receive ids: %d %d\n", player_id[0], player_id[1]);
            #endif
            
            // generate player 1
            if (fork() == 0){
                dup2(pipe_fd1[0][1], STDOUT_FILENO);
                dup2(pipe_fd1[1][0], STDIN_FILENO);
                close(pipe_fd1[0][0]); close(pipe_fd1[0][1]);
                close(pipe_fd1[1][0]); close(pipe_fd1[1][1]);
                sprintf(id_buf, "%d", player_id[0]);
                #ifdef DEBUG
                perr("EXEC: ./player %s\n", id_buf);
                #endif
                if (execlp("./player", "./player", id_buf, (char *)NULL) < 0)
                    ERR_EXIT("execlp");
            }
            else {
                fp1_r = fdopen(pipe_fd1[0][0], "r");
                fp1_w = fdopen(pipe_fd1[1][1], "w");
            }
            
            // generate player 2
            if (fork() == 0){
                dup2(pipe_fd2[0][1], STDOUT_FILENO);
                dup2(pipe_fd2[1][0], STDIN_FILENO);
                close(pipe_fd2[0][0]); close(pipe_fd2[0][1]);
                close(pipe_fd2[1][0]); close(pipe_fd2[1][1]);
                sprintf(id_buf, "%d", player_id[1]);
                #ifdef DEBUG
                perr("EXEC: ./player %s\n", id_buf);
                #endif
                if (execlp("./player", "./player", id_buf, (char *)NULL) < 0)
                    ERR_EXIT("execlp");
            }
            else {
                fp2_r = fdopen(pipe_fd2[0][0], "r");
                fp2_w = fdopen(pipe_fd2[1][1], "w");
            }
            
            for (int i=0; i<ROUNDS; ++i){
                fscanf(fp1_r, "%d%d", &p1_id, &p1_money);
                fscanf(fp2_r, "%d%d", &p2_id, &p2_money);
                winner_id = (p1_money > p2_money)?p1_id:p2_id;
                winner_money = (winner_id == p1_id)?p1_money:p2_money;
                #ifdef DEBUG
                perr("leaf gets %d:%d, %d:%d\n", p1_id, p1_money, p2_id, p2_money);
                #endif

                // send winner_id, money to child_host
                printf("%d %d\n", winner_id, winner_money);
                fflush(stdout);

                // read from child_host the winner_id of the round
                // and send to players
                scanf("%d", &winner_id);
                if (i < ROUNDS-1){
                    fprintf(fp1_w, "%d\n", winner_id); fflush(fp1_w);
                    fprintf(fp2_w, "%d\n", winner_id); fflush(fp2_w);
                }
            }    
            wait(NULL); wait(NULL);
        }
        close(pipe_fd1[0][0]); close(pipe_fd1[0][1]); 
        close(pipe_fd1[1][0]); close(pipe_fd1[1][1]);
        close(pipe_fd2[0][0]); close(pipe_fd2[0][1]);
        close(pipe_fd2[1][0]); close(pipe_fd2[1][1]);
    }

    if (depth < 2) { 
        fclose(fp1_r); fclose(fp1_w);
        fclose(fp2_r); fclose(fp2_w);
        wait(NULL); wait(NULL);
    }
    
    if (depth == 0){
        fclose(RDFIFO_fd);
        fclose(WRFIFO_fd);
    }
    return 0;
}

void send_rank(){
    int fd = fileno(WRFIFO_fd);
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;

    // acquire lock
    while (fcntl(fd, F_SETLK, &lock) < 0){;}
    
    fprintf(WRFIFO_fd, "%d\n", key);
    for (int i=0; i<8; ++i){
        fprintf(WRFIFO_fd, "%d %d\n", player_id[i], ranking[i]);
    }
    fflush(WRFIFO_fd);
    
    // release lock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
}