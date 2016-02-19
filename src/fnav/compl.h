#ifndef FN_COMPL_H
#define FN_COMPL_H

#include "fnav/cmd.h"
#include "fnav/lib/uthash.h"

typedef struct {
  char *key;
  int colcount;
  char *columns;
} compl_item;

typedef struct {
  int rowcount;
  int matchcount;
  char comp_type;       /* COMPL_STATIC, COMPL_DYNAMIC */
  compl_item **rows;
  compl_item **matches;
} fn_compl;

typedef struct fn_context fn_context;
struct fn_context {
  char *key;
  char *comp;
  char *type;
  fn_compl *cmpl;
  int argc;
  fn_context **params;
  UT_hash_handle hh;
};

typedef void (*compl_genfn)(List *args);

typedef struct {
  char *key;
  compl_genfn gen;
  UT_hash_handle hh;
} compl_entry;

#define COMPL_STATIC  0
#define COMPL_DYNAMIC 1

void compl_init();
void compl_cleanup();
void compl_add_context(char *);
void compl_add_arg(const char *, char *);

fn_context* context_start();
void compl_update(fn_context *cx, const char *);
void compl_build(fn_context *cx, List *args);
void compl_destroy(fn_context *cx);

void compl_new(int size, int dynamic);
void compl_delete(fn_compl *cmpl);
void compl_set_index(int idx, int count, char *, char *fmt, ...);

fn_compl* compl_match_index(int idx);
fn_context* find_context(fn_context *cx, char *);
bool compl_isdynamic(fn_context *cx);

#endif
