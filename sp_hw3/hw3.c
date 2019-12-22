#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/types.h>
#include "scheduler.h"

#define SIGUSR3 SIGWINCH

void init();
// initialize the link list of FCB

void sig_handler1(int sig);
void sig_handler2(int sig);
void sig_handler3(int sig);
// signal handler for SIGUSR1 SIGUSR2, SIGUSR3

void enqueue(int id);
void dequeue();

int idx;
char arr[10000];
jmp_buf SCHEDULER, MAIN;

FCB_ptr Current, Head;
int P, Q, complete[4], task, wtf;
int Queue[4], Qhead, Qend, Qlen;
volatile int mutex;
char buf[1] = {0x1};
sigset_t newmask, oldmask;

int main(int argc, char* argv[]){
    if (argc != 5){
        fprintf(stderr, "Usage: %s [P][Q][task][]\n", argv[0]);
    }
    P = atoi(argv[1]); Q = atoi(argv[2]);
    task = atoi(argv[3]); wtf = atoi(argv[4]);
    // TODO: task2 didn't implement mutex lock

    if (task == 3){
        signal(SIGUSR1, sig_handler1);
        signal(SIGUSR2, sig_handler2);
        signal(SIGUSR3, sig_handler3);
        sigemptyset(&newmask);
        sigaddset(&newmask, SIGUSR1);
        sigaddset(&newmask, SIGUSR2);
        sigaddset(&newmask, SIGUSR3);
    }
    
    init();
    if (setjmp(MAIN) == 0){
        funct_5(1);
    }
    else Scheduler();
    return 0;
}

void init(){
    Qhead = Qend = 0;
    Head = (FCB_ptr) malloc(sizeof(FCB)*4);
    Current = Head;
    for (int i=1; i<4; ++i){
        Current->Next = Current+1;
        Current->Next->Previous = Current;
        Current->Name = i;
        Current = Current->Next;
    }
    Current->Next = Head;
    Head->Previous = Current;
    Current->Name = 4;
    Current = Head;
}

