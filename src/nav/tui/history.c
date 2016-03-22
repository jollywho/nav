#include <string.h>

#include "nav/lib/sys_queue.h"
#include "nav/tui/history.h"
#include "nav/tui/ex_cmd.h"
#include "nav/log.h"
#include "nav/cmdline.h"

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

static fn_hist *hist_cmds;
static fn_hist *hist_regs;
static fn_hist *cur;

static fn_hist* hist_new()
{
  fn_hist *hst = malloc(sizeof(fn_hist));
  memset(hst, 0, sizeof(fn_hist));
  TAILQ_INIT(&hst->p);
  return hst;
}

static void hist_delete(fn_hist* hst)
{
  log_msg("HIST", "hist_delete");
  while (!TAILQ_EMPTY(&hst->p)) {
    hist_item *it = TAILQ_FIRST(&hst->p);
    TAILQ_REMOVE(&hst->p, it, ent);
    free(it->line);
    free(it);
  }
  free(hst);
}

void hist_init()
{
  hist_cmds = hist_new();
  hist_regs = hist_new();
}

void hist_cleanup()
{
  hist_delete(hist_cmds);
  hist_delete(hist_regs);
}

void hist_set_state(int state)
{
  if (state == EX_CMD_STATE)
    cur = hist_cmds;
  else
    cur = hist_regs;
}

void hist_pop()
{
  log_msg("HIST", "hist_pop");
  hist_item *last = TAILQ_LAST(&cur->p, cont);
  TAILQ_REMOVE(&cur->p, last, ent);
  free(last->line);
  free(last);
  cur->count--;
}

void hist_push(int state, Cmdline *cmd)
{
  log_msg("HIST", "hist_push");
  hist_set_state(state);
  cur->cmd = cmd;
  hist_item *item = malloc(sizeof(hist_item));
  cur->count++;
  item->line = strdup("");
  cur->cur = item;
  TAILQ_INSERT_TAIL(&cur->p, item, ent);
  //if (count > SETTING_MAX)
  //  pop from head
}

void hist_save()
{
  log_msg("HIST", "hist_save");
  if (!cur->cmd->line)
    return;
  hist_item *last = TAILQ_LAST(&cur->p, cont);
  hist_item *prev = TAILQ_PREV(last, cont, ent);

  if (prev && !strcmp(prev->line, cur->cmd->line))
    return hist_pop();
  if (cur->cmd->tokens && utarray_len(cur->cmd->tokens) < 1)
    return hist_pop();

  free(last->line);
  last->line = strdup(cur->cmd->line);
}

const char* hist_prev()
{
  log_msg("HIST", "hist_prev");
  hist_item *item = TAILQ_PREV(cur->cur, cont, ent);
  if (!item)
    return NULL;
  cur->cur = item;
  return item->line;
}

const char* hist_next()
{
  log_msg("HIST", "hist_next");
  hist_item *item = TAILQ_NEXT(cur->cur, ent);
  if (!item)
    return NULL;
  cur->cur = item;
  return item->line;
}

void hist_insert(int state, char *line)
{
  // get type based on line[0] from ex_cmd
  //hist_item *item = malloc(sizeof(hist_item));
  //hst->count++;
  //item->line = strdup(line);
  //TAILQ_INSERT_TAIL(&hst->p, item, ent);
}
