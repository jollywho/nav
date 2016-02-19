#include <string.h>

#include "fnav/lib/sys_queue.h"
#include "fnav/tui/history.h"
#include "fnav/log.h"
#include "fnav/cmdline.h"

typedef struct hist_item hist_item;
struct hist_item {
  char *line;
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
  memset(hst, 0, sizeof(fn_hist));
  TAILQ_INIT(&hst->p);
  return hst;
}

void hist_delete(fn_hist* hst)
{
  log_msg("HIST", "hist_delete");
  while (!TAILQ_EMPTY(&hst->p)) {
    hist_item *it = TAILQ_FIRST(&hst->p);
    TAILQ_REMOVE(&hst->p, it, ent);
    free(it);
  }
  free(hst);
}

void hist_pop(fn_hist *hst)
{
  log_msg("HIST", "hist_pop");
  hist_item *last = TAILQ_LAST(&hst->p, cont);
  TAILQ_REMOVE(&hst->p, last, ent);
  free(last->line);
  free(last);
  hst->count--;
}

void hist_push(fn_hist *hst, Cmdline *cmd)
{
  log_msg("HIST", "hist_push");
  hst->cmd = cmd;
  hist_item *item = malloc(sizeof(hist_item));
  hst->count++;
  item->line = strdup("");
  hst->cur = item;
  TAILQ_INSERT_TAIL(&hst->p, item, ent);
  //if (count > SETTING_MAX)
  //  pop from head
}

void hist_save(fn_hist *hst)
{
  log_msg("HIST", "hist_save");
  hist_item *last = TAILQ_LAST(&hst->p, cont);

  //TODO:
  // discard if line matches the one above it
  if (hst->cmd->tokens && utarray_len(hst->cmd->tokens) < 1)
    return hist_pop(hst);

  free(last->line);
  last->line = strdup(hst->cmd->line);
}

const char* hist_prev(fn_hist *hst)
{
  log_msg("HIST", "hist_prev");
  hist_item *item = TAILQ_PREV(hst->cur, cont, ent);
  if (!item)
    return NULL;
  hst->cur = item;
  return item->line;
}

const char* hist_next(fn_hist *hst)
{
  log_msg("HIST", "hist_next");
  hist_item *item = TAILQ_NEXT(hst->cur, ent);
  if (!item)
    return NULL;
  hst->cur = item;
  return item->line;
}

void hist_insert(char *line)
{
  // get type based on line[0] from ex_cmd
}
