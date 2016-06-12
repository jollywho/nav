#ifndef NV_MACROS_H
#define NV_MACROS_H

/// Calculate the length of a C array.
///
/// This should be called with a real array. Calling this with a pointer is an
/// error. A mechanism to detect many (though not all) of those errors at compile
/// time is implemented. It works by the second division producing a division by
/// zero in those cases (-Wdiv-by-zero in GCC).
#define LENGTH(arr) ((sizeof(arr)/sizeof((arr)[0])) / \
  ((size_t)(!(sizeof(arr) % sizeof((arr)[0])))))

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

#define SWAP(t, a, b) \
  do { t __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

#define TOUPPER_ASC(c) (((c) < 'a' || (c) > 'z') ? (c) : (c) - ('a' - 'A'))
#define TOLOWER_ASC(c) (((c) < 'A' || (c) > 'Z') ? (c) : (c) + ('a' - 'A'))

#define DRAW_CH(obj,win,ln,col,str,color)           \
  do {                                              \
    wattron((obj)->win, COLOR_PAIR((obj)->color));  \
    mvwaddch((obj)->win, ln, col, str);             \
    wattroff((obj)->win, COLOR_PAIR((obj)->color)); \
  } while (0)

#define SWAP_ALLOC_PTR(a,b)   \
  do {                        \
    char *_tmp_str = (b);    \
    free(a);                  \
    (a) = _tmp_str;           \
  } while (0)                 \

#define BITMASK_CHECK(x,y) (((x) & (y)) == (x))
#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)

#endif
