#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "annotation/bbcount_region_annotations.h"

#define INPUT_BUFFER_LENGTH 1024

#define IS_WHITESPACE(c) ((c == ' ') || (c == '\t'))
#define IS_DIGIT(c) ((c >= '0') && (c <= '9'))
#define IS_CONSTANT_QUALIFIER(c) ((c == '+') || (c == '-'))
#define OP_IS_STRONGER(a, b) (operator_strength(a) > operator_strength(b))

#ifdef _MSC_VER
# define SSCANF(src, pattern, dst, len) sscanf_s(src, pattern, dst, len)
# define STRCPY(dst, src) strcpy_s(dst, strlen(src) + 1, src)
# define FOPEN(file, filename, mode) fopen_s(&file, filename, mode);
#else
# define SSCANF(src, pattern, dst, len) sscanf(src, pattern, dst)
# define STRCPY(dst, src) strcpy(dst, src)
# define FOPEN(file, filename, mode) file = fopen(filename, mode);
#endif

#define START_REGION_COUNTER(expression) \
do { \
    if (expression->region != NULL) \
        BB_REGION_ANNOTATE_START_COUNTER(expression->region->id); \
} while (0)

#define STOP_REGION_COUNTER(expression) \
do { \
    if (expression->region != NULL) \
        BB_REGION_ANNOTATE_STOP_COUNTER(expression->region->id); \
} while (0)

typedef unsigned int uint;
typedef enum _bool {
    false,
    true
} bool;

typedef struct _script_region_t {
    uint id;
    const char *name;
    struct _script_region_t *next;
} script_region_t;

typedef struct _script_regions_t {
    script_region_t *head;
    script_region_t *tail;
    uint region_index;
    script_region_t *active_region;
} script_regions_t;

typedef enum _expresion_type_t {
    TYPE_CONSTANT,
    TYPE_COMPUTATION
} expression_type_t;

typedef enum _operator_t {
    OP_MODULO,
    OP_DIVISION,
    OP_MULTIPLICATION,
    OP_SUBTRACTION,
    OP_ADDITION,
    OP_NONE
} operator_t;

typedef struct _expression_t {
    expression_type_t type;
    union {
        int value;
        operator_t op;
    } content;
    struct _expression_t *left;
    struct _expression_t *right;
    bool is_grouped;
    script_region_t *region;
    int result;
} expression_t;

typedef struct _expression_list_t {
    expression_t *expression;
    struct _expression_list_t *next;
} expression_list_t;

static script_regions_t *script_regions_list;

/*** Constructors ***/

static expression_t *new_constant(int value) {
    expression_t *constant = malloc(sizeof(expression_t));
    constant->type = TYPE_CONSTANT;
    constant->content.value = value;
    constant->region = script_regions_list->active_region;
    return constant;
}

static expression_t *new_computation(operator_t op, expression_t *l, expression_t *r) {
    expression_t *computation = malloc(sizeof(expression_t));
    computation->type = TYPE_COMPUTATION;
    computation->content.op = op;
    computation->left = l;
    computation->right = r;
    computation->region = script_regions_list->active_region;
    return computation;
}

/*** Parsing ***/

static operator_t parse_operator(char **walk)
{
    switch (*((*walk)++)) {
        case '+': return OP_ADDITION;
        case '-': return OP_SUBTRACTION;
        case '*': return OP_MULTIPLICATION;
        case '/': return OP_DIVISION;
        case '%': return OP_MODULO;
        default: return OP_NONE;
    }
}

static int operator_strength(operator_t op) {
    switch (op) {
        case OP_ADDITION:
        case OP_SUBTRACTION:
            return 1;
        case OP_MULTIPLICATION:
        case OP_DIVISION:
        case OP_MODULO:
            return 2;
    }
    return -1;
}

static char next_char(char **walk)
{
    char c = **walk;
    for (; IS_WHITESPACE(c); c = *(++*walk))
        ;
    return c;
}

static void skip_constant(char **walk)
{
    char c = **walk;

    if (IS_CONSTANT_QUALIFIER(c))
        c = *(++*walk);

    for (; IS_DIGIT(c); c = *(++*walk))
        ;
}

static expression_t *parse_constant(char **walk) {
    expression_t *constant;
    next_char(walk);
    constant = new_constant(atoi(*walk));
    skip_constant(walk);
    return constant;
}

static expression_t *parse_computation(char **walk, char terminator)
{
    char c = **walk;
    operator_t op = OP_NONE;
    expression_t *computation = NULL, *sub_computation;

    while ((c = next_char(walk)) != terminator) {
        if (computation == NULL) {
            if (c == '(') {
                (*walk)++;
                computation = parse_computation(walk, ')');
                computation->is_grouped = true;
            } else {
                computation = parse_constant(walk);
            }
            continue;
        }

        if (op == OP_NONE) {
            op = parse_operator(walk);
            continue;
        }

        if (c == '(') {
            (*walk)++;
            sub_computation = parse_computation(walk, ')');
            sub_computation->is_grouped = true;
        } else {
            sub_computation = parse_constant(walk);
        }
        if ((computation->type == TYPE_COMPUTATION) && !computation->is_grouped &&
            (OP_IS_STRONGER(op, computation->content.op))) {
            sub_computation = new_computation(op, computation->right, sub_computation);
            computation->right = sub_computation;
        } else {
            computation = new_computation(op, computation, sub_computation);
        }
        op = OP_NONE;
    }
    (*walk)++;

    return computation;
}

static expression_list_t *push_expression(expression_list_t *list,
                                            expression_t *expression)
{
    expression_list_t *element = malloc(sizeof(expression_list_t));
    element->expression = expression;
    element->next = list;
    return element;
}

