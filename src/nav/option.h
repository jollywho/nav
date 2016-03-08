#ifndef FN_OPTION_H
#define FN_OPTION_H

#include "nav/nav.h"
#include "nav/lib/uthash.h"
#include "nav/lib/utarray.h"
#include "nav/plugins/op/op.h"

extern char *p_sh;          /* 'shell' */
extern char *p_cp;          /* 'copy   cmd' */
extern char *p_mv;          /* 'move   cmd' */
extern char *p_rm;          /* 'remove cmd' */

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

typedef struct {
  UT_hash_handle hh;
  char *key;
  char *var;
} fn_var;

typedef struct {
  UT_hash_handle hh;
  UT_array *lines;
  char *key;
} fn_func;

void option_init();
void option_cleanup();
void set_color(fn_group *grp, int fg, int bg);
short attr_color(const char *);

fn_group* set_group(const char *name);
fn_group* get_group(const char *);
void set_syn(fn_syn *syn);
fn_syn* get_syn(const char *);
int get_syn_colpair(const char *name);

void set_var(fn_var *var);
char* opt_var(const char *name);
void set_func(fn_func *fn);
fn_func* opt_func(const char *name);
void set_opt(const char *name, const char *val);
char* get_opt_str(const char *name);
void options_list(List *args);
void groups_list(List *args);

#endif
