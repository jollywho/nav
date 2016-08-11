#ifndef NV_MODEL_H
#define NV_MODEL_H

#include "nav/table.h"

typedef struct nv_line nv_line;

void model_init(Handle *hndl);
void model_cleanup(Handle *hndl);
void model_open(Handle *hndl);
void model_close(Handle *hndl);
bool model_blocking(Handle *hndl);

void model_ch_focus(Handle *);
void model_set_sort(Model *m, char *, bool);
void model_sort(Model *m);
void model_flush(Handle *, bool);
void model_recv(Model *m);
void refind_line(Model *m);

void model_read_entry(Model *m, TblLis *lis, Ventry *head);
void model_null_entry(Model *m, TblLis *lis);
void model_full_entry(Model *m, TblLis *lis);

char* model_str_line(Model *m, int index);
void* model_fld_line(Model *m, const char *fld, int index);
void* model_curs_value(Model *m, const char *fld);
TblRec* model_rec_line(Model *m, int index);

void model_clear_filter(Model *m);
void model_filter_line(Model *m, int);

void model_set_curs(Model *m, int index);
int model_count(Model *m);
char* model_str_expansion(char* val, char *key);

#endif
