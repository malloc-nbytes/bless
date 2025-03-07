#ifndef DYN_ARRAY_H
#define DYN_ARRAY_H

#include <stdlib.h>
#include <string.h>

#define dyn_array_type(ty, name) \
    typedef struct {             \
        ty *data;                \
        size_t len, cap;         \
    } name

#define dyn_array_init(da)       \
    do {                         \
        (da).data = malloc(sizeof(*(da).data)); \
        (da).cap = 1;            \
        (da).len = 0;            \
    } while (0)

//////////////////////////////////////////////////
// Creates a new dynamic array.
// Example:
//   dyn_array(int, int_vector);
#define dyn_array(ty, name)                                        \
    struct {                                                       \
        ty *data;                                                  \
        size_t len, cap;                                           \
    } (name) = { .data = malloc(sizeof(ty)), .cap = 1, .len = 0 };

//////////////////////////////////////////////////
// Append to a dynamic array.
// Example:
//   dyn_array(int, int_vector);
//   for (int i = 0; i < 10; ++i)
//     dyn_array_append(int_vector, i);
#define dyn_array_append(da, value)                                          \
    do {                                                                     \
        if ((da).len >= (da).cap) {                                          \
            (da).cap *= 2;                                                   \
            (da).data = realloc((da).data, (da).cap * sizeof((da).data[0])); \
        }                                                                    \
        (da).data[(da).len++] = (value);                                     \
    } while (0)

//////////////////////////////////////////////////
// Free a dynamic array.
// Example:
//   dyn_array(int, int_vector);
//   dyn_array_free(int_vector);
#define dyn_array_free(da)       \
    do {                         \
        free((da).data);         \
        (da).len = (da).cap = 0; \
    } while (0)

//////////////////////////////////////////////////
// Get an element safely at an index.
// Will panic if the element is out of bounds.
// Example:
//   dyn_array(int, int_vector);
//   dyn_array_append(int_vector, i);
//   printf("%d\n", dyn_array_at_s(int_vector));
#define dyn_array_at_s(da, i) \
    ((i) < (da).len ? (da).data[i] : (fprintf(stderr, \
    "[dyn_array error]: index %zu is out of bounds (len = %zu)\n", \
    (size_t)(i), (size_t)(da).len), exit(1), (da).data[0]))

//////////////////////////////////////////////////
// Get an element at an index.
// Example:
//   dyn_array(int, int_vector);
//   dyn_array_append(int_vector, i);
//   printf("%d\n", dyn_array_at(int_vector));
#define dyn_array_at(da, i) ((da).data[i])

//////////////////////////////////////////////////
// Clear a dynamic array.
// Example:
//   dyn_array(int, int_vector);
//   dyn_array_append(int_vector, i);
//   dyn_array_clear(int_vector);
#define dyn_array_clear(da) (da).len = 0;

#define dyn_array_rm_at(da, idx) \
    do {                                                     \
        for (size_t __i_ = (idx); __i_ < (da).len-1; ++__i_) \
            (da).data[__i_] = (da).data[__i_+1];             \
        (da).len--;                                          \
    } while (0)

#define dyn_array_explode(da) (da).data, (da).len, (da).cap

#define dyn_array_explode_mem(da) &(da).data, &(da).len, &(da).cap

#endif // DYN_ARRAY_H
