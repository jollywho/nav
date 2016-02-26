#ifndef FN_CMDLINE_H
#define FN_CMDLINE_H

#include "fnav.h"
#include "fnav/plugins/plugin.h"
#include "fnav/lib/queue.h"
#include "fnav/lib/utarray.h"

typedef struct {
  char v_type;              /* see below: VAR_NUMBER, VAR_STRING, etc. */
  union {
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

struct Cmdstr {
  int flag;               /* pipe flag types */
  char rev;               /* reverse flag TODO :merge with pipe flags */
  QUEUE stack;
  Token args;
  int ret_t;
  int exec;
  void *ret;
  int st;
  int ed;
  UT_array *chlds;  /* Cmdstr */
};

#define PIPE         1
#define PIPE_LEFT    2
#define PIPE_RIGHT   3

struct Cmdline {
  UT_array *cmds;
  UT_array *tokens;
  QUEUE refs;
  char *line;
};

void cmdline_init_config(Cmdline *cmdline, char *);
void cmdline_init(Cmdline *cmdline, int size);
void cmdline_cleanup(Cmdline *cmdline);
void cmdline_build(Cmdline *cmdline);
void cmdline_req_run(Cmdline *cmdline);

Token* cmdline_tokbtwn(Cmdline *cmdline, int st, int ed);
Cmdstr* cmdline_cmdbtwn(Cmdline *cmdline, int st, int ed);
Token* list_tokbtwn(List *lst, int st, int ed);
Token* cmdline_tokindex(Cmdline *cmdline, int idx);
Token* cmdline_last(Cmdline *cmdline);
char* cmdline_line_from(Cmdline *cmdline, int idx);
Cmdstr* cmdline_getcmd(Cmdline *cmdline);

void* token_val(Token *token, char v_type);
void* list_arg(List *lst, int argc, char v_type);
void* tok_arg(List *lst, int argc);
int str_num(char *, int *);

#endif
