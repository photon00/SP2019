#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/types.h>


#define ERR_EXIT(a) { perror(a); exit(1); }
#define DATABASE "./account_list"

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
	int item;  // use to identify current step for write request
    int wait_for_write;  // used by handle_read to know if the header is read or not.
    // handle write operation
    int open_fd, id;
    struct flock lock;
} request;

typedef struct{
    int id;
    int balance;
} Account;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";

#ifndef READ_SERVER
char write_hash[21];
#endif

// Forwards

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

static int handle_read(request* reqP);
// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error

static int read_account(int id);

#ifndef READ_SERVER
static int write_account(request* reqP);
static void int2bytes(char* p, int i);
#endif

int main(int argc, char** argv) {
    int i, ret;

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;

    fd_set master, readfds;  // for fd_set arguments that select() takes
    int current_maxfd = 0;  // for first argument that select() takes
    struct timeval tv;  // time interval for select() wait

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Get file descripter table size and initize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);


    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    // initialize arguments for select()
    FD_ZERO(&master);
    FD_ZERO(&readfds);
    FD_SET(svr.listen_fd, &master);
    if (svr.listen_fd > current_maxfd) current_maxfd = svr.listen_fd;
    tv.tv_sec = 100;
    tv.tv_usec = 0;

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    while (1) {
        // TODO: Add IO multiplexing
        readfds = master;
        ret = select(current_maxfd+1, &readfds, NULL, NULL, &tv);
        if (ret == -1){ ERR_EXIT("select()"); }
        if (ret == 0){
            fprintf(stderr, "select() timeout\n");
            continue;
        }
        else {
            // Check new connection
            for (i=0; i<=current_maxfd; ++i){
                if (FD_ISSET(i, &readfds)){
                    if (i == svr.listen_fd){
                        clilen = sizeof(cliaddr);
                        conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                        if (conn_fd < 0) {
                            if (errno == EINTR || errno == EAGAIN) continue;  // try again
                            if (errno == ENFILE) {
                                (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                                continue;
                            }
                            ERR_EXIT("accept")
                        }
                        requestP[conn_fd].conn_fd = conn_fd;
                        strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                        if (conn_fd > current_maxfd) current_maxfd = conn_fd;  // update current max fd on service
                        FD_SET(conn_fd, &master);

                        fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                    }
                    else {
                        ret = handle_read(&requestP[i]); // parse data from client to requestP[conn_fd].buf
                        if (ret < 0) {
                            fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                            continue;
                        }
#ifdef READ_SERVER
                        int id = atoi(requestP[i].buf);
                        int money = read_account(id);
                        if (money < 0) sprintf(buf, "This account is locked.\n");
                        else sprintf(buf, "%d %d\n", id, money);
                        
                        write(requestP[i].conn_fd, buf, strlen(buf));
                        close(requestP[i].conn_fd);
                        free_request(&requestP[i]);
                        FD_CLR(i, &master);
                    
#else  // write server
                        if (requestP[i].open_fd == 0){
                            int id = atoi(requestP[i].buf);
                            requestP[i].open_fd = open(DATABASE, O_RDWR);
                            requestP[i].lock = (struct flock){
                                .l_type=F_WRLCK,
                                .l_whence=SEEK_SET,
                                .l_start=(off_t)((id-1)*sizeof(Account)),
                                .l_len=sizeof(Account),
                                .l_pid=0
                            };
                            if (write_hash[id] != 0 || fcntl(requestP[i].open_fd, F_SETLK, &requestP[i].lock) < 0){
                                fprintf(stderr, "Fail to acquire write lock on id %d!!\n", id);
                                close(requestP[i].open_fd);
                                requestP[i].open_fd = 0;
                                sprintf(buf, "This account is locked.\n");
                                write(requestP[i].conn_fd, buf, strlen(buf));
                                close(requestP[i].conn_fd);
                                free_request(&requestP[i]);
                                FD_CLR(i, &master);
                            }
                            else {
                                requestP[i].id = id;
                                fprintf(stderr, "Acquire write lock on id %d\n", id);
                                sprintf(buf, "This account is modifiable.\n");
                                write(requestP[i].conn_fd, buf, strlen(buf));
                                write_hash[id] = 1;
                            }
                        }
                        else {
                            if (write_account(&requestP[i]) < 0){
                                sprintf(buf, "Operation failed.\n");
                                write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                            }
                            close(requestP[i].conn_fd);
                            free_request(&requestP[i]);
                            FD_CLR(i, &master);
                        }
#endif
                    }
                }
            }
        }
    }
    free(requestP);
    return 0;
}


// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void* e_malloc(size_t size);


static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->item = 0;
    reqP->wait_for_write = 0;
    reqP->open_fd = 0;
    reqP->id = 0;
}

static void free_request(request* reqP) {
    init_request(reqP);
}

// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error
static int handle_read(request* reqP) {
    int r;
    char buf[512];

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
	char* p1 = strstr(buf, "\015\012");
	int newline_len = 2;
	// be careful that in Windows, line ends with \015\012
	if (p1 == NULL) {
		p1 = strstr(buf, "\012");
		newline_len = 1;
		if (p1 == NULL) {
			ERR_EXIT("this really should not happen...");
		}
	}
	size_t len = p1 - buf + 1;
	memmove(reqP->buf, buf, len);
	reqP->buf[len - 1] = '\0';
	reqP->buf_len = len-1;
    return 1;
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }
}

static void* e_malloc(size_t size) {
    void* ptr;

    ptr = malloc(size);
    if (ptr == NULL) ERR_EXIT("out of memory");
    return ptr;
}

// return balance of an account if the file isn't locked, else -1
static int read_account(int id){
    char buf[8];
    struct flock lock = {
        .l_type=F_RDLCK,
        .l_whence=SEEK_SET,
        .l_start=(off_t)((id-1)*sizeof(Account)),
        .l_len=sizeof(Account),
        .l_pid=0
    };
    int fd = open(DATABASE, O_RDONLY);
    if (fcntl(fd, F_SETLK, &lock) < 0){
        fprintf(stderr, "Fail to acquire read lock on id %d!!\n", id);
        close(fd);
        return -1;
    }
    fprintf(stderr, "Acquire read lock on id %d\n", id);
    lseek(fd, (off_t)((id-1)*sizeof(Account)), SEEK_SET);
    read(fd, buf, 8);
    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &lock) < 0){
        ERR_EXIT("fcntl()");
    }
    close(fd);
    return ((Account*)buf)->balance;
}

#ifndef READ_SERVER
static int write_account(request* reqP){  // return -1 if operation is invalid
    char buf[8];
    char *space_ptr = strchr(reqP->buf, ' ');
    *space_ptr = '\0';

    lseek(reqP->open_fd, (off_t)((reqP->id-1)*sizeof(Account)), SEEK_SET);
    read(reqP->open_fd, buf, 8);
    int current_money = ((Account*)buf)->balance;
    lseek(reqP->open_fd, -8, SEEK_CUR);

    int error_flag = 0;
    if (strcmp("save", reqP->buf) == 0){
        int add_money = atoi(space_ptr+1);
        if (add_money < 0) error_flag = 1;
        else {
            int2bytes(buf+4, current_money + add_money);
            write(reqP->open_fd, buf, 8);
        }
    }
    else if (strcmp("withdraw", reqP->buf) == 0){
        int minus_money = atoi(space_ptr+1);
        if (minus_money < 0 || minus_money > current_money) error_flag = 1;
        else {
            int2bytes(buf+4, current_money - minus_money);
            write(reqP->open_fd, buf, 8);
        }
    }
    else if (strcmp("transfer", reqP->buf) == 0){
        char *space_ptr2 = strchr(++space_ptr, ' ');
        *space_ptr2 = '\0';
        int another_id = atoi(space_ptr);
        int transfer_money = atoi(space_ptr2+1);
        if (transfer_money > current_money || transfer_money < 0) error_flag = 1;
        else {
            int2bytes(buf+4, current_money - transfer_money);
            write(reqP->open_fd, buf, 8);
            struct flock lock2 = {
                .l_type=F_WRLCK,
                .l_whence=SEEK_SET,
                .l_start=(off_t)((another_id-1)*sizeof(Account)),
                .l_len=sizeof(Account),
                .l_pid=0
            };
            if (fcntl(reqP->open_fd, F_SETLKW, &lock2) < 0){ ERR_EXIT("transfer fcntl()"); }
            lseek(reqP->open_fd, (off_t)((another_id-1)*sizeof(Account)), SEEK_SET);
            read(reqP->open_fd, buf, 8);
            lseek(reqP->open_fd, -8, SEEK_CUR);
            int2bytes(buf+4, ((Account*)buf)->balance + transfer_money);
            write(reqP->open_fd, buf, 8);
        }
    }
    else if (strcmp("balance", reqP->buf) == 0){
        int new_money = atoi(space_ptr+1);
        if (new_money < 0) error_flag = 1;
        else {
            int2bytes(buf+4, new_money);
            write(reqP->open_fd, buf, 8);
        }
    }
    else { error_flag = 1; }
    
    reqP->lock.l_type = F_UNLCK;
    if (fcntl(reqP->open_fd, F_SETLK, &reqP->lock) < 0){
        ERR_EXIT("fcntl()");
    }
    write_hash[reqP->id] = 0;
    close(reqP->open_fd);
    reqP->open_fd = 0;
    return error_flag ? -1 : 0;
}

static void int2bytes(char* p, int i){
    *p     = (i >> 0) & 0xff;
    *(p+1) = (i >> 8) & 0xff;
    *(p+2) = (i >> 16) & 0xff;
    *(p+3) = (i >> 24) & 0xff;
}
#endif