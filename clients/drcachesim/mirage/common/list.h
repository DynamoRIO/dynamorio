/*
 * This file is taken from the Pintos kernel.
 *
 */

#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

/* Doubly linked list.

   This implementation of a doubly linked list does not require
   use of dynamically allocated memory.  Instead, each structure
   that is a potential list element must embed a struct list_elem
   member.  All of the list functions operate on these `struct
   list_elem's.  The list_entry macro allows conversion from a
   struct list_elem back to a structure object that contains it.

   For example, suppose there is a needed for a list of `struct
   foo'.  `struct foo' should contain a `struct list_elem'
   member, like so:

      struct foo
        {
          struct list_elem elem;
          int bar;
          ...other members...
        };

   Then a list of `struct foo' can be be declared and initialized
   like so:

      struct list foo_list;

      list_init (&foo_list);

   Iteration is a typical situation where it is necessary to
   convert from a struct list_elem back to its enclosing
   structure.  Here's an example using foo_list:

      struct list_elem *e;

      for (e = list_begin (&foo_list); e != list_end (&foo_list);
           e = list_next (e))
        {
          struct foo *f = list_entry (e, struct foo, elem);
          ...do something with f...
        }

   You can find real examples of list usage throughout the
   source; for example, malloc.c, palloc.c, and thread.c in the
   threads directory all use lists.

   The interface for this list is inspired by the list<> template
   in the C++ STL.  If you're familiar with list<>, you should
   find this easy to use.  However, it should be emphasized that
   these lists do *no* type checking and can't do much other
   correctness checking.  If you screw up, it will bite you.

   Glossary of list terms:

     - "front": The first element in a list.  Undefined in an
       empty list.  Returned by list_front().

     - "back": The last element in a list.  Undefined in an empty
       list.  Returned by list_back().

     - "tail": The element figuratively just after the last
       element of a list.  Well defined even in an empty list.
       Returned by list_end().  Used as the end sentinel for an
       iteration from front to back.

     - "beginning": In a non-empty list, the front.  In an empty
       list, the tail.  Returned by list_begin().  Used as the
       starting point for an iteration from front to back.

     - "head": The element figuratively just before the first
       element of a list.  Well defined even in an empty list.
       Returned by list_rend().  Used as the end sentinel for an
       iteration from back to front.

     - "reverse beginning": In a non-empty list, the back.  In an
       empty list, the head.  Returned by list_rbegin().  Used as
       the starting point for an iteration from back to front.

     - "interior element": An element that is not the head or
       tail, that is, a real list element.  An empty list does
       not have any interior elements.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "assert.h"

/* List element. */
struct list_elem {
  struct list_elem* prev; /* Previous list element. */
  struct list_elem* next; /* Next list element. */
};

/* List. */
struct list {
  struct list_elem head; /* List head. */
  struct list_elem tail; /* List tail. */
};

/* Converts pointer to list element LIST_ELEM into a pointer to
   the structure that LIST_ELEM is embedded inside.  Supply the
   name of the outer structure STRUCT and the member name MEMBER
   of the list element.  See the big comment at the top of the
   file for an example. */
#define list_entry(LIST_ELEM, STRUCT, MEMBER)                                                      \
  ((STRUCT*)((uint8_t*)&(LIST_ELEM)->next - offsetof(STRUCT, MEMBER.next)))

/* List initialization.

   A list may be initialized by calling list_init():

       struct list my_list;
       list_init (&my_list);

   or with an initializer using LIST_INITIALIZER:

       struct list my_list = LIST_INITIALIZER (my_list); */
#define LIST_INITIALIZER(NAME)                                                                     \
  {                                                                                                \
    {NULL, &(NAME).tail}, { &(NAME).head, NULL }                                                   \
  }

void list_init(struct list*);

/* List traversal. */
struct list_elem* list_begin(struct list*);
struct list_elem* list_next(struct list_elem*);
struct list_elem* list_end(struct list*);

struct list_elem* list_rbegin(struct list*);
struct list_elem* list_prev(struct list_elem*);
struct list_elem* list_rend(struct list*);

struct list_elem* list_head(struct list*);
struct list_elem* list_tail(struct list*);

/* List insertion. */
void list_insert_before(struct list_elem*, struct list_elem*);
void list_insert_after(struct list_elem*, struct list_elem*);
void list_splice(struct list_elem* before, struct list_elem* first, struct list_elem* last);
void list_push_front(struct list*, struct list_elem*);
void list_push_back(struct list*, struct list_elem*);

/* List removal. */
struct list_elem* list_remove(struct list_elem*);
struct list_elem* list_pop_front(struct list*);
struct list_elem* list_pop_back(struct list*);

/* List elements. */
struct list_elem* list_front(struct list*);
struct list_elem* list_back(struct list*);

/* List properties. */
size_t list_size(struct list*);
bool list_empty(struct list*);

/* Miscellaneous. */
void list_reverse(struct list*);

/* Compares the value of two list elements A and B, given
   auxiliary data AUX.  Returns true if A is less than B, or
   false if A is greater than or equal to B. */
typedef bool list_less_func(const struct list_elem* a, const struct list_elem* b, void* aux);

/* Operations on lists with ordered elements. */
void list_sort(struct list*, list_less_func*, void* aux);
void list_insert_ordered(struct list*, struct list_elem*, list_less_func*, void* aux);
void list_unique(struct list*, struct list* duplicates, list_less_func*, void* aux);

/* Max and min. */
struct list_elem* list_max(struct list*, list_less_func*, void* aux);
struct list_elem* list_min(struct list*, list_less_func*, void* aux);

#endif /* lib/kernel/list.h */