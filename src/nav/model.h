#ifndef FN_MODEL_H
#define FN_MODEL_H

#include "nav/table.h"

typedef struct fn_line fn_line;
typedef struct {
  int sort_type; //sort type flag
  int sort_rev;  //reversed sort flag
} sort_t;

void model_init(fn_handle *hndl);
void model_cleanup(fn_handle *hndl);
void model_open(fn_handle *hndl);
void model_close(fn_handle *hndl);
bool model_blocking(fn_handle *hndl);
void model_inherit(fn_handle *hndl);

void model_ch_focus(fn_handle *);
void model_sort(Model *m, sort_t);
void model_flush(fn_handle *, bool);
void model_recv(Model *m);
void refind_line(Model *m);

void model_read_entry(Model *m, fn_lis *lis, ventry *head);
void model_null_entry(Model *m, fn_lis *lis);
void model_full_entry(Model *m, fn_lis *lis);

char* model_str_line(Model *m, int index);
void* model_fld_line(Model *m, const char *fld, int index);
void* model_curs_value(Model *m, const char *fld);
fn_rec* model_rec_line(Model *m, int index);

void model_clear_filter(Model *m);
void model_filter_line(Model *m, int);

void model_set_curs(Model *m, int index);
int model_count(Model *m);
char* model_str_expansion(char* val, char *key);

#endif
