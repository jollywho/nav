#ifndef FN_TUI_FM_CNTLR_H
#define FN_TUI_FM_CNTLR_H

#include "fnav/tui/cntlr.h"
#include "fnav/event/fs.h"

typedef struct FM_cntlr FM_cntlr;
typedef struct FM_mark FM_mark;

struct FM_cntlr {
  Cntlr base;
  int op_count;
  int mo_count;
  String cur_dir;
  fn_fs *fs;
};

struct FM_mark {
  String key;
  String path;
  UT_hash_handle hh;
};

void fm_init();
void fm_cleanup();
Cntlr* fm_new(Buffer *buf);
void fm_delete(Cntlr *cntlr);

void fm_req_dir(Cntlr *cntlr, String path);
String fm_cur_dir(Cntlr *cntlr);

void fm_mark_dir(Cntlr *cntlr, String label);
void mark_list(List *args);
void marklbl_list(List *args);

#endif
