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
  int matchcount;
  char comp_type;       /* COMPL_STATIC, COMPL_DYNAMIC */
  compl_item **rows;
  compl_item **matches;
} fn_compl;

#define COMPL_STATIC  0
#define COMPL_DYNAMIC 1

typedef struct fn_context fn_context;
struct fn_context {
  String key;
  fn_context *sub;
  String comp;
  String type;
  fn_compl *cmpl;
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
void compl_cleanup();
void compl_add_context(String fmt_compl);

fn_context* context_start();
void compl_update(fn_context *cx, String line);
void compl_build(fn_context *cx, String line);
void compl_destroy(fn_context *cx);

void compl_new(int size, int dynamic);
void compl_delete(fn_compl *cmpl);
void compl_set_index(int idx, String key, int colcount, String *cols);

fn_compl* compl_match_index(int idx);
fn_context* find_context(fn_context *cx, String name);
void compl_force_cur();

#endif
