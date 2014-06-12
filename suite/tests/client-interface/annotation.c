#if defined(_MSC_VER) && !defined(WINDOWS)
# define WINDOWS
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "annotation/dynamorio_annotations.h"
#include "annotation/test_mode_annotations.h"
#include "annotation/test_annotation_arguments.h"

#ifdef WINDOWS
# include <windows.h>
# include <process.h>
#else
# include <pthread.h>
# include <dlfcn.h>
# include <unistd.h>
#endif

#define MAX_ITERATIONS 10
#define MAX_THREADS 8
#define TOLERANCE 1.0E-5

#ifdef WINDOWS
# define SPRINTF(dst, size, src, ...) sprintf_s(dst, size, src, __VA_ARGS__);
# define LIB_NAME "client.annotation.dll"
# define MODULE_TYPE HMODULE
#else
# define SPRINTF(dst, size, src, ...) sprintf(dst, src, __VA_ARGS__);
# define LIB_NAME "libclient.annotation.appdll.so"
# define MODULE_TYPE void *
#endif

#define VALIDATE(value, predicate, error_message) \
do { \
    if (value predicate) { \
        printf("\n Error: "error_message"\n", value); \
        fflush(stdout); \
        exit(-1); \
    } \
} while (0)

typedef struct _thread_init_t {
    unsigned int id;
    int iteration_count;
} thread_init_t;

enum {
    MODE_0,
    MODE_1
};

static void
usage(const char *message);

static double
distance(double *x_old, double *x_new);

static void *
find_function(MODULE_TYPE module, const char *name);

void (*jacobi_init)(int matrix_size, int annotation_mode);
void (*jacobi_exit)();
void (*jacobi)(double *dst, double *src, double **coefficients,
               double *rhs_vector, int limit);

#ifdef WINDOWS
int WINAPI
#else
void
#endif
thread_main(thread_init_t *);

static double **a_matrix, *rhs_vector, *x_new, *x_old;
static int thread_handling_index, matrix_size;
MODULE_TYPE module;

