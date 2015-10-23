#ifndef FN_MODEL_H
#define FN_MODEL_H

#include "fnav/table.h"

typedef struct fn_line fn_line;

void model_init(fn_handle *hndl);
void model_open(fn_handle *hndl);
void model_close(fn_handle *hndl);
void model_read_entry(Model *m, fn_lis *lis);
void model_read_stream(void **arg);
fn_line* model_req_line(Model *m, int index);

#endif
