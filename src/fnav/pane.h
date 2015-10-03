#ifndef FN_GUI_PANE_H
#define FN_GUI_PANE_H

#include "fnav/cntlr.h"
typedef struct CntlrNode CntlrNode;

struct CntlrNode {
  CntlrNode *prev;
  CntlrNode *next;
  Cntlr *cntlr;
};

struct Pane {
  CntlrNode *clist;
  CntlrNode *focus;
  Window *window;
};


void pane_init(Pane *pane);
void pane_add(Pane *pane, Cntlr *c);
void pane_input(Pane *pane, int key);
void pane_req_draw(fn_buf *buf, argv_callback cb);
#endif
