#if defined(_MSC_VER) && !defined(WINDOWS)
# define WINDOWS
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "annotation/dynamorio_annotations.h"
#include "annotation/bbcount_region_annotations.h"
#include "annotation/memcheck.h"

#ifdef WINDOWS
# include <windows.h>
# include <process.h>
#else
# include <pthread.h>
#endif

#define MAX_ITERATIONS 1000
#define MAX_THREADS 8
#define TOLERANCE 1.0E-5

#ifdef WINDOWS
# define SPRINTF(dst, size, src, ...) sprintf_s(dst, size, src, __VA_ARGS__);
#else
# define SPRINTF(dst, size, src, ...) sprintf(dst, src, __VA_ARGS__);
#endif

#define VALIDATE(value, predicate, error_message) \
do { \
    if (value predicate) { \
        printf("\n Error: "error_message, value); \
        exit(-1); \
    } \
} while (0)

typedef struct _thread_init_t {
    unsigned int id;
    int inner_iteration_count;
    int outer_iteration_count;
} thread_init_t;

static void
usage();

static double
distance(double *x_old, double *x_new);

#ifdef WINDOWS
int WINAPI
#else
void
#endif
jacobi(thread_init_t *);

static double **a_matrix, *rhs_vector, *x_new, *x_old, *x_temp;
static int thread_handling_index, matrix_size;

int main(int argc, char **argv)
{
    int i, class_id, num_threads, i_thread, i_row, i_col, iteration = 0;
    double sum, row_sum, memoryused = 0.0;

    thread_init_t *thread_inits;
#ifdef WINDOWS
    uintptr_t result;
    HANDLE *threads;
#else
    int result;
    pthread_attr_t pta;
    pthread_t *threads;
#endif

    printf("\n    -------------------------------------------------------------------");
    printf("\n     Performance for solving AX=B Linear Equation using JACOBI METHOD");
    if (DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO())
    printf("\n     Running on DynamoRIO");
    else
    printf("\n     Running native");
    printf("\n    ...................................................................\n");

    if( argc != 2 )
        usage();

    class_id = *argv[1] - 'A';
    num_threads = atoi(argv[1] + 1);

    if (num_threads > MAX_THREADS ) {
        printf("\nMaximum thread count is %d. Exiting now.\n", MAX_THREADS);
        exit(-1);
    }
    if ((class_id >= 0) && (class_id <= 2))
        matrix_size = 1024 * (1 << class_id);
    else
        usage();

    printf("\n     Matrix Size :  %d", matrix_size);
    printf("\n     Threads     :  %d", num_threads);

    a_matrix = (double **) malloc(matrix_size * sizeof(double *));
    rhs_vector = (double *) malloc(matrix_size * sizeof(double));

    memoryused += (matrix_size * matrix_size * sizeof(double));
    memoryused += (matrix_size * sizeof(double));

    /* Populating the a_matrix and rhs_vector */
    row_sum = (double) (matrix_size * (matrix_size + 1) / 2.0);
    for (i_row = 0; i_row < matrix_size; i_row++) {
        a_matrix[i_row] = (double *) malloc(matrix_size * sizeof(double));
        sum = 0.0;
        for (i_col = 0; i_col < matrix_size; i_col++) {
            a_matrix[i_row][i_col]= (double) (i_col+1);
            if (i_row == i_col)
                a_matrix[i_row][i_col] = (double)(row_sum);
            sum = sum + a_matrix[i_row][i_col];
        }
        rhs_vector[i_row] = (double)(2 * row_sum) - (double)(i_row + 1);
    }

    printf("\n");

    x_new = (double *) malloc(matrix_size * sizeof(double));
    memoryused += (matrix_size * sizeof(double));
    x_old = (double *) malloc(matrix_size * sizeof(double));
    memoryused += (matrix_size * sizeof(double));
    x_temp = (double *) malloc(matrix_size * sizeof(double));
    memoryused += (matrix_size * sizeof(double));

    /* Initailize X[i] = B[i] */
    for (i_row = 0; i_row < matrix_size; i_row++) {
        x_temp[i_row] = rhs_vector[i_row];
        x_new[i_row] =  rhs_vector[i_row];
    }

    for (i_thread = 0; i_thread < num_threads; i_thread++) {
        char counter_name[32] = {0};
        SPRINTF(counter_name, 32, "thread #%d", i_thread); // TODO: macro
        BB_REGION_ANNOTATE_INIT_COUNTER(i_thread, counter_name);
    }
    thread_handling_index = num_threads;
    BB_REGION_ANNOTATE_INIT_COUNTER(thread_handling_index, "thread-handling");

#ifdef WINDOWS
    threads = (HANDLE*) malloc(sizeof(HANDLE) * num_threads);
#else
    threads = (pthread_t *) malloc(sizeof(pthread_t) * num_threads);
    pthread_attr_init(&pta);
#endif
    thread_inits = malloc(sizeof(thread_init_t) * num_threads);

    do {
        BB_REGION_ANNOTATE_START_COUNTER(thread_handling_index);

        for (i = 0; i < matrix_size; i++)
            x_old[i] = x_new[i];

        for (i_thread = 0; i_thread < num_threads; i_thread++) {
            thread_inits[i_thread].id = i_thread;
            thread_inits[i_thread].inner_iteration_count = matrix_size/num_threads;
            thread_inits[i_thread].outer_iteration_count = iteration;

            /* Creating The Threads */
#ifdef WINDOWS
            result = _beginthreadex(NULL, 0, jacobi, &thread_inits[i_thread], 0, NULL);
            VALIDATE(result, <= 0, "_beginthread() returned code %d ");
            threads[i_thread] = (HANDLE) result;
#else
            result = pthread_create(&threads[i_thread], &pta, (void *(*) (void *))jacobi,
                                    (void *) &thread_inits[i_thread]);
            VALIDATE(result, != 0, "pthread_create() returned code %d");
#endif
        }

        iteration++;
        for (i_thread = 0; i_thread < num_threads; i_thread++) {
#ifdef WINDOWS
            WaitForSingleObject(threads[i_thread], INFINITE);
#else
            result = pthread_join(threads[i_thread], NULL);
            VALIDATE(result, != 0, "pthread_join() returned code %d");
#endif
        }

#ifndef WINDOWS
        result = pthread_attr_destroy(&pta);
        VALIDATE(result, != 0, "pthread_attr_destroy() returned code %d");
#endif
        BB_REGION_ANNOTATE_STOP_COUNTER(thread_handling_index);

        if (DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO()) {
            unsigned int region_count = 0;
            unsigned int bb_count = 0;
            unsigned int thread_region_count, thread_bb_count;
            for (i_thread = 0; i_thread < num_threads; i_thread++) {
                BB_REGION_GET_BASIC_BLOCK_STATS(i_thread, &thread_region_count,
                                                &thread_bb_count);
                region_count += thread_region_count;
                bb_count += thread_bb_count;
            }
            if (region_count > 0) {
                printf("\n     After %d iterations, executed %d basic blocks "
                       "in %d regions", iteration, bb_count, region_count);
            }
        }
    } while ((iteration < MAX_ITERATIONS) && (distance(x_old, x_new) >= TOLERANCE));

    printf("\n");
    printf("\n     The Jacobi Method For AX=B .........DONE");
    printf("\n     Total Number Of iterations   :  %d",iteration);
    printf("\n     Memory Utilized              :  %lf MB",(memoryused/(1024*1024)));
    printf("\n    ...................................................................\n");

    free(x_new);
    free(x_old);
    free(a_matrix);
    free(rhs_vector);
    free(x_temp);
    free(threads);
    free(thread_inits);

    return 0;
}

