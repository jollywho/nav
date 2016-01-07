#include <string.h>

#include "fnav/lib/sys_queue.h"
#include "fnav/tui/history.h"
#include "fnav/log.h"
#include "fnav/cmdline.h"

typedef struct hist_item hist_item;
struct hist_item {
  String line;
  TAILQ_ENTRY(hist_item) ent;
};

struct fn_hist {
  TAILQ_HEAD(cont, hist_item) p;
  hist_item *cur;
  int count;
  Cmdline *cmd;
};

fn_hist* hist_new()
{
  fn_hist *hst = malloc(sizeof(fn_hist));
  TAILQ_INIT(&hst->p);
  hst->cur = NULL;
  hst->count = 0;
  return hst;
}

void hist_pop(fn_hist *hst)
{
  hist_item *last = TAILQ_LAST(&hst->p, cont);
  TAILQ_REMOVE(&hst->p, last, ent);
  free(last);
  hst->count--;
}

void hist_push(fn_hist *hst, Cmdline *cmd)
{
  log_msg("HIST", "hist_push");
  hst->cmd = cmd;
  hist_item *item = malloc(sizeof(hist_item));
  hst->count++;
  item->line = cmd->line;
  hst->cur = item;
  TAILQ_INSERT_TAIL(&hst->p, item, ent);
  //if (count > SETTING_MAX)
  //  pop from head
}

void hist_save(fn_hist *hst)
{
  hist_item *last = TAILQ_LAST(&hst->p, cont);
  if (utarray_len(hst->cmd->tokens) > 0) {
    last->line = strdup(hst->cmd->line);
  }
  else
    hist_pop(hst);
}

String hist_prev(fn_hist *hst)
{
  log_msg("HIST", "hist_prev");
  hist_item *item = TAILQ_PREV(hst->cur, cont, ent);
  if (!item)
    return NULL;
  hst->cur = item;
  return item->line;
}

String hist_next(fn_hist *hst)
{
  log_msg("HIST", "hist_next");
  hist_item *item = TAILQ_NEXT(hst->cur, ent);
  if (!item)
    return NULL;
  hst->cur = item;
  return item->line;
}
