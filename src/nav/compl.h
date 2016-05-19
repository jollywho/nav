#ifndef FN_COMPL_H
#define FN_COMPL_H

#include "nav/cmd.h"
#include "nav/lib/uthash.h"

typedef void (*compl_genfn)(List *args);
typedef struct {
  char flag;
  char *label;
  compl_genfn gen;
} compl_param;

typedef struct {
  char *key;
  int argc;
  compl_param **params;
  UT_hash_handle hh;
} compl_context;

typedef struct {
  char *label;
  char *key;
  int colcount;     //count of columns; (supports 0 or 1)
  char *columns;    //column string
} compl_item;

typedef struct {
  int matchcount;
  char comp_type;
#define COMPL_STATIC  0   //list remains the same
#define COMPL_DYNAMIC 1   //list is regenerated
  UT_array *rows;    //compl_item
  UT_array *matches;
  int invalid_pos;
} compl_list;

void compl_init();
void compl_cleanup();

void compl_begin();
void compl_end();
bool compl_dead();

compl_list* compl_complist();

bool compl_forward(char *);
void compl_backward();
void compl_build(List *args);
void compl_update(char *line);

void compl_list_add(char *fmt, ...);
void compl_set_col(int idx, char *fmt, ...);

bool compl_isdynamic();
compl_item* compl_idx_match(int idx);
void compl_invalidate(int pos);
bool compl_validate(int pos);
int compl_last_pos();
int compl_cur_pos();

#endif
