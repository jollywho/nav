#ifndef FN_COMPL_H
#define FN_COMPL_H

#include "fnav/cmd.h"
#include "fnav/lib/uthash.h"

typedef struct {
  String key;
  int colcount;
  String *columns;
} compl_item;

typedef struct {
  int rowcount;
  compl_item **rows;
} fn_compl;

typedef struct fn_context fn_context;
struct fn_context {
  String key;
  String comp;
  fn_compl *cmpl;
  fn_compl *matches;
  int argc;
  fn_context **params;
  UT_hash_handle hh;
};

typedef void (*compl_genfn)(String line);
typedef struct {
  String key;
  compl_genfn gen;
  UT_hash_handle hh;
} compl_entry;

void compl_init();
void compl_add_context(String fmt_compl);

fn_context* context_start();
void compl_build(fn_context *cx, String line);

void compl_new(int size);
void compl_set_index(int idx, String key, int colcount, String *cols);

fn_compl* compl_match_index(int idx);

#endif
