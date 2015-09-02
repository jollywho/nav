#ifndef FN_MACROS_H
#define FN_MACROS_H

/// Calculate the length of a C array.
///
/// This should be called with a real array. Calling this with a pointer is an
/// error. A mechanism to detect many (though not all) of those errors at compile
/// time is implemented. It works by the second division producing a division by
/// zero in those cases (-Wdiv-by-zero in GCC).
#define ARRAY_SIZE(arr) ((sizeof(arr)/sizeof((arr)[0])) / ((size_t)(!(sizeof(arr) % sizeof((arr)[0])))))

#endif
