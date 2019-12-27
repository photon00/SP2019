#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NUM_IMG 60000
#define LEARNING_RATE 0.01
#define THREAD

#define F double

typedef struct {
    F* data;
    int rows, cols;
} Mat;

typedef struct {
    Mat A, B, C;
} Mat3struct;

void* matmul_t(void *arg); // AB = C
void* matmul(void *arg); // AB = C
int num_threads;
int xtrain_fd, ytrain_fd, xtest_fd;
pthread_t *tids;
Mat3struct *args;

unsigned char xbytes[60000*784], ybytes[60000];
F xdata[NUM_IMG*785], ydata[NUM_IMG*10], y_hat[NUM_IMG*10], xdata_t[785*NUM_IMG];
F weights[785*10], w_grad[785*10];
// For Adam
F M[785*10], V[785*10];
F beta1 = 0.9, beta2 = 0.999;

int main(int argc, char* argv[]){
    if (argc != 5){
        fprintf(stderr, "Usage: %s [X_train] [y_train] [X_test] [number of threads]\n", argv[0]);
        exit(1);
    }

    num_threads = atoi(argv[4]);
    xtrain_fd = open(argv[1], O_RDONLY);
    ytrain_fd = open(argv[2], O_RDONLY);
    //xtest_fd  = open(argv[3], O_RDONLY);

    tids = malloc(sizeof(pthread_t) * num_threads);
    args = malloc(sizeof(Mat3struct) * num_threads);

    read(xtrain_fd, xbytes, 60000*784); close(xtrain_fd);
    read(ytrain_fd, ybytes, 60000); close(ytrain_fd);

    // prepare data in float
    for (int i=0; i<NUM_IMG; ++i){
        for (int j=0; j<784; ++j){
            xdata[785*i+j] = (F)xbytes[784*i+j]/255;
            xdata_t[NUM_IMG*j+i] = xdata[785*i+j];
        }
        xdata[785*i+784] = 1; 
        xdata_t[NUM_IMG*784+i] = 1;
        ydata[10*i+ybytes[i]] = 1;
    }

    // NOTE: xdata[60000 * 785], ydata[60000 * 10], weights[785 * 10]
    // random initialize weights
    srand(0);
    for (int i=0; i<785; ++i){
        for (int j=0; j<10; ++j){
            weights[10*i+j] = ((rand()/(RAND_MAX + 1.0)) - 0.5) / 28;
        }
    }

    F lr = LEARNING_RATE;
    F loss = 0, pre_loss = 0, min_loss=100;
    long F tmp, acc;

    // start training
    int epoch = 0, min_epoch=0;
    int dividable = (NUM_IMG%num_threads)==0? 1 : 0;
    int job_rows = dividable ? NUM_IMG/num_threads : NUM_IMG/(num_threads-1);
    int start_row = 0, tmp_i;
    Mat3struct arg;
    while (1){
        epoch++;
        // parallel compute matrix multplication
        for (int i=0; i<num_threads; ++i){
            if (i == num_threads-1 && !dividable){
                args[i].A = (Mat){ xdata+i*job_rows*785, NUM_IMG % job_rows, 785 };
                args[i].C = (Mat){ y_hat+i*job_rows*10, NUM_IMG % job_rows, 10 };
            }
            else {
                args[i].A = (Mat){ xdata+i*job_rows*785, job_rows, 785 };
                args[i].C = (Mat){ y_hat+i*job_rows*10, job_rows, 10 };
            }
            args[i].B = (Mat){ weights, 785, 10 };
            #ifdef THREAD 
            pthread_create(&tids[i], NULL, matmul_t, (void*)&args[i]);
            #else
            tids[i] = matmul((void*)&args[i]);
            #endif
        }

        // compute softmax
        for (int t=0; t<num_threads; ++t){
            #ifdef THREAD
            pthread_join(tids[t], (void**)&start_row);
            #else
            start_row = tids[t];
            #endif
            int end_row = start_row+job_rows > NUM_IMG ? NUM_IMG : start_row+job_rows;
            for (int i=start_row; i<end_row; ++i){
                tmp = 1e-7;
                for (int j=0; j<10; ++j){
                    tmp += expl(y_hat[10*i+j]);
                }
                for (int j=0; j<10; ++j){
                    y_hat[10*i+j] = expl(y_hat[10*i+j])/tmp;
                }
            }
        }
        
        // compute accuracy and loss
        acc = loss = 0;
        for (int i=0; i<NUM_IMG; ++i){
            for (int j=0; j<10; ++j){
                if (j == 0){ tmp = y_hat[10*i], tmp_i = 0; }
                if (y_hat[10*i+j] > tmp){ tmp = y_hat[10*i+j], tmp_i = j; }
                y_hat[10*i+j] -= ydata[10*i+j];
                loss += y_hat[10*i+j]*y_hat[10*i+j];
            }
            if (ydata[10*i+tmp_i] == 1){
                acc += 1;
            }
        }
        acc /= NUM_IMG;
        loss /= NUM_IMG;
        fprintf(stderr, "epoch %d: loss = %.4f, acc = %.4Lf\n", epoch, loss, acc);
        if (loss < min_loss) { min_epoch = epoch; min_loss = loss; }
        if (acc > 0.99 || (loss-min_loss > 1 && epoch - min_epoch > 10)){
            fprintf(stderr, "early stop!\n");
            break;
        }
        pre_loss = loss;

        // compute matrix multplication (gradient)
        arg.A = (Mat){ xdata_t, 785, NUM_IMG };
        arg.B = (Mat){ y_hat, NUM_IMG, 10 };
        arg.C = (Mat){ w_grad, 785, 10 };
        matmul((void*)&arg);

        // compute Adam optimizer parameter
        F W_grad;
        for (int i=0; i<785; ++i){
            for (int j=0; j<10; ++j){
                W_grad = w_grad[10*i+j] / NUM_IMG;
                M[10*i+j] = beta1*M[10*i+j] + (1-beta1)*W_grad;
                V[10*i+j] = beta1*V[10*i+j] + (1-beta2)*W_grad*W_grad;
            }
        }
        
        // update weights
        F M_head, V_head;
        for (int i=0; i<785; ++i){
            for (int j=0; j<10; ++j){
                M_head = M[10*i+j] / (1 - pow(beta1, epoch));
                V_head = V[10*i+j] / (1 - pow(beta2, epoch));
                weights[10*i+j] -= lr * M_head / (1e-8 + sqrt(V_head));
            }
        }
    }

    free(tids);
    free(args);
    return 0;
}

