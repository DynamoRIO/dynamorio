/* ******************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

/* This app tests for accurate annotation detection in potentially problematic scenarios:
 *
 *   - annotations in function arguments lists
 *   - annotations in annotation argument lists
 *   - annotated inline functions appearing in annotation argument lists
 *   - multiple annotations on a single line
 *
 * The app additionally verifies that annotations are executed correctly when instrumented
 * in potentially problematic control flow constructs:
 *
 *   - setjmp/longjmp
 *   - constructors (including exception constructors)
 *   - virtual function implementations
 *   - try/catch blocks
 *   - switch and goto statements inside a loop
 *
 * These tests are especially important for the Windows x64 annotations, which rely on
 * MSVC to compile the annotation into a closed control flow unit, knowing that the
 * compiler has no strict obligation to do so.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdexcept>
#include <setjmp.h>
#include "tools.h"
#include "dr_annotations.h"
#include "test_annotation_arguments.h"

#ifdef WINDOWS
#    define INLINE __inline
#else
#    define INLINE inline
#endif

#define DR_LOG(format, ...) \
    DYNAMORIO_ANNOTATE_LOG("<annotation-detection> " format, ##__VA_ARGS__)

/* Test virtual function calls */
class Shape {
public:
    virtual double
    get_area() = 0;
    virtual unsigned int
    get_vertex_count() = 0;
};

/* Test inheritance */
class Square : public Shape {
public:
    Square(double side_length);
    double
    get_side_length();
    double
    get_area();
    unsigned int
    get_vertex_count();

private:
    double side_length;
};

/* Test inheritance */
class Triangle : public Shape {
public:
    Triangle(double a, double b, double c);
    double
    get_a();
    double
    get_b();
    double
    get_c();
    double
    get_area();
    unsigned int
    three();
    void
    set_lengths(double a, double b, double c);
    unsigned int
    get_vertex_count();

private:
    double lengths[3];

    double
    calculate_s();
};

/* Test exception handling */
class Fail : public std::runtime_error {
public:
    Fail(int error_code);
    int
    get_error_code() const;

private:
    int error_code;
};

/* Test a constructor containing only an annotation */
Square::Square(double side_length)
    : side_length(side_length)
{
    DR_LOG("Square::Square() ${timestamp}\n");
}

/* Test an inline C++ function containing an annotation and returning a value */
INLINE double
Square::get_side_length()
{
    TEST_ANNOTATION_TWO_ARGS(1001, (unsigned int)side_length, {
        print("Native two-args in Square::get_side_length()\n");
    });

    return side_length;
}

/* Test calls to an annotated inline function within an annotation arg list */
double
Square::get_area()
{
    TEST_ANNOTATION_TWO_ARGS(1002, (unsigned int)(get_side_length() * get_side_length()),
                             { print("Native two-args in Square::get_area()\n"); });

    return get_side_length() * get_side_length();
}

/* For testing virtual calls as arguments to an annotated inline function */
#pragma auto_inline(off)
unsigned int
Square::get_vertex_count()
{
    return 4;
}
#pragma auto_inline(on)

Triangle::Triangle(double a, double b, double c)
{
    DR_LOG("Triangle::Triangle(): ${timestamp}\n");

    set_lengths(a, b, c);

    TEST_ANNOTATION_THREE_ARGS(1003, (unsigned int)b, (unsigned int)get_area());
}

/* Tests annotations in a virtual function implementation */
unsigned int
Triangle::three()
{
    return TEST_ANNOTATION_THREE_ARGS((unsigned int)get_a(), (unsigned int)get_b(),
                                      (unsigned int)get_c());
}

/* Tests annotations in an inline non-virtual C++ function */
INLINE void
Triangle::set_lengths(double a, double b, double c)
{
    lengths[0] = a;

    TEST_ANNOTATION_TWO_ARGS(1004, (unsigned int)b,
                             { print("Native two-args in Square::set_lengths()\n"); });

    lengths[1] = b;
    lengths[2] = c;

    TEST_ANNOTATION_THREE_ARGS(1005, (unsigned int)b, (unsigned int)get_area());
}

/* For adding variety to long argument lists */
INLINE double
Triangle::get_a()
{
    return lengths[0];
}

