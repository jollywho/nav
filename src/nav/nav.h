#ifndef NV_NAV_H
#define NV_NAV_H

#define STR_(x) #x
#define STR(x) STR_(x)

#ifndef NAV_VERSION
#define NAV_VERSION "unknown"
#define NAV_DATE    "unknown"
#endif
#ifndef NAV_LONG_VERSION
#define NAV_LONG_VERSION "NAV-"NAV_VERSION
#endif

typedef struct Token Token;
typedef struct Pair Pair;
typedef struct Dict Dict;
typedef struct List List;
typedef struct Scope Scope;
typedef struct Cmdstr Cmdstr;
typedef struct Cmdline Cmdline;
typedef struct Cmdret Cmdret;

#include <stdbool.h>
#include "nav/macros.h"

#endif
