#include <stdio.h>
#include <stdlib.h>
#ifndef ROUNDS
    #define ROUNDS 1
#endif

int player_id, money, winner_id;

int main(int argc, char *argv[]){
    if (argc != 2){
        fprintf(stderr, "Usage: %s [player_id]", argv[0]);
        exit(1);
    }
    player_id = atoi(argv[1]);
    money = player_id * 100;
    for (int i=0; i<ROUNDS; ++i){
        printf("%d %d\n", player_id, money);
        fflush(stdout);
        if (i < ROUNDS-1) scanf("%d", &winner_id);
    }
    return 0;
}