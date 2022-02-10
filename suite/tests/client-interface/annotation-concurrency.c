/* ******************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
 * ******************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* This app is designed to test several aspects of the DR annotations. The basic
 * functionality of the app is to solve simple linear equations using the Jacobi method.
 * It exercises the following special cases of annotations:
 *
 *   - long argument lists
 *   - concurrent invocation of annotations
 *   - concurrent un/registration of Valgrind annotation handlers
 *   - un/registration between subsequent translation of the same DR annotation
 *   - repeatedly loading and unloading the same shared library, which is also annotated
 *
 * Note that concurrent un/registration of DR annotations is not an interesting test case
 * because a DR annotation is instrumented directly with a clean call, and does not
 * change behavior with un/registration after the instrumentation has occurred.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "tools.h"
#include "dr_annotations.h"
#include "test_mode_annotations.h"
#include "test_annotation_arguments.h"

#ifdef WINDOWS
#    include <windows.h>
#    include <process.h>
#else
#    include <pthread.h>
#    include <dlfcn.h>
#    include <unistd.h>
#endif

#ifdef ANNOTATIONS_DISABLED
/* TODO i#1672: Add annotation support to AArchXX.
 * For now, we provide a fallback so we can build this app for use with
 * drcachesim tests.
 */
#    undef DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO
#    define DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO() (true)
#endif

#define MAX_ITERATIONS 10
#define MAX_THREADS 8
#define TOLERANCE 1.0E-5
#define UNKNOWN_MODE 0xffffffffU

#ifdef WINDOWS
#    define SPRINTF(dst, size, src, ...) sprintf_s(dst, size, src, __VA_ARGS__)
#    define THREAD_ENTRY_DECL int WINAPI
typedef HMODULE MODULE_TYPE;
#else
#    define SPRINTF(dst, size, src, ...) sprintf(dst, src, __VA_ARGS__)
#    define THREAD_ENTRY_DECL void
typedef void *MODULE_TYPE;
#endif

#define VALIDATE(value, predicate, error_message)          \
    do {                                                   \
        if (value predicate) {                             \
            print("\n Error: " error_message "\n", value); \
            exit(1);                                       \
        }                                                  \
    } while (0)

typedef struct _thread_init_t {
    unsigned int id;
    int iteration_count;
} thread_init_t;

enum { MODE_0, MODE_1 };

static double **a_matrix, *rhs_vector, *x_new, *x_old;
static int thread_handling_index = 0, matrix_size;

/* shared library handles */
static const char *lib_name;
static MODULE_TYPE jacobi_module;
static void (*jacobi_init)(int matrix_size, bool enable_annotations);
static void (*jacobi_exit)();
static void (*jacobi)(double *dst, double *src, double **coefficients, double *rhs_vector,
                      int limit, unsigned int worker_id);

static void
print_usage()
{
    print("usage: jacobi { A | B | C } <thread-count> [matrix-size iters]\n");
    print(" e.g.: jacobi A 4\n");
}

#ifdef WINDOWS
static void *
find_function(MODULE_TYPE jacobi_module, const char *name)
{
    void *function = GetProcAddress(jacobi_module, name);
    if (function == NULL) {
        print("Error: failed to load %s() from lib %s:\n", name, lib_name);
        exit(1);
    }
    return function;
}
#else
static void *
find_function(MODULE_TYPE jacobi_module, const char *name)
{
    const char *error;
    void *function = dlsym(jacobi_module, name);
    error = dlerror();
    if (error != NULL) {
        print("Error: failed to load %s() from lib %s:\n%s\n", name, lib_name, error);
        exit(1);
    }
    return function;
}
#endif

/* Computes current solution accuracy. Called at the end of each work cycle. */
static double
distance(double *x_old, double *x_new)
{
    int i;
    double sum = 0.0;

    TEST_ANNOTATION_SET_MODE(thread_handling_index, MODE_1,
                             { print("     Mode 1 on %d\n", thread_handling_index); });
    for (i = 0; i < matrix_size; i++)
        sum += (x_new[i] - x_old[i]) * (x_new[i] - x_old[i]);
    print("\n     Finished computing current solution distance in mode %d.\n",
          TEST_ANNOTATION_GET_MODE(thread_handling_index));
    TEST_ANNOTATION_SET_MODE(thread_handling_index, MODE_0,
                             { print("     Mode 0 on %d\n", thread_handling_index); });
    print("     Mode changed to %d.\n", TEST_ANNOTATION_GET_MODE(thread_handling_index));
    return sum;
}

