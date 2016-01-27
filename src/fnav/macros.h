#ifndef FN_MACROS_H
#define FN_MACROS_H

/// Calculate the length of a C array.
///
/// This should be called with a real array. Calling this with a pointer is an
/// error. A mechanism to detect many (though not all) of those errors at compile
/// time is implemented. It works by the second division producing a division by
/// zero in those cases (-Wdiv-by-zero in GCC).
#define ARRAY_SIZE(arr) ((sizeof(arr)/sizeof((arr)[0])) / \
  ((size_t)(!(sizeof(arr) % sizeof((arr)[0])))))

#define MIN(X, Y) (X < Y ? X : Y)
#define MAX(X, Y) (X > Y ? X : Y)

#define TOUPPER_ASC(c) (((c) < 'a' || (c) > 'z') ? (c) : (c) - ('a' - 'A'))
#define TOLOWER_ASC(c) (((c) < 'A' || (c) > 'Z') ? (c) : (c) + ('a' - 'A'))

#define TOGGLE_ATTR(var,win,attr)  \
  if (var) wattron(win, attr);

#define DRAW_CH(obj,win,ln,col,str,color)           \
  do {                                              \
    wattron((obj)->win, COLOR_PAIR((obj)->color));  \
    mvwaddch((obj)->win, ln, col, str);             \
    wattroff((obj)->win, COLOR_PAIR((obj)->color)); \
  } while (0)

#define DRAW_STR(obj,win,ln,col,str,color)          \
  do {                                              \
    wattron((obj)->win, COLOR_PAIR((obj)->color));  \
    mvwaddstr((obj)->win, ln, col, str);            \
    wattroff((obj)->win, COLOR_PAIR((obj)->color)); \
  } while (0)

#define SWAP_ALLOC_PTR(a,b)   \
  String _tmp_str = (b);      \
  free(a);                    \
  (a) = _tmp_str;             \

#endif
