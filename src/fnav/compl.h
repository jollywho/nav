#ifndef FN_COMPL_H
#define FN_COMPL_H

#include "fnav/cmd.h"
#include "fnav/lib/uthash.h"

typedef struct {
  String key;
  String *columns;
} compl_item;

typedef struct {
  String name;
  compl_item **rows;
} fn_compl;

typedef fn_compl* (*compl_genfn)(String line);
typedef struct {
  String key;
  compl_genfn gen;
  UT_hash_handle hh;
} compl_entry;

typedef struct fn_context fn_context;

void compl_init();
void compl_add_context(String fmt_compl);
fn_compl* compl_create(String name, String line);
fn_context* context_start();

#endif
