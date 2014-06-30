
#include <stdlib.h>
#include <stdio.h>
#include "dr_annotations.h"
#include "test_annotation_arguments.h"

#ifdef _MSC_VER
# define INLINE __inline
#else
# define INLINE inline
# ifdef __LP64__
#  define uintptr_t unsigned long long
# else
#  define uintptr_t unsigned long
# endif
#endif

class Shape
{
    public:
        virtual double get_area() = 0;
};

class Square : public Shape
{
    public:
        Square(int side_length);
        double get_side_length();
        double get_area();

    private:
        double side_length;
};

class Triangle : public Shape
{
    public:
        Triangle(double a, double b, double c);
        double get_a();
        double get_b();
        double get_c();
        double get_area();
        unsigned int three();
        void set_lengths(double a, double b, double c);

    private:
        double lengths[3];

        double calculate_s();
};

Square::Square(int side_length)
{
    this->side_length = side_length;
}

INLINE double Square::get_side_length()
{
    TEST_ANNOTATION_TWO_ARGS(__LINE__, (unsigned int) side_length, {
        printf("Native two-args in Square::get_side_length()\n");
    });

    return side_length;
}

double Square::get_area()
{
    TEST_ANNOTATION_TWO_ARGS(__LINE__,
                             (unsigned int) (get_side_length() * get_side_length()), {
        printf("Native two-args in Square::get_area()\n");
    });

    return get_side_length() * get_side_length();
}

Triangle::Triangle(double a, double b, double c)
{
    set_lengths(a, b, c);

    TEST_ANNOTATION_THREE_ARGS(__LINE__, (unsigned int) b,
                               (unsigned int) get_area());
}

unsigned int Triangle::three()
{
    return TEST_ANNOTATION_THREE_ARGS((unsigned int) get_a(), (unsigned int) get_b(),
                                      (unsigned int) get_c());
}

INLINE void Triangle::set_lengths(double a, double b, double c)
{
    lengths[0] = a;

    TEST_ANNOTATION_TWO_ARGS(__LINE__, (unsigned int) b, {
        printf("Native two-args in Square::set_lengths()\n");
    });

    lengths[1] = b;
    lengths[2] = c;

    TEST_ANNOTATION_THREE_ARGS(__LINE__, (unsigned int) b,
                               (unsigned int) get_area());
}

INLINE double Triangle::get_a()
{
    return lengths[0];
}

INLINE double Triangle::get_b()
{
    TEST_ANNOTATION_TWO_ARGS(__LINE__, get_c(), {
        printf("Native two-args in Triangle::get_b()\n");
    });

    return lengths[1];
}

INLINE double Triangle::get_c()
{
    TEST_ANNOTATION_TWO_ARGS(__LINE__, get_a(), {
        printf("Native two-args in Triangle::get_c()\n");
    });

    return lengths[2];
}

INLINE double Triangle::get_area()
{
    double s = calculate_s();
    return s * (s - get_a()) * (s - get_b()) * (s - get_c());
}

double Triangle::calculate_s()
{
    return (lengths[0] + lengths[1] + lengths[2]) / 2.0;
}

static void
annotation_wrapper(int a, int b, int c, int d)
{
    if (DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO())
        TEST_ANNOTATION_EIGHT_ARGS(a, b, c, d, a, b, c, d);
}

static int
power(int x, unsigned int exp)
{
    unsigned int i;
    int base = x;
    for (i = 1; i < exp; i++)
        x *= base;
    return x;
}

static INLINE int
two()
{
    TEST_ANNOTATION_TWO_ARGS(__LINE__, 5, {
        printf("Native two args: %d, %d\n", __LINE__, 5);
    });
    return 2;
}

static INLINE int
three(unsigned int a)
{
    return TEST_ANNOTATION_THREE_ARGS(__LINE__, a, 4);
}

static void
colocated_annotation_test()
{
    TEST_ANNOTATION_EIGHT_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8); TEST_ANNOTATION_NINE_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9);
}

// C control flow: ?, switch, if/else, goto, return <value>, setjmp/longjmp,
// C++ control flow: exceptions, vtable?

int main(void)
{
    unsigned int i = 2, j = 3;

    Shape *shape;
    Triangle *t = new Triangle(4.3, 5.2, 6.1);
    Square *s = new Square(7.0);

    shape = t;
    printf("Triangle [%f x %f x %f] area: %f\n", t->get_a(), t->get_b(), t->get_c(),
           shape->get_area());

    shape = s;
    printf("Square [%f x %f] area: %f (%d)\n", s->get_side_length(), s->get_side_length(),
           shape->get_area(), three(t->get_b()));

    printf("tri.three(): %d\n", t->three());
    printf("three-args tri: %d\n", TEST_ANNOTATION_THREE_ARGS(t->three(), t->three(), t->three()));

    // if (1) return 0;

    /*
    TEST_ANNOTATION_NINE_ARGS(
        power(2, power(i, 3)), power(3, 4), power(i, j), power(2, i),
        power(two(), 3), power(3, 4), DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO(),
        power(i, j), power(DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO(), i));
    */

    test_annotation_nine_args(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9);

    TEST_ANNOTATION_TWO_ARGS(1, two(), { printf("Native line %d\n", __LINE__); });
    TEST_ANNOTATION_TWO_ARGS(two(), 4, { printf("Native line %d\n", __LINE__); });
    printf("three args #0: %d\n", TEST_ANNOTATION_THREE_ARGS(1, 2, 3));
    printf("three args #1: %d\n", TEST_ANNOTATION_THREE_ARGS(three(9), two(), 1));
    printf("three args #2: %d\n", TEST_ANNOTATION_THREE_ARGS(two(), 4, three(2)));

    colocated_annotation_test();

    j = (rand() % 10);
    for (i = 0; i < 10; i++) {
        switch ((i + j) % 10) {
            case 0:
                TEST_ANNOTATION_NINE_ARGS(
                    power(2, power(i, 3)), power(3, 4), power(i, j), power(2, i),
                    power(two(), 3), power(3, 4), DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO(),
                    power(i, j), power(DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO(), i));
            case 1:
                TEST_ANNOTATION_EIGHT_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8);
            case 2:
case2:
            case 3:
            case 4:
                TEST_ANNOTATION_NINE_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9);
                TEST_ANNOTATION_EIGHT_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8);
                break;
            case 5:
                TEST_ANNOTATION_NINE_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9);
                annotation_wrapper(i, j, i + j, i * j);
                TEST_ANNOTATION_TEN_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9, 10);
                break;
            case 6:
                TEST_ANNOTATION_EIGHT_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8);
            case 7: {
                unsigned int a = 0, b = 0;
                TEST_ANNOTATION_TEN_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9, 10);
                TEST_ANNOTATION_TEN_ARGS(__LINE__, 2, 3, (unsigned int) (uintptr_t) &b, 5, 6, 7, 8, 9, 10);
                a = b;
                if (b > 0)
                    goto case2;
            }
            case 8:
            case 9:
            default:
                TEST_ANNOTATION_EIGHT_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8);
                TEST_ANNOTATION_NINE_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9);
                TEST_ANNOTATION_TEN_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        }
    }
}
