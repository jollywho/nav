#ifndef FN_COMPL_H
#define FN_COMPL_H

#include "fnav.h"
#include "fnav/lib/uthash.h"

typedef struct {
  String key;
  String *columns;
} compl_item;

typedef struct {
  String name;
  compl_item *rows;
} compl_list;

typedef compl_list* (*compl_genfn)();
typedef struct {
  String key;
  compl_genfn gen;
  UT_hash_handle hh;
} compl_entry;

void compl_init();
compl_list* compl_of(String name);

#endif
