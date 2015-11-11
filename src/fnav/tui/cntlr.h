#ifndef FN_TUI_CNTLR_H
#define FN_TUI_CNTLR_H

#include "fnav/fnav.h"

typedef void (*argv_callback)(void **argv);
typedef struct Window Window;
typedef struct Cntlr Cntlr;
typedef struct fn_tbl fn_tbl;
typedef struct Buffer Buffer;
typedef struct fn_handle fn_handle;
typedef struct Model Model;
typedef struct HookHandler HookHandler;

struct fn_handle {
  Buffer *buf;
  Model *model;
  String tn;      // listening table name
  String key;     // listening value
  String key_fld; // listening field
  String fname;   // filter field
};

struct Cntlr {
  String name;
  fn_handle *hndl;
  void *top;
  void (*_cancel)(Cntlr *cntlr);
  int  (*_input)(Cntlr *cntlr, int key);
  HookHandler *event_hooks;
};

typedef struct {
  int lnum;    /* line number */
  int col;     /* column number */
} pos_T;

typedef Cntlr* (*cntlr_open_cb)(Buffer *b);
typedef void (*cntlr_close_cb)(Cntlr *cntlr);
void cntlr_load(String name, cntlr_open_cb open_cb, cntlr_close_cb close_cb);
Cntlr* cntlr_open(String name, Buffer *buf);
void cntlr_close(Cntlr *cntlr);
int cntlr_isloaded(String name);

#endif
