#ifndef FN_MODEL_H
#define FN_MODEL_H

#include "fnav/table.h"

typedef struct fn_line fn_line;

void model_init(fn_handle *hndl);
void model_cleanup(fn_handle *hndl);
void model_open(fn_handle *hndl);
void model_close(fn_handle *hndl);
int model_blocking(fn_handle *hndl);

void model_sort(Model *m, String fld, int flags);
void model_recv(Model *m);
void model_read_entry(Model *m, fn_lis *lis, ventry *head);
size_t model_read_stream(Model *m, char *output, size_t remaining,
    bool to_buffer, bool eof);
String model_str_line(Model *m, int index);
void* model_fld_line(Model *m, String field, int index);
void* model_curs_value(Model *m, String field);
void model_set_curs(Model *m, int index);
int model_count(Model *m);

String model_str_expansion(String val);


struct fn_reg {
  String key;
  fn_rec *rec;
  UT_hash_handle hh;
};

fn_reg* reg_get(fn_handle *hndl, String reg_ch);
void reg_set(fn_handle *hndl, String reg_ch, String fld);

#endif