static script_region_t *get_script_region(char *region_name)
{
    script_region_t *walk = script_regions_list->head;
    for (; walk != NULL; walk = walk->next) {
        if (strcmp(region_name, walk->name) == 0)
            return walk;
    }
    return NULL;
}

static script_region_t *parse_annotation(char **walk)
{
    char region_name[256];
    script_region_t *region;

    if (strncmp(*walk, "@end", 4) == 0) {
        script_regions_list->active_region = NULL;
        return NULL;
    }

    if (!SSCANF(*walk, "@begin(%[-A-Za-z_])\n", region_name, 256)) {
        printf("Parse error on annotation: '%s'. Exiting now.\n", *walk);
        return NULL; //exit(1);
    }

    region = get_script_region(region_name);
    if (region == NULL) {
        char *new_region_name = malloc(sizeof(char) * 256);
        STRCPY(new_region_name, region_name);

        region = malloc(sizeof(script_region_t));
        region->id = script_regions_list->region_index++;
        region->name = new_region_name;
        region->next = NULL;
        if (script_regions_list->head == NULL) {
            script_regions_list->head = script_regions_list->tail = region;
        } else {
            script_regions_list->tail->next = region;
            script_regions_list->tail = region;
        }
    }

    script_regions_list->active_region = region;
    return region;
}

/*** Computation ***/

static void compute_expression(expression_t *expression)
{
    if (expression->type == TYPE_CONSTANT) {
        expression->result = expression->content.value;
        return;
    }

    compute_expression(expression->left);
    compute_expression(expression->right);
    switch (expression->content.op) {
        case OP_ADDITION:
            expression->result = expression->left->result + expression->right->result;
            break;
        case OP_SUBTRACTION:
            expression->result = expression->left->result - expression->right->result;
            break;
        case OP_MULTIPLICATION:
            expression->result = expression->left->result * expression->right->result;
            break;
        case OP_DIVISION:
            expression->result = expression->left->result / expression->right->result;
            break;
        case OP_MODULO:
            expression->result = expression->left->result % expression->right->result;
            break;
    }
}

static void compute_expressions(expression_list_t *list)
{
    expression_list_t *walk = list;
    for (; walk != NULL; walk = walk->next) {
        START_REGION_COUNTER(walk->expression);
        compute_expression(walk->expression);
        STOP_REGION_COUNTER(walk->expression);
    }
}

/*** Printing ***/

static void print_operator(operator_t op) {
    switch (op) {
        case OP_ADDITION:
            printf(" + ");
            break;
        case OP_SUBTRACTION:
            printf(" - ");
            break;
        case OP_MULTIPLICATION:
            printf(" * ");
            break;
        case OP_DIVISION:
            printf(" / ");
            break;
        case OP_MODULO:
            printf(" %% ");
            break;
    }
}

static void print_expression(expression_t *expression) {
    switch (expression->type) {
        case TYPE_CONSTANT:
            printf("%d", expression->content.value);
            break;
        case TYPE_COMPUTATION:
            printf("(");
            print_expression(expression->left);
            print_operator(expression->content.op);
            print_expression(expression->right);
            printf(")");
            break;
    }
}

static void print_expression_list(expression_list_t *list)
{
    expression_list_t *walk = list;
    for (; walk != NULL; walk = walk->next) {
        START_REGION_COUNTER(walk->expression);

        print_expression(walk->expression);
        printf(" = %d", walk->expression->result);

        if (walk->expression->region != NULL)
            printf(" (@%s)", walk->expression->region->name);

        printf("\n");

        STOP_REGION_COUNTER(walk->expression);
    }
}

/*** Main ***/

int main(int argc, const char *argv[])
{
    char input_buffer[INPUT_BUFFER_LENGTH];
    const char *input_filename;
    FILE *input_file;
    expression_list_t *computations = NULL;
    uint computation_count = 0;

    if (argc != 2) {
        printf("Usage: calculator <input-file>\n");
        return 1;
    }

    input_filename = argv[1];
    FOPEN(input_file, input_filename, "r");
    if (input_file == NULL) {
        printf("Failed to open input file '%s'. Exiting now.\n", input_filename);
        return 1;
    }

    script_regions_list = malloc(sizeof(script_regions_t));
    memset(script_regions_list, 0, sizeof(script_regions_t));
    script_regions_list->region_index = 10;

    BB_REGION_TEST_MANY_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

    BB_REGION_ANNOTATE_INIT_COUNTER(3, "parsing computations");
    BB_REGION_ANNOTATE_INIT_COUNTER(1, "computing expressions");
    BB_REGION_ANNOTATE_INIT_COUNTER(2, "printing expressions");

    BB_REGION_ANNOTATE_START_COUNTER(3);
    while (fgets(input_buffer, INPUT_BUFFER_LENGTH, input_file) != NULL) {
        char *walk = input_buffer;
        if (next_char(&walk) == '@') {
            script_region_t *region = parse_annotation(&walk);
            if (region != NULL)
                BB_REGION_ANNOTATE_INIT_COUNTER(region->id, region->name);
        } else {
            computations = push_expression(computations, parse_computation(&walk, '\n'));
            computation_count++;
        }
    }
    BB_REGION_ANNOTATE_STOP_COUNTER(3);

    printf("Loaded %d computations.\n", computation_count);

    BB_REGION_ANNOTATE_START_COUNTER(1);
    compute_expressions(computations);
    BB_REGION_ANNOTATE_STOP_COUNTER(1);

    printf("Evaluated %d computations.\n", computation_count);

    BB_REGION_ANNOTATE_START_COUNTER(2);
    print_expression_list(computations);
    BB_REGION_ANNOTATE_STOP_COUNTER(2);

    // thread per n computations

    // track bb count and mems for:
    // + total parsing
    // + each computation thread
    // + printing

    return 0;
}
