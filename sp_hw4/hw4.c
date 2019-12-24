#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define F float

typedef struct {
    F* data;
    int rows, cols;
} Mat;

void matmul(Mat A, Mat B, Mat C); // AB = C
int num_threads;
int xtrain_fd, ytrain_fd, xtest_fd;

unsigned char xbytes[60000*784], ybytes[60000];
F xdata[60000*784], ydata[60000*10], y_hat[60000*10], xdata_t[784*60000];
F weights[784*10], w_grad[784*10];
F lr;

int main(int argc, char* argv[]){
    if (argc != 5){
        fprintf(stderr, "Usage: %s [X_train] [y_train] [X_test] [number of threads]\n", argv[0]);
        exit(1);
    }

    num_threads = atoi(argv[4]);
    xtrain_fd = open(argv[1], O_RDONLY);
    ytrain_fd = open(argv[2], O_RDONLY);
    //xtest_fd  = open(argv[3], O_RDONLY);

    read(xtrain_fd, xbytes, 60000*784);
    read(ytrain_fd, ybytes, 60000);

    // prepare data in float
    for (int i=0; i<60000; ++i){
        for (int j=0; j<784; ++j){
            xdata[784*i+j] = (F)xbytes[784*i+j]/255;
            xdata_t[60000*j+i] = xdata[784*i+j];
        }
        ydata[10*i+ybytes[i]] = 1;
    }

    // NOTE: xdata[60000 * 784], ydata[60000 * 10], weights[784 * 10]
    // TODO: random initialize weights
    lr = 0.001;
    
    matmul( (Mat){xdata, 60000, 784}, 
            (Mat){weights, 784, 10},
            (Mat){y_hat, 60000, 10} );
    
    F tmp;
    // loop i
    for (int i=0; i<60000; ++i){
        tmp = 0;
        for (int j=0; j<10; ++j){
            tmp += exp(y_hat[10*i+j]);
        }
        for (int j=0; j<10; ++j){
            y_hat[10*i+j] = exp(y_hat[10*i+j])/tmp;
        }
    }

    // loop i, j
    for (int i=0; i<60000; ++i){
        for (int j=0; j<10; ++j){
            y_hat[10*i+j] -= ydata[10*i+j];
        }
    }

    matmul( (Mat){xdata_t, 784, 60000}, 
            (Mat){y_hat, 60000, 10},
            (Mat){w_grad, 784, 10} );

    // loop i, j
    for (int i=0; i<784; ++i){
        for (int j=0; j<10; ++j){
            weights[10*i+j] -= lr * w_grad[10*i+j];
        }
    }



    return 0;
}

void matmul(Mat A, Mat B, Mat C){
    // loop i, j
    for (int i=0; i<A.rows; ++i){
        for (int j=0; j<B.cols; ++j){
            C.data[i*B.cols+j] = 0;
            for (int k=0; k<A.cols; ++k){
                C.data[i*C.cols+j] += A.data[i*A.cols+k] * B.data[j+(B.cols*k)];
            }
        }
    }
}
