#ifndef NV_OPTION_H
#define NV_OPTION_H

#include "nav/nav.h"
#include "nav/lib/uthash.h"
#include "nav/lib/utarray.h"
#include "nav/plugins/op/op.h"

extern char *p_rm;          /* 'remove    cmd' */
extern char *p_xc;          /* 'clipboard cmd' */

typedef struct nv_group nv_group;
struct nv_group {
  UT_hash_handle hh;
  char *key;
  short colorpair;
  Op_group *opgrp; /* make array if support > 1 binding per group */
};

typedef struct nv_syn nv_syn;
struct nv_syn {
  UT_hash_handle hh;
  char *key;
  nv_group *group;
};

typedef struct {
  UT_hash_handle hh;
  char *key;
  char *var;
} nv_var;

typedef struct {
  UT_hash_handle hh;
  UT_array *lines;    //saved lines as string
  nv_var *locals;     //local vars
  int argc;
  char **argv;
  char *key;
} nv_func;

enum nv_color_group {
  BUF_SEL_ACTIVE,
  BUF_SEL_INACTIVE,
  COMPL_SELECTED,
  BUF_TEXT,
  BUF_DIR,
  BUF_SZ,
  BUF_STDOUT,
  BUF_STDERR,
  OVERLAY_SEP,
  OVERLAY_LINE,
  OVERLAY_BUFNO,
  OVERLAY_ACTIVE,
  OVERLAY_ARGS,
  OVERLAY_INACTIVE,
  OVERLAY_TEXTINACTIVE,
  OVERLAY_PROGRESS,
  OVERLAY_FILTER,
  COMPL_TEXT,
  COMPL_PARAM,
  MSG_ERROR,
  MSG_MESSAGE,
  MSG_ASK,
};

void option_init();
void option_cleanup();
void set_color(nv_group *, int, int);
short opt_color(enum nv_color_group color);

nv_group* set_group(const char *);
nv_group* get_group(const char *);
void set_syn(nv_syn *);
nv_syn* get_syn(const char *);
int get_syn_colpair(const char *);

void set_var(nv_var *, nv_func *);
char* opt_var(Token *, nv_func *);
void set_func(nv_func *);
void clear_locals(nv_func *);
nv_func* opt_func(const char *);
void set_opt(const char *, const char *);
char* get_opt_str(const char *);
uint get_opt_uint(const char *);
int get_opt_int(const char *);
void options_list(List *);
void groups_list(List *);

#endif
