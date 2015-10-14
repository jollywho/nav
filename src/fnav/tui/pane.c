
#include "fnav/tui/pane.h"
#include "fnav/tui/window.h"
#include "fnav/event/event.h"
#include "fnav/event/loop.h"
#include "fnav/tui/buffer.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/log.h"

#define FC focus->cntlr

void pane_init(Pane *p)
{
  log_msg("INIT", "PANE");
  p->clist = NULL;
  p->focus = NULL;
}

void pane_add(Pane *p, Cntlr *c)
{
  log_msg("INIT", "PANE +ADD+");
  CntlrNode *cn = malloc(sizeof(CntlrNode*));
  if (!p->clist) {
    cn->next = cn;
    cn->prev = cn;
    cn->cntlr = c;
    p->clist = cn;
    p->focus = cn;
  }
  else {
    cn->cntlr = c;
    cn->prev = p->clist->prev;
    cn->next = p->clist;
    p->clist->prev->next = cn;
    p->clist->prev = cn;
  }
}

void pane_remove(Pane *p, Cntlr *c)
{
  //TODO
}

static CntlrNode* pane_next(Pane *p) {
  return p->focus->next;
}

static void pane_focus(Pane *p, CntlrNode* cn)
{
  if (cn)
    p->focus = cn;
  p->FC->_focus(p->FC);
}

void pane_input(Pane *p, int key)
{
  if (key == 'n') {
    pane_focus(p, pane_next(p));
  }
  if (key == 'o')
    fm_cntlr_init(p);
  p->FC->_input(p->FC, key);
}

void pane_req_draw(fn_buf *buf, argv_callback cb)
{
  window_req_draw(buf, cb);
}
