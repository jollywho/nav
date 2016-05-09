#ifndef FN_COMPL_H
#define FN_COMPL_H

#include "nav/cmd.h"
#include "nav/lib/uthash.h"

typedef struct {
  char *key;        //key value
  int colcount;     //count of columns; (supports 0 or 1)
  char *columns;    //column string
} compl_item;

typedef struct {
  int rowcount;
  int matchcount;
  char comp_type;       /* COMPL_STATIC, COMPL_DYNAMIC */
  compl_item **rows;
  UT_array *matches;
  int invalid_pos;
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

#define COMPL_STATIC  0   //list remains the same
#define COMPL_DYNAMIC 1   //list is regenerated

void compl_init();
void compl_cleanup();
void compl_add_context(char *);
void compl_add_arg(const char *, char *);

fn_context* context_start();
void compl_update(fn_context *cx, char *line);
void compl_build(fn_context *cx, List *args);
void compl_destroy(fn_context *cx);

void compl_new(int size, int dynamic);
void compl_delete(fn_compl *cmpl);
void compl_set_key(int idx, char *fmt, ...);
void compl_set_col(int idx, char *fmt, ...);

fn_context* find_context(fn_context *cx, char *);
bool compl_isdynamic(fn_context *cx);
compl_item* compl_idx_match(fn_compl *cmpl, int idx);
void compl_invalidate(fn_context *cx, int pos);
bool compl_validate(fn_context *cx, int pos);

#endif
