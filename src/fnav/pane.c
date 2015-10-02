
#include "fnav/event.h"
#include "fnav/loop.h"
#include "fnav/pane.h"
#include "fnav/buffer.h"
#include "fnav/fm_cntlr.h"
#include "fnav/log.h"

typedef struct CntlrNode CntlrNode;

struct CntlrNode {
  CntlrNode *prev;
  CntlrNode *next;
  Cntlr *cntlr;
};

Loop *loop;
CntlrNode *clist;
CntlrNode *focus;
bool draw_pending;

#define FC focus->cntlr

void pane_init()
{
  log_msg("INIT", "PANE");
  //loop = uv_default_loop();
  draw_pending = 0;
  clist = NULL;
  focus = NULL;
  fm_cntlr_init();
}

void pane_add(Cntlr *c)
{
  log_msg("INIT", "PANE +ADD+");
  CntlrNode *cn = malloc(sizeof(CntlrNode*));
  if (!clist) {
    cn->next = cn;
    cn->prev = cn;
    cn->cntlr = c;
    clist = cn;
    focus = cn;
  }
  else {
    cn->cntlr = c;
    cn->prev = clist->prev;
    cn->next = clist;
    clist->prev->next = cn;
    clist->prev = cn;
  }
}

static CntlrNode* pane_next() {
  return focus->next;
}

static void pane_focus(CntlrNode* cn)
{
  focus = cn;
  FC->_focus(FC);
}

void pane_input(int key)
{
  if (key == 'n') {
    pane_focus(pane_next());
  }
  FC->_input(FC, key);
}

void pane_set_dirty(fn_buf *buf)
{
  buf_allow_draw(eventloop(), buf);
  if (!draw_pending) {
    draw_pending = 1;
    buf_allow_draw(eventloop(), buf);
  }
}