THREAD_ENTRY_DECL
thread_main(thread_init_t *init)
{
    TEST_ANNOTATION_SET_MODE(init->id, MODE_1,
                             { print("     Mode 1 on %d\n", init->id); });
    jacobi(x_new, x_old, a_matrix, rhs_vector, init->iteration_count, init->id);
    TEST_ANNOTATION_SET_MODE(init->id, MODE_0,
                             { print("     Mode 0 on %d\n", init->id); });
#ifdef WINDOWS
    return 0;
#endif
}

int
main(int argc, char **argv)
{
    int i, class_id, num_threads, i_thread, i_row, i_col, iteration = 0;
    double sum, row_sum;

    thread_init_t *thread_inits;
#ifdef WINDOWS
    uintptr_t result;
    HANDLE *threads;
#else /* UNIX */
    int result;
    char *error, *filename_start, *lib_path;
    char buffer[1024];
    pthread_attr_t pta;
    pthread_t *threads;
#endif

    /* Parse and evaluate arguments */
    if (argc != 4 && argc != 6) {
        print("Wrong number of arguments--found %d but expected 3 or 5.\n", argc);
        for (i = 1; i < argc; i++)
            print("\targ: '%s'\n", argv[i]);
        print_usage();
        exit(1);
    }

    lib_name = argv[1];

#ifdef UNIX
    /* Find the test's dynamic library in the app cwd */
    lib_path = argv[0];
    if (lib_path[0] == '/') {
        strcpy(buffer, lib_path);
    } else {
        if (getcwd(buffer, BUFFER_SIZE_ELEMENTS(buffer)) == NULL) {
            print("Failed to locate the test module!\n");
            exit(1);
        }
        strcat(buffer, "/");
        strcat(buffer, lib_path);
    }
    lib_path = buffer;
    filename_start = strrchr(lib_path, '/');
    if (filename_start == NULL) {
        print("Failed to locate the test module!\n");
        exit(1);
    }
    *(filename_start + 1) = '\0';
    strcat(lib_path, lib_name);
#endif

    /* Print app banner */
    print("\n    -------------------------------------------------------------------");
    print("\n     Performance for solving AX=B Linear Equation using Jacobi method");
    if (DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO()) {
        print("\n     Running on DynamoRIO");
        print("\n     Client version %s", TEST_ANNOTATION_GET_CLIENT_VERSION());
    } else {
        print("\n     Running native");
    }
    print("\n    ...................................................................\n");

    class_id = *argv[2] - 'A';
    num_threads = atoi(argv[3]);
    int total_iterations = MAX_ITERATIONS;
    matrix_size = 512;
    if (argc == 6) {
        matrix_size = atoi(argv[4]);
        total_iterations = atoi(argv[5]);
    }

    if (num_threads > MAX_THREADS) {
        print("\nMaximum thread count is %d. Exiting now.\n", MAX_THREADS);
        exit(1);
    }
    if ((class_id >= 0) && (class_id <= 2))
        matrix_size = matrix_size * (1 << class_id);
    else {
        print("Unknown class id\n");
        print_usage();
        exit(1);
    }

    print("\n     Matrix Size :  %d", matrix_size);
    print("\n     Threads     :  %d", num_threads);
    print("\n\n");

    /* Allocate data structures */
    a_matrix = (double **)malloc(matrix_size * sizeof(double *));
    rhs_vector = (double *)malloc(matrix_size * sizeof(double));
    x_new = (double *)malloc(matrix_size * sizeof(double));
    x_old = (double *)malloc(matrix_size * sizeof(double));

    /* Initialize the data structures */
    row_sum = (double)(matrix_size * (matrix_size + 1) / 2.0);
    for (i_row = 0; i_row < matrix_size; i_row++) {
        a_matrix[i_row] = (double *)malloc(matrix_size * sizeof(double));
        sum = 0.0;
        for (i_col = 0; i_col < matrix_size; i_col++) {
            a_matrix[i_row][i_col] = (double)(i_col + 1);
            if (i_row == i_col)
                a_matrix[i_row][i_col] = (double)(row_sum);
            sum = sum + a_matrix[i_row][i_col];
        }
        rhs_vector[i_row] = (double)(2 * row_sum) - (double)(i_row + 1);
    }

    TEST_ANNOTATION_GET_PC();

    TEST_ANNOTATION_TEN_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

    /* Initailize X[i] = B[i] */
    for (i_row = 0; i_row < matrix_size; i_row++) {
        x_new[i_row] = rhs_vector[i_row];
    }

    /* Initialize the client's per-thread data structures (if necessary) */
    if (DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO()) {
        TEST_ANNOTATION_INIT_MODE(MODE_0);
        TEST_ANNOTATION_INIT_MODE(MODE_1);

        for (i_thread = 0; i_thread < num_threads; i_thread++) {
            char counter_name[32] = { 0 };
            SPRINTF(counter_name, 32, "thread #%d", i_thread);
            TEST_ANNOTATION_INIT_CONTEXT(i_thread, counter_name, MODE_0);
        }
        thread_handling_index = num_threads;
        TEST_ANNOTATION_INIT_CONTEXT(thread_handling_index, "thread-handling", MODE_0);
    }

    /* Allocate and initialize threads */
#ifdef WINDOWS
    threads = (HANDLE *)malloc(sizeof(HANDLE) * num_threads);
#else
    threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    pthread_attr_init(&pta);
#endif
    thread_inits = malloc(sizeof(thread_init_t) * num_threads);
    for (i_thread = 0; i_thread < num_threads; i_thread++) {
        thread_inits[i_thread].id = i_thread;
        thread_inits[i_thread].iteration_count = matrix_size / num_threads;
    }

    do {
        /* Initialize the next iteration */
        for (i = 0; i < matrix_size; i++)
            x_old[i] = x_new[i];

            /* Load the shared library */
#ifdef WINDOWS
        jacobi_module = LoadLibrary(lib_name);
        if (jacobi_module == NULL) {
            print("Error: failed to load lib %s\n", lib_name);
            exit(1);
        }
#else
        jacobi_module = dlopen(lib_path, RTLD_LAZY);
        if (jacobi_module == 0) {
            print("Error: failed to load lib %s\n", lib_path);
            exit(1);
        }
#endif

        /* Find the shared functions */
        jacobi_init = find_function(jacobi_module, "jacobi_init");
        jacobi_exit = find_function(jacobi_module, "jacobi_exit");
        jacobi = find_function(jacobi_module, "jacobi");

        iteration++;
        print("\n     Started iteration %d of the computation...\n", iteration);

        /* Initialize the shared library */
        jacobi_init(matrix_size, (iteration % 2) > 0);

        TEST_ANNOTATION_SET_MODE(thread_handling_index, MODE_1, {
            print("     Mode 1 on %d\n", thread_handling_index);
        });

        /* Create work threads */
        for (i_thread = 0; i_thread < num_threads; i_thread++) {
#ifdef WINDOWS
            result =
                _beginthreadex(NULL, 0, thread_main, &thread_inits[i_thread], 0, NULL);
            VALIDATE(result, <= 0, "_beginthread() returned code %d ");
            threads[i_thread] = (HANDLE)result;
#else
            result =
                pthread_create(&threads[i_thread], &pta, (void *(*)(void *))thread_main,
                               (void *)&thread_inits[i_thread]);
            VALIDATE(result, != 0, "pthread_create() returned code %d");
#endif
        }

        /* Wait for the work threads to complete */
#ifdef WINDOWS
        WaitForMultipleObjects(num_threads, threads, TRUE /* all */, INFINITE);
#else
        for (i_thread = 0; i_thread < num_threads; i_thread++) {
            result = pthread_join(threads[i_thread], NULL);
            VALIDATE(result, != 0, "pthread_join() returned code %d");
        }
#endif

        TEST_ANNOTATION_SET_MODE(thread_handling_index, MODE_0, {
            print("     Mode 0 on %d\n", thread_handling_index);
        });

        /* Clean up work threads */
#ifdef UNIX
        result = pthread_attr_destroy(&pta);
        VALIDATE(result, != 0, "pthread_attr_destroy() returned code %d");
#endif

        jacobi_exit();

        /* Release the shared library */
#ifdef WINDOWS
        if (jacobi_module != NULL)
            FreeLibrary(jacobi_module);
#else
        if (jacobi_module != 0)
            dlclose(jacobi_module);
#endif

        TEST_ANNOTATION_EIGHT_ARGS(iteration, 2, 3, 4, 5, 6, 7, 18);
        TEST_ANNOTATION_EIGHT_ARGS(1, 2, 3, 4, 5, 6, 7, 28);
    } while ((distance(x_old, x_new) >= TOLERANCE) && (iteration < total_iterations));

    print("\n");
    print("\n     The Jacobi Method For AX=B .........DONE");
    print("\n     Total Number Of iterations   :  %d", iteration);
    print("\n    ...................................................................\n");

    free(x_new);
    free(x_old);
    free(a_matrix);
    free(rhs_vector);
    free(threads);
    free(thread_inits);

    return 0;
}
