#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define ERR_EXIT(s) { perror(s); exit(1); }
#define BUF_SIZE 32
#define OFFSET 0x0 //0x30

typedef struct {
    int id, score, rank;
} Player;

void init();
int dequeue();
void enqueue(int host_id);
void generate_combinations(int pos);
int find_host(int key);
void calculate_score();
int cmp_score(const void *pa, const void *pb){ return ((Player*)pb)->score - ((Player*)pa)->score; }
int cmp_id(const void *pa, const void *pb){ return ((Player*)pa)->id - ((Player*)pb)->id; }

int num_host, num_player;
int random_key[11];  // random key which assign to each root_host (random_key[host_id] = key)
Player players[15];

// queue to store idle root_host
char idle_queue[10];
int size, head, end;

FILE *fifo_fd[11];  // fifo file descriptor list

// generate combinations over players
char combinations[3004][8];
int num_filled, num_sent, num_completed;

// tmp variable
char buf[BUF_SIZE];

int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "Usage: %s [host_num] [player_num]\n", argv[0]);
        exit(1);
    }
    num_host = atoi(argv[1]);
    num_player = atoi(argv[2]);

    // make FIFO for communication with root_host
    if (mkfifo("./Host.FIFO", 0600) < 0) ERR_EXIT("mkfifo");

    for (int i=1; i<=num_host; ++i){
        sprintf(buf, "./Host%d.FIFO", i);
        if (mkfifo(buf, 0600) < 0) ERR_EXIT("mkfifo");
    }
    
    // fork and exec root_host
    char id_buf[4];
    char key_buf[16];
    int key, pid;
    srand(time(NULL));
    for (int i=1; i<=num_host; ++i){
        sprintf(id_buf, "%d", i);
        key = rand() % (2<<15);
        sprintf(key_buf, "%d", key);
        random_key[i] = key;

        if ((pid = fork()) == 0){
            if (execlp("./host", "./host", id_buf, key_buf, "0", (char *)NULL) < 0)  // run root_host
                ERR_EXIT("execlp");
        }
    }

    fifo_fd[0] = fopen("./Host.FIFO", "r");
    for (int i=1; i<=num_host; ++i){
        sprintf(buf, "./Host%d.FIFO", i);
        fifo_fd[i] = fopen(buf, "w");
    }

    // init idle root_hosts and 
    init();

    // generate combinations C(n, 8) competitions
    generate_combinations(0);

    // compute all the cometition and send unfinished competition
    int host_id;
    while (num_completed < num_filled){
        // 4 bytes random_key
        // 1+1 bytes (player_id + ranking)
        // 7 times above
        // sum = 20 bytes
        fscanf(fifo_fd[0], "%d", &key);
        host_id = find_host(key);
        if (host_id < 0) ERR_EXIT("find_host");

        // calculate scores
        calculate_score(buf+4);

        // sned competition to 
        if (num_sent < num_filled){
            for (int i=0; i<7; ++i)
                fprintf(fifo_fd[host_id], "%d ", combinations[num_sent][i]);
            fprintf(fifo_fd[host_id], "%d\n", combinations[num_sent++][7]);
            fflush(fifo_fd[host_id]);
        }
    }

    // update ranking according to scores
    qsort((void *)(players+1), num_player, sizeof(players[0]), cmp_score);
    for (int i=2; i<=num_player; ++i){
        if (players[i].score < players[i-1].score)
            players[i].rank = players[i-1].rank+1;
        else players[i].rank = players[i-1].rank;
    }
    qsort((void *)(players+1), num_player, sizeof(players[0]), cmp_id);

    // output the player_id and ranking
    for (int i=1; i<=num_player; ++i){
        printf("%d %d\n", players[i].id, players[i].rank);
    }

    // terminate all root_host
    memset(buf, -1, 8);
    for (int i=1; i<=num_host; ++i){
        fprintf(fifo_fd[i], "-1 -1 -1 -1 -1 -1 -1 -1\n");
        fflush(fifo_fd[i]);
    }

    // dispose child bodies
    for (int i=1; i<=num_host; ++i){
        wait(NULL);
    }
    
    // unlink FIFO previously build
    for (int i=1; i<=num_host; ++i){
        sprintf(buf, "Host%d.FIFO", i);
        fclose(fifo_fd[i]);
        if (unlink(buf) < 0) ERR_EXIT("unlink");

    }
    fclose(fifo_fd[0]);
    if (unlink("Host.FIFO") < 0) ERR_EXIT("unlink");

    return 0;
}

void init(){
    for (int i=1; i<=num_host; ++i){
        idle_queue[i-1] = i & 0xff;
    }
    end = num_host;
    size = num_host;
    for (int i=1; i<=num_player; ++i){
        players[i].id = i;
        players[i].rank = 1;
        players[i].score = 0;
    }
    num_filled = num_sent = num_completed = 0;
}

int dequeue(){
    int head_value = (int)idle_queue[head];
    head = (head+1) % num_host;
    size--;
    return head_value;
}

void enqueue(int host_id){
    idle_queue[end] = host_id & 0xff;
    end = (end+1) % num_host;
    size++;
}

void generate_combinations(int pos){
    // This function generate all combinations of player and sent them to competitions,
    // if there remain idle hosts.
    if (pos == 0){
        //printf("pos %d: renge [%d, %d]\n", pos, 1, num_player-7);
        for (int id=1; id<=num_player-7; ++id){
            combinations[num_filled][pos] = (id+OFFSET) & 0xff;
            generate_combinations(pos+1);
        }
    }
    else if (pos < 8){
        int left_id = combinations[num_filled][pos-1]-OFFSET;
        //printf("pos %d: renge [%d, %d]\n", pos, left_id+1, num_player-7+pos);
        for (int id=left_id+1; id<=num_player-7+pos; ++id){
            combinations[num_filled][pos] = (id+OFFSET) & 0xff;
            generate_combinations(pos+1);
        }
    }
    else {  // pos == 8
        memcpy(combinations[num_filled+1], combinations[num_filled], 8);
        if (size > 0){
            int idle_id = dequeue();
            for (int i=0; i<7; ++i)
                fprintf(fifo_fd[idle_id], "%d ", combinations[num_sent][i]);
            fprintf(fifo_fd[idle_id], "%d\n", combinations[num_sent++][7]);
            fflush(fifo_fd[idle_id]);
        }
        num_filled++;
    }
}

int find_host(int key){
    for (int id=1; id<=num_host; ++id)
        if (random_key[id] == key)
            return id;
    return -1;
}

void calculate_score(){
    int player_id, rank;
    for (int i=0; i<8; ++i){
        fscanf(fifo_fd[0], "%d%d", &player_id, &rank);
        players[player_id].score += 8-rank;
    }
    num_completed++;
}