static void
usage()
{
    printf("usage: jacobi { A | B | C }<thread-count>\n");
    printf(" e.g.: jacobi A4\n");
    exit(-1);
}

double distance(double *x_old, double *x_new)
{
    int i;
    double sum = 0.0;

    BB_REGION_ANNOTATE_START_COUNTER(thread_handling_index);
    for (i = 0; i < matrix_size; i++)
        sum += (x_new[i] - x_old[i]) * (x_new[i] - x_old[i]);
    BB_REGION_ANNOTATE_STOP_COUNTER(thread_handling_index);
    return sum;
}

#ifdef WINDOWS
int WINAPI
#else
void
#endif
jacobi(thread_init_t *init)
{
    int i, j;

    BB_REGION_ANNOTATE_START_COUNTER(init->id);
    for (i = 0; i < init->inner_iteration_count; i++) {
        x_temp[i] = rhs_vector[i];

        for (j = 0; j < i; j++) {
            x_temp[i] -= x_old[j] * a_matrix[i][j];
        }
        for (j = i+1; j < init->inner_iteration_count; j++) {
            x_temp[i] -= x_old[j] * a_matrix[i][j];
        }
        x_temp[i] = x_temp[i] / a_matrix[i][i];
    }
    for (i = 0; i < init->inner_iteration_count; i++) {
        x_new[i] = x_temp[i];
    }
    BB_REGION_ANNOTATE_STOP_COUNTER(init->id);
#ifdef WINDOWS
    return 0;
#endif
}