void* matmul_t(void *arg){
    Mat A = ((Mat3struct*)arg) -> A;
    Mat B = ((Mat3struct*)arg) -> B;
    Mat C = ((Mat3struct*)arg) -> C;
    for (int i=0; i<A.rows; ++i){
        for (int j=0; j<B.cols; ++j){
            C.data[i*C.cols+j] = 0;
            for (int k=0; k<A.cols; ++k){
                C.data[i*C.cols+j] += A.data[i*A.cols+k] * B.data[j+(B.cols*k)];
            }
        }
    }
    pthread_exit( (void*)((A.data - xdata)/785) );
}

void* matmul(void *arg){
    Mat A = ((Mat3struct*)arg) -> A;
    Mat B = ((Mat3struct*)arg) -> B;
    Mat C = ((Mat3struct*)arg) -> C;
    for (int i=0; i<A.rows; ++i){
        for (int j=0; j<B.cols; ++j){
            C.data[i*C.cols+j] = 0;
            for (int k=0; k<A.cols; ++k){
                C.data[i*C.cols+j] += A.data[i*A.cols+k] * B.data[j+(B.cols*k)];
            }
            if (C.data[i*C.cols+j] != C.data[i*C.cols+j])
                fprintf(stderr, "matmul: C.data = %lf\n", C.data[i*C.cols+j]);
        }
    }
    return (void*)((A.data - xdata)/785);
}
