#ifndef FN_MODEL_H
#define FN_MODEL_H

#include "fnav/table.h"

typedef struct fn_line fn_line;

void model_init(fn_handle *hndl);
void model_open(fn_handle *hndl);
void model_close(fn_handle *hndl);
void model_read_entry(Model *m, fn_lis *lis, ventry *head);
void model_read_stream(void **arg);
String model_str_line(Model *m, int index);
void* model_curs_value(Model *m, String field);
void model_set_curs(Model *m, int index);
int model_count(Model *m);

#endif