/* Tests an inline function having a void annotation with an annotated inline function
 * in its arg list. The function returns a value to confirm stack integrity.
 */
INLINE double
Triangle::get_b()
{
    TEST_ANNOTATION_TWO_ARGS(1006, (unsigned int)get_c(),
                             { print("Native two-args in Triangle::get_b()\n"); });

    return lengths[1];
}

/* Tests an inline function having void and non-void annotations and a return value */
INLINE double
Triangle::get_c()
{
    TEST_ANNOTATION_TWO_ARGS(1007, (unsigned int)get_a(),
                             { print("Native two-args in Triangle::get_c()\n"); });
    TEST_ANNOTATION_THREE_ARGS(1008, 0x77, 0x7890);

    return lengths[2];
}

/* Tests an inline virtual function implementation that calls annotated inline functions
 * which have inline function calls in their arg lists. The optimized build inlines the
 * entire chain. It is especially important on Windows x64 that the inline function
 * `get_c()` be positioned between this `get_area()` and `Triangle::get_b()`, since it
 * uses the annotation line number in the jump-over predicate.
 */
INLINE double
Triangle::get_area()
{
    double s = calculate_s();
    s *= (s - get_a());
    print("get_area(): s with a: %f\n", s);
    s *= (s - get_b());
    print("get_area(): s with b: %f\n", s);
    s *= (s - get_c());
    print("get_area(): s with c: %f\n", s);
    return s;
}

/* For testing virtual calls as arguments to an annotated inline function */
#pragma auto_inline(off)
unsigned int
Triangle::get_vertex_count()
{
    return 3;
}
#pragma auto_inline(on)

/* For variety in arg lists */
double
Triangle::calculate_s()
{
    return (lengths[0] + lengths[1] + lengths[2]) / 2.0;
}

/* Tests an exception constructor containing only an annotation */
Fail::Fail(int error_code)
    : runtime_error("foo")
    , error_code(error_code)
{
    TEST_ANNOTATION_TWO_ARGS(1009, error_code,
                             { print("Native two-args in Fail::Fail()\n"); });
}

/* For testing an annotated function call on a `catch` block's argument */
int
Fail::get_error_code() const
{
    TEST_ANNOTATION_TWO_ARGS(1010, error_code,
                             { print("Native two-args in Fail::get_error_code()\n"); });

    return error_code;
}

/* For testing stack integrity around annotations with long arg lists */
static void
annotation_wrapper(int a, int b, int c, int d)
{
    if (DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO())
        TEST_ANNOTATION_EIGHT_ARGS(a, b, c, d, a, b, c, d);
}

/* For testing C calls appearing in annotation arg lists */
static int
power(int x, unsigned int exp)
{
    unsigned int i;
    int base = x;

    if (x == 0)
        return 0;
    if (exp == 0)
        return 1;

    for (i = 1; i < exp; i++)
        x *= base;
    return x;
}

/* Tests an inline function with an annotation having static args */
static INLINE int
two()
{
    TEST_ANNOTATION_TWO_ARGS(1011, 5, { print("Native two args: %d, %d\n", 1012, 5); });
    return 2;
}

/* Tests an inline function returning an annotation result. Also facilitates testing
 * stange arguments passed to an annotation via inline function wrapper.
 */
static INLINE int
three(unsigned int a, unsigned int b)
{
    return TEST_ANNOTATION_THREE_ARGS(1013, a, b);
}

/* Tests correctness of two Windows x64 annotations on the same line */
static void
colocated_annotation_test()
{
    /* This line is over length so that it can test two annotations on the same line */
    TEST_ANNOTATION_EIGHT_ARGS(1014, 2, 3, 4, 5, 6, 7, 8);
    TEST_ANNOTATION_NINE_ARGS(1014, 2, 3, 4, 5, 6, 7, 8, 9);
}

/* Tests control flow correctness in a large loopy function having a switch statement,
 * goto, exceptions, setjmp/longjmp and very long chains of inline functions having
 * long arg lists with annotations at every level of inlining, including return values.
 */