void funct_1(int name){
    int jmpVal = setjmp(Current->Environment);
    if (jmpVal == 0){
        Current = Current->Next;
        funct_5(2);
    }
    if (task == 1){
        for (int i=0; i<P; ++i){
            for (int j=0; j<Q; ++j){
                #ifndef DEBUG
                sleep(1);
                #endif
                arr[idx++] = '1';
            }
        }
    }
    else if (task == 2){
        for (int i=0; i<P; ++i){
            if (i == wtf) {
                if (setjmp(Current->Environment) == 0){
                    mutex = 0;
                    longjmp(SCHEDULER, 2);
                }
            }
            if (mutex == 0 || mutex == 1){
                mutex = 1;
                for (int j=0; j<Q; ++j){
                    #ifndef DEBUG
                    sleep(1);
                    #endif
                    arr[idx++] = '1';
                }
                mutex = 0;
            }
            else {
                longjmp(SCHEDULER, 2);
            }
        }
    }
    else {  // task == 3
        if (mutex == 0 || mutex == 1){
            if (Queue[Qhead] == 1){
                fprintf(stderr, "Release 1 from queue\n");
                dequeue();
            }
            mutex = 1;
            while (P-complete[0] > 0){
                sigprocmask(SIG_BLOCK, &newmask, &oldmask);
                for (int j=0; j<Q; ++j){
                    sleep(1);
                    arr[idx++] = '1';
                }
                complete[0]++;
                sigprocmask(SIG_UNBLOCK, &newmask, NULL);
            }
            mutex = 0;
        }
        else {
            enqueue(1);
            longjmp(SCHEDULER, 3);
        }
    }
    longjmp(SCHEDULER, -2);
}
void funct_2(int name){
    int jmpVal = setjmp(Current->Environment);
    if (jmpVal == 0){
        Current = Current->Next;
        funct_5(3);
    }
    if (task == 1){
        for (int i=0; i<P; ++i){
            for (int j=0; j<Q; ++j){
                #ifndef DEBUG
                sleep(1);
                #endif
                arr[idx++] = '2';
            }
        }
    }
    else if (task == 2){
        for (int i=0; i<P; ++i){
            if (i == wtf) {
                if (setjmp(Current->Environment) == 0){
                    mutex = 0;
                    longjmp(SCHEDULER, 2);
                }
            }
            if (mutex == 0 || mutex == 2){
                mutex = 2;
                for (int j=0; j<Q; ++j){
                    #ifndef DEBUG
                    sleep(1);
                    #endif
                    arr[idx++] = '2';
                }
                mutex = 0;
            }
            else {
                longjmp(SCHEDULER, 2);
            }
        }
    }
    else {  // task == 3
        if (mutex == 0 || mutex == 2){
            if (Queue[Qhead] == 2){
                fprintf(stderr, "Release 2 from queue\n");
                dequeue();
            }
            mutex = 2;
            while (P-complete[1] > 0){
                sigprocmask(SIG_BLOCK, &newmask, &oldmask);
                for (int j=0; j<Q; ++j){
                    sleep(1);
                    arr[idx++] = '2';
                }
                complete[1]++;
                sigprocmask(SIG_UNBLOCK, &newmask, NULL);
            }
            mutex = 0;
        }
        else {
            enqueue(2);
            longjmp(SCHEDULER, 3);
        }
    }
    longjmp(SCHEDULER, -2);
}
void funct_3(int name){
    int jmpVal = setjmp(Current->Environment);
    if (jmpVal == 0){
        Current = Current->Next;
        funct_5(4);
    }
    if (task == 1){
        for (int i=0; i<P; ++i){
            for (int j=0; j<Q; ++j){
                #ifndef DEBUG
                sleep(1);
                #endif
                arr[idx++] = '3';
            }
        }
    }
    else if (task == 2){
        for (int i=0; i<P; ++i){
            if (i == wtf) {
                if (setjmp(Current->Environment) == 0){
                    mutex = 0;
                    longjmp(SCHEDULER, 2);
                }
            }
            if (mutex == 0 || mutex == 3){
                mutex = 3;
                for (int j=0; j<Q; ++j){
                    #ifndef DEBUG
                    sleep(1);
                    #endif
                    arr[idx++] = '3';
                }
                mutex = 0;
            }
            else {
                longjmp(SCHEDULER, 2);
            }
        }
    }
    else {  // task == 3
        if (mutex == 0 || mutex == 3){
            if (Queue[Qhead] == 3){
                fprintf(stderr, "Release 3 from queue\n");
                dequeue();
            }
            mutex = 3;
            while (P-complete[2] > 0){
                sigprocmask(SIG_BLOCK, &newmask, &oldmask);
                for (int j=0; j<Q; ++j){
                    sleep(1);
                    arr[idx++] = '3';
                }
                complete[2]++;
                sigprocmask(SIG_UNBLOCK, &newmask, NULL);
            }
            mutex = 0;
        }
        else {
            enqueue(3);
            longjmp(SCHEDULER, 3);
        }
    }
    longjmp(SCHEDULER, -2);
}
void funct_4(int name){
    int jmpVal = setjmp(Current->Environment);
    if (jmpVal == 0){
        longjmp(MAIN, 1);
    }
    if (task == 1){
        for (int i=0; i<P; ++i){
            for (int j=0; j<Q; ++j){
                #ifndef DEBUG
                sleep(1);
                #endif
                arr[idx++] = '4';
            }
        }
    }
    else if (task == 2){
       for (int i=0; i<P; ++i){
            if (i == wtf) {
                if (setjmp(Current->Environment) == 0){
                    mutex = 0;
                    longjmp(SCHEDULER, 2);
                }
            }
            if (mutex == 0 || mutex == 4){
                mutex = 4;
                for (int j=0; j<Q; ++j){
                    #ifndef DEBUG
                    sleep(1);
                    #endif
                    arr[idx++] = '4';
                }
                mutex = 0;
            }
            else {
                longjmp(SCHEDULER, 2);
            }
        }
    }
    else {  // task == 3
        if (mutex == 0 || mutex == 4){
            if (Queue[Qhead] == 4){
                fprintf(stderr, "Release 4 from queue\n");
                dequeue();
            }
            mutex = 4;
            while (P-complete[3] > 0){
                sigprocmask(SIG_BLOCK, &newmask, &oldmask);
                for (int j=0; j<Q; ++j){
                    sleep(1);
                    arr[idx++] = '4';
                }
                complete[3]++;
                sigprocmask(SIG_UNBLOCK, &newmask, NULL);
            }
            mutex = 0;
        }
        else {
            enqueue(4);
            longjmp(SCHEDULER, 3);
        }
    }
    longjmp(SCHEDULER, -2);
}

void funct_5(int name){  // dummy function
    int a[10000];
    switch(name){
        case 1: funct_1(name); break;
        case 2: funct_2(name); break;
        case 3: funct_3(name); break;
        case 4: funct_4(name); break;
        default: exit(1);
    }
}

void sig_handler1(int sig){
    fprintf(stderr, "Receive SIGUSR1\n");
    write(STDOUT_FILENO, buf, 1);
    longjmp(SCHEDULER, 1);
}
void sig_handler2(int sig){
    fprintf(stderr, "Receive SIGUSR2\n");
    write(STDOUT_FILENO, buf, 1);
    mutex = 0;
    longjmp(SCHEDULER, 2);
}
void sig_handler3(int sig){
    fprintf(stderr, "Receive SIGUSR3\n");
    write(STDOUT_FILENO, buf, 1);
    for (int i=0; i<Qlen-1; ++i){
        printf("%d ", Queue[Qhead+i]);
    } printf("%d\n", Queue[Qend-1]);
    fflush(stdout);
    Current = Current->Previous;
    longjmp(SCHEDULER, 3);
}

void enqueue(int id){
    fprintf(stderr, "Unable to get lock, put funct_%d in the queue\n", id);
    Queue[Qend] = id;
    Qend = (Qend + 1) % 4;
    Qlen++;
}
void dequeue(){
    Queue[Qhead] = 0;
    Qhead = (Qhead + 1) % 4;
    Qlen--;
}
