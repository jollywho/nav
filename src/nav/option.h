#ifndef FN_OPTION_H
#define FN_OPTION_H

#include "nav/nav.h"
#include "nav/lib/uthash.h"
#include "nav/lib/utarray.h"
#include "nav/plugins/op/op.h"

extern char *p_cp;          /* 'copy      cmd' */
extern char *p_mv;          /* 'move      cmd' */
extern char *p_rm;          /* 'remove    cmd' */
extern char *p_xc;          /* 'clipboard cmd' */

typedef struct fn_group fn_group;
struct fn_group {
  UT_hash_handle hh;
  char *key;
  short colorpair;
  Op_group *opgrp; /* make array if support > 1 binding per group */
};

typedef struct fn_syn fn_syn;
struct fn_syn {
  UT_hash_handle hh;
  char *key;
  fn_group *group;
};

typedef char* (*fld_cb)(void *, const char *);
typedef struct {
  UT_hash_handle hh;
  char *key;
  char *var;
  void *owner;
  fld_cb cb;
} fn_fldvar;

typedef struct {
  UT_hash_handle hh;
  char *key;
  char *var;
} fn_var;

typedef struct {
  UT_hash_handle hh;
  UT_array *lines;
  fn_var *locals;
  int argc;
  char **argv;
  char *key;
} fn_func;

enum nv_color_group {
  BUF_SEL_ACTIVE,
  BUF_SEL_INACTIVE,
  COMPL_SELECTED,
  BUF_TEXT,
  BUF_DIR,
  BUF_SZ,
  OVERLAY_SEP,
  OVERLAY_LINE,
  OVERLAY_BUFNO,
  OVERLAY_ACTIVE,
  OVERLAY_ARGS,
  OVERLAY_INACTIVE,
  OVERLAY_TEXTINACTIVE,
  OVERLAY_PROGRESS,
  COMPL_TEXT,
  MSG_ERROR,
  MSG_MESSAGE,
  MSG_ASK,
};

void option_init();
void option_cleanup();
void set_color(fn_group *, int, int);
short opt_color(enum nv_color_group color);

fn_group* set_group(const char *);
fn_group* get_group(const char *);
void set_syn(fn_syn *);
fn_syn* get_syn(const char *);
int get_syn_colpair(const char *);

void set_fldvar(void *, const char *, fld_cb);
void set_var(fn_var *, fn_func *);
char* opt_var(const char *, fn_func *);
void set_func(fn_func *);
void clear_locals(fn_func *);
fn_func* opt_func(const char *);
void set_opt(const char *, const char *);
char* get_opt_str(const char *);
uint get_opt_uint(const char *);
int get_opt_int(const char *);
void options_list(List *);
void groups_list(List *);

#endif