int
main(void)
{
    int result = 0;
    unsigned int i, j;
    jmp_buf setjmp_context;

    Shape *shape;
    Triangle *t = new Triangle(4.3, 5.2, 6.1);
    Square *s = new Square(7.0);

    result = setjmp(setjmp_context);
    if ((TEST_ANNOTATION_THREE_ARGS(1015, 2, 3) == two()) || (result != 0)) {
        print("longjmp %d (%f)\n", result, t->get_area());
        goto done;
    }

    shape = t;
    print("Triangle [%f x %f x %f] area: %f (%d)\n", t->get_a(), t->get_b(), t->get_c(),
          t->get_area(), three((unsigned int)shape->get_vertex_count(), t->three()));

    shape = s;
    print("Square [%f x %f] area: %f (%d)\n", s->get_side_length(), s->get_side_length(),
          shape->get_area(),
          three((unsigned int)shape->get_vertex_count(),
                three((unsigned int)shape->get_area(), (unsigned int)t->get_b()) == two()
                    ? DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO()
                    : TEST_ANNOTATION_THREE_ARGS(t->three(), t->three(), t->three())));

    try {
        TEST_ANNOTATION_NINE_ARGS(1016, 2, 3, 4, 5, 6, 7, 8, 9);
        throw(Fail(TEST_ANNOTATION_THREE_ARGS((unsigned int)t->get_b(),
                                              (unsigned int)shape->get_area(), 4)));
        TEST_ANNOTATION_TWO_ARGS(two(), 4, { print("Native line %d\n", 1017); });
    } catch (const Fail &fail) {
        TEST_ANNOTATION_TWO_ARGS(1, two(), { print("Native line %d\n", 1018); });
        print("Fail! %d\n", fail.get_error_code());
    }

    TEST_ANNOTATION_TWO_ARGS(two(), 4, { print("Native line %d\n", 1019); });
    print("three args #0: %d\n", TEST_ANNOTATION_THREE_ARGS(1, 2, 3));
    print("three args #1: %d\n", TEST_ANNOTATION_THREE_ARGS(three(9, 8), two(), 1));
    print("three args #2: %d\n", TEST_ANNOTATION_THREE_ARGS(two(), 4, three(2, 3)));

    colocated_annotation_test();

    j = ((unsigned int)shape->get_area()) % 11;
    for (i = 0; i < 10; i++) {
        DR_LOG("Iteration %d\n", i);
        switch ((i + j) % 10) {
        case 0:
            TEST_ANNOTATION_NINE_ARGS(
                power(2, power(i, 3) % 9), power(3, 4), power(i, j), power(2, i),
                power(two(), 3), power(3, 4), DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO(),
                power(i, j), power(DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO(), i));
        case 1: TEST_ANNOTATION_EIGHT_ARGS(1020, 2, 3, 4, 5, 6, 7, 8);
        case 2:
        test_goto_label:
        case 3:
        case 4:
            TEST_ANNOTATION_NINE_ARGS(1021, 2, 3, 4, 5, 6, 7, 8, 9);
            TEST_ANNOTATION_EIGHT_ARGS(1022, 2, 3, 4, 5, 6, 7, 8);
            break;
        case 5:
            TEST_ANNOTATION_NINE_ARGS(1023, 2, 3, 4, 5, 6, 7, 8, 9);
            annotation_wrapper(i, j, i + j, i * j);
            TEST_ANNOTATION_TEN_ARGS(1024, 2, 3, 4, 5, 6, 7, 8, 9, 10);
            break;
        case 6: TEST_ANNOTATION_EIGHT_ARGS(1025, 2, 3, 4, 5, 6, 7, 8);
        case 7: {
            unsigned int a = two();
            unsigned int b = three(i, j);
            TEST_ANNOTATION_TEN_ARGS(1026, 2, 3, 4, 5, 6, 7, 8, 9, 10);
            TEST_ANNOTATION_TEN_ARGS(1027, 2, 3, power(4, b), 5, 6, 7, 8, 9, 10);
            a = b;
            if (b > 0)
                goto test_goto_label;
        }
        case 8:
        case 9:
        default:
            TEST_ANNOTATION_EIGHT_ARGS(1028, 2, 3, 4, 5, 6, 7, 8);
            TEST_ANNOTATION_NINE_ARGS(1029, 2, 3, 4, 5, 6, 7, 8, 9);
            TEST_ANNOTATION_TEN_ARGS(1030, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        }
    }

    longjmp(setjmp_context, three(i, j));

done:
    return result;
}
