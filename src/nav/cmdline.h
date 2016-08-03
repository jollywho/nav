#ifndef NV_CMDLINE_H
#define NV_CMDLINE_H

#include "nav.h"
#include "nav/plugins/plugin.h"
#include "nav/lib/queue.h"
#include "nav/lib/utarray.h"

typedef struct {
  char v_type;              /* see below: VAR_NUMBER, VAR_STRING, etc. */
  union {
    char    *v_string;      /* string value (can be NULL!) */
    Pair    *v_pair;        /* pair value (can be NULL!)   */
    List    *v_list;        /* list value (can be NULL!)   */
    Dict    *v_dict;        /* dict value (can be NULL!)   */
  } vval;
} typ_T;

#define TOKENCHARS ".~!/;:|<>,[]{}() "

#define VAR_NUMBER  1       /* "v_number" is used          */
#define VAR_STRING  2       /* "v_string" is used          */
#define VAR_PAIR    3       /* "v_pair" is used            */
#define VAR_LIST    4       /* "v_list" is used            */

struct Token {
  bool quoted;
  typ_T var;
  int start;                /* start pos of token in line */
  int end;                  /* start pos of token in line */
};

struct Pair {
  Token key;
  Token value;
  bool scope;
};

struct List {
  UT_array *items;
};

struct Cmdret {
  int type;           //cmd type: OUTPUT, BUFFER, STRING, etc
  union {
    int   v_int;      //integer value
    char *v_str;      //string  value
  } val;
};

struct Cmdstr {
  QUEUE stack;
  Cmdret ret;          //return value
  Token args;
  bool bar;            /* '|' cmd separator */
  bool rev;            /* reverse flag */
  bool exec;           //exec flag
  int st;              //start pos of cmdstr
  int ed;              //end   pos of cmdstr
  int idx;             //index in cmdline
  Cmdstr *caller;      //used for returning values
  UT_array *chlds;     //list of cmdstr subexpressions
};

struct Cmdline {
  QUEUE refs;          //queue of token refs to avoid recursive cleanup
  bool cont;           //line continuation
  bool err;
  int len;
  int lvl;             //subexpression level
  UT_array *cmds;      //list of cmdstr
  UT_array *tokens;    //list of tokens
  UT_array *vars;      //list of vars
  UT_array *arys;      //list of arys
  char *line;          //the raw string being built upon
};

void cmdline_build(Cmdline *cmdline, char *line);
void cmdline_build_tokens(Cmdline *cmdline, char *line);
void cmdline_req_run(Cmdstr *caller, Cmdline *cmdline);
void cmdline_cleanup(Cmdline *cmdline);

Token* cmdline_tokbtwn(Cmdline *cmdline, int st, int ed);
Token* cmdline_tokindex(Cmdline *cmdline, int idx);
Token* cmdline_last(Cmdline *cmdline);
char* cmdline_line_from(Cmdline *cmdline, int idx);
char* cmdline_line_after(Cmdline *cmdline, int idx);
char* cmdline_cont_line(Cmdline *cmdline);
Cmdstr* cmdline_getcmd(Cmdline *cmdline);
List* cmdline_lst(Cmdline *cmd);
int cmdline_can_exec(Cmdstr *cmd, char *line);

void* token_val(Token *token, char v_type);
void* list_arg(List *lst, int argc, char v_type);
void* tok_arg(List *lst, int argc);
int str_num(const char *, int *);
int str_tfmt(const char *str, char *fmt, void *tmp);
Token* container_elem(Token *ary, List *args, int idx, int n);
char* container_str(char *line, Token *elem);
char* repl_elem(char *line, char *expr, List *args, int fst, int lst);

#endif
