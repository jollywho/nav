#ifndef FN_CMDLINE_H
#define FN_CMDLINE_H

#include "fnav.h"
#include "fnav/lib/queue.h"
#include "fnav/lib/utarray.h"

typedef struct Token Token;
typedef struct Pair Pair;
typedef struct Dict Dict;
typedef struct List List;
typedef struct Cmdstr Cmdstr;
typedef struct Cmdline Cmdline;

typedef struct {
  char v_type;              /* see below: VAR_NUMBER, VAR_STRING, etc. */
  union {
    int      v_number;      /* number value                */
    char    *v_string;      /* string value (can be NULL!) */
    Pair    *v_pair;        /* pair value (can be NULL!)   */
    List    *v_list;        /* list value (can be NULL!)   */
    Dict    *v_dict;        /* dict value (can be NULL!)   */
  } vval;
} typ_T;

#define VAR_NUMBER  1       /* "v_number" is used          */
#define VAR_STRING  2       /* "v_string" is used          */
#define VAR_PAIR    3       /* "v_pair" is used            */
#define VAR_LIST    4       /* "v_list" is used            */
#define VAR_DICT    5       /* "v_dict" is used            */

struct Token {
  int block;                /* block level flag           */
  int start;                /* start pos of token in line */
  int end;                  /* start pos of token in line */
  char esc;                 /* escape char for token      */
  typ_T var;
};

struct Pair {
  Token *key;
  Token *value;
};

struct List {
  UT_array *items;
};

struct Dict {
  UT_array *items;
};

struct Cmdstr {
  char pipet;               /* pipe flag types */
  QUEUE stack;
  Token args;
};

#define PIPE_CMD     1
#define PIPE_CNTLR   2

struct Cmdline {
  UT_array *cmds;
  UT_array *tokens;
  String line;
};

void cmdline_init(Cmdline *cmdline, int size);
void cmdline_cleanup(Cmdline *cmdline);
void cmdline_build(Cmdline *cmdline);
void cmdline_req_run(Cmdline *cmdline);

int  cmdline_prev_word(Cmdline *cmdline, int pos);

#endif