int main(int argc, char **argv)
{
    int i, class_id, num_threads, i_thread, i_row, i_col, iteration = 0;
    double sum, row_sum;
    char *error;

    thread_init_t *thread_inits;
#ifdef WINDOWS
    uintptr_t result;
    HANDLE *threads;
#else
    int result;
    pthread_attr_t pta;
    pthread_t *threads;
#endif

#ifndef WINDOWS
    char buffer[1024];
    char *scan, *lib_path = argv[0];
    if (lib_path[0] == '/') {
        strcpy(buffer, lib_path);
    } else {
        if (getcwd(buffer, 1024) == NULL) {
            printf("Failed to locate the test module!\n");
            exit(1);
        }
        strcat(buffer, "/");
        strcat(buffer, lib_path);
    }
    lib_path = buffer;
    scan = lib_path + strlen(lib_path);
    while ((--scan > lib_path) && (*scan != '/'))
        ;
    *(scan+1) = '\0';
    strcat(lib_path, LIB_NAME);
#endif

    printf("\n    -------------------------------------------------------------------");
    printf("\n     Performance for solving AX=B Linear Equation using Jacobi method");
    if (DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO())
        printf("\n     Running on DynamoRIO");
    else
        printf("\n     Running native");
    printf("\n    ...................................................................\n");

    if( argc != 2 )
        usage("Wrong number of arguments.");

    class_id = *argv[1] - 'A';
    num_threads = atoi(argv[1] + 1);

    if (num_threads > MAX_THREADS) {
        printf("\nMaximum thread count is %d. Exiting now.\n", MAX_THREADS);
        exit(-1);
    }
    if ((class_id >= 0) && (class_id <= 2))
        matrix_size = 1024 * (1 << class_id);
    else
        usage("Unknown class id");

    printf("\n     Matrix Size :  %d", matrix_size);
    printf("\n     Threads     :  %d", num_threads);
    printf("\n\n");
    fflush(stdout);

    a_matrix = (double **) malloc(matrix_size * sizeof(double *));
    rhs_vector = (double *) malloc(matrix_size * sizeof(double));

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

    TEST_ANNOTATION_TEN_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

    x_new = (double *) malloc(matrix_size * sizeof(double));
    x_old = (double *) malloc(matrix_size * sizeof(double));

    /* Initailize X[i] = B[i] */
    for (i_row = 0; i_row < matrix_size; i_row++) {
        x_new[i_row] =  rhs_vector[i_row];
    }

    TEST_ANNOTATION_INIT_MODE(MODE_0);
    TEST_ANNOTATION_INIT_MODE(MODE_1);

    for (i_thread = 0; i_thread < num_threads; i_thread++) {
        char counter_name[32] = {0};
        SPRINTF(counter_name, 32, "thread #%d", i_thread);
        TEST_ANNOTATION_INIT_CONTEXT(i_thread, counter_name, MODE_0);
    }
    thread_handling_index = num_threads;
    TEST_ANNOTATION_INIT_CONTEXT(thread_handling_index, "thread-handling", MODE_0);

#ifdef WINDOWS
    threads = (HANDLE*) malloc(sizeof(HANDLE) * num_threads);
#else
    threads = (pthread_t *) malloc(sizeof(pthread_t) * num_threads);
    pthread_attr_init(&pta);
#endif
    thread_inits = malloc(sizeof(thread_init_t) * num_threads);
    for (i_thread = 0; i_thread < num_threads; i_thread++) {
        thread_inits[i_thread].id = i_thread;
        thread_inits[i_thread].iteration_count = matrix_size/num_threads;
    }

    do {
        for (i = 0; i < matrix_size; i++)
            x_old[i] = x_new[i];

#ifdef WINDOWS
        module = LoadLibrary(LIB_NAME);
        if (module == NULL) {
            printf("Error: failed to load "LIB_NAME"\n");
            exit(1);
        }
#else
        module = dlopen(lib_path, RTLD_LAZY);
        if (module == 0) {
            printf("Error: failed to load "LIB_NAME"\n");
            exit(1);
        }

#endif
        jacobi_init = find_function(module, "jacobi_init");
        jacobi_exit = find_function(module, "jacobi_exit");
        jacobi = find_function(module, "jacobi");

        iteration++;
        printf("\n     Started iteration %d of the computation...\n", iteration);
        fflush(stdout);

        jacobi_init(matrix_size, iteration % 2);

        TEST_ANNOTATION_SET_MODE(thread_handling_index, MODE_1);

        for (i_thread = 0; i_thread < num_threads; i_thread++) {
            /* Creating The Threads */
#ifdef WINDOWS
            result = _beginthreadex(NULL, 0, thread_main, &thread_inits[i_thread], 0, NULL);
            VALIDATE(result, <= 0, "_beginthread() returned code %d ");
            threads[i_thread] = (HANDLE) result;
#else
            result = pthread_create(&threads[i_thread], &pta, (void *(*) (void *))thread_main,
                                    (void *) &thread_inits[i_thread]);
            VALIDATE(result, != 0, "pthread_create() returned code %d");
#endif
        }

#ifdef WINDOWS
        WaitForMultipleObjects(num_threads, threads, TRUE /* all */, INFINITE);
#else
        for (i_thread = 0; i_thread < num_threads; i_thread++) {
            result = pthread_join(threads[i_thread], NULL);
            VALIDATE(result, != 0, "pthread_join() returned code %d");
        }
#endif

        TEST_ANNOTATION_SET_MODE(thread_handling_index, MODE_0);

#ifndef WINDOWS
        result = pthread_attr_destroy(&pta);
        VALIDATE(result, != 0, "pthread_attr_destroy() returned code %d");
#endif

        jacobi_exit();
#ifdef WINDOWS
        if (module != NULL)
            FreeLibrary(module);
#else
        if (module != 0)
            dlclose(module);
#endif

        TEST_ANNOTATION_EIGHT_ARGS(1, 2, 3, 4, 5, 6, 7, 8);
        TEST_ANNOTATION_EIGHT_ARGS(1, 2, 3, 4, 5, 6, 7, 8);
    } while ((distance(x_old, x_new) >= TOLERANCE) && (iteration < MAX_ITERATIONS));

    printf("\n");
    printf("\n     The Jacobi Method For AX=B .........DONE");
    printf("\n     Total Number Of iterations   :  %d",iteration);
    printf("\n    ...................................................................\n");

    free(x_new);
    free(x_old);
    free(a_matrix);
    free(rhs_vector);
    free(threads);
    free(thread_inits);

    return 0;
}

static void
usage(const char *message)
{
    printf("%s\n", message);
    printf("usage: jacobi { A | B | C }<thread-count>\n");
    printf(" e.g.: jacobi A4\n");
    exit(-1);
}

static void *
find_function(MODULE_TYPE module, const char *name)
{
    char *error;

    void *function = dlsym(module, name);
    if (error = dlerror()) {
        printf("Error: failed to load %s() from "LIB_NAME":\n%s\n", name, error);
        exit(1);
    }
    return function;
}

double distance(double *x_old, double *x_new)
{
    int i;
    double sum = 0.0;

    TEST_ANNOTATION_SET_MODE(thread_handling_index, MODE_1);
    for (i = 0; i < matrix_size; i++)
        sum += (x_new[i] - x_old[i]) * (x_new[i] - x_old[i]);
    TEST_ANNOTATION_SET_MODE(thread_handling_index, MODE_0);
    return sum;
}

#ifdef WINDOWS
int WINAPI
#else
void
#endif
thread_main(thread_init_t *init)
{
    TEST_ANNOTATION_SET_MODE(init->id, MODE_1);
    jacobi(x_new, x_old, a_matrix, rhs_vector, init->iteration_count);
    TEST_ANNOTATION_SET_MODE(init->id, MODE_0);
#ifdef WINDOWS
    return 0;
#endif
}





















