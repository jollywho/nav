#ifndef NV_OPTION_H
#define NV_OPTION_H

#include "nav/nav.h"
#include "nav/lib/uthash.h"
#include "nav/lib/utarray.h"
#include "nav/plugins/op/op.h"

extern char *p_rm;          /* 'remove    cmd' */
extern char *p_xc;          /* 'clipboard cmd' */

typedef struct nv_group nv_group;
typedef struct nv_module nv_module;
typedef struct nv_syn nv_syn;
typedef struct nv_block nv_block;

struct nv_group {
  UT_hash_handle hh;
  char *key;
  short colorpair;
  Op_group *opgrp; /* make array if support > 1 binding per group */
};

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
  char *key;
  int argc;
  char **argv;
  UT_array *lines;
  nv_module *module;
} nv_func;

struct nv_block {
  nv_var *vars;
  nv_func *fn;
};

struct nv_module {
  UT_hash_handle hh;
  char *key;
  char *path;
  nv_block blk;
};

enum nv_color_group {
  BUF_DIR,
  BUF_SEL_ACTIVE,
  BUF_SEL_INACTIVE,
  BUF_STDERR,
  BUF_STDOUT,
  BUF_SZ,
  BUF_TEXT,
  COMPL_PARAM,
  COMPL_SELECTED,
  COMPL_TEXT,
  MSG_ASK,
  MSG_ERROR,
  MSG_MESSAGE,
  OVERLAY_ACTIVE,
  OVERLAY_ARGS,
  OVERLAY_BUFNO,
  OVERLAY_FILTER,
  OVERLAY_INACTIVE,
  OVERLAY_LINE,
  OVERLAY_PROGRESS,
  OVERLAY_SEP,
  OVERLAY_TEXTINACTIVE,
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

void set_module(nv_module *);
nv_module* get_module(const char *);

void set_var(nv_var *, nv_block *);
char* opt_var(Token *, nv_block *);
void set_func(nv_func *, nv_block *);
void clear_block(nv_block *);
nv_func* opt_func(const char *, nv_block *);

void clear_opts(nv_option **);
void add_opt(nv_option **, char *, enum opt_type);
void set_opt(const char *, const char *);
char* get_opt_str(const char *);
uint get_opt_uint(const char *);
int get_opt_int(const char *);
void options_list();
void groups_list();

#endif
