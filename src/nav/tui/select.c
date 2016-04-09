#include <malloc.h>
#include "nav/tui/select.h"
#include "nav/log.h"

typedef struct {
  int orgn_lnum;
  int orgn_index;
  int *lines;
  int max;
  int count;
  bool enabled;
  bool active;
  bool aditv;
} Select;

static Select sel;

void select_toggle(int lnum, int index, int max)
{
  log_msg("SELECT", "toggle");
  sel.enabled = !sel.enabled;
  if (!sel.enabled)
    return;
  if (!sel.active) {
    sel.lines = calloc(max, sizeof(int));
    sel.count = 0;
  }

  int idx = lnum + index;
  sel.aditv = !sel.lines[idx];

  sel.max = max;
  sel.active = true;
  sel.orgn_lnum = lnum;
  sel.orgn_index = index;
  sel.lines[idx] = 1;
}

void select_clear()
{
  if (sel.lines)
    free(sel.lines);
  sel.lines = NULL;
  sel.enabled = false;
  sel.active = false;
  sel.count = 0;
}

bool select_active()
{
  return sel.active;
}

int select_count()
{
  return sel.count;
}

void select_enter(int idx)
{
  if (!sel.enabled || !select_active())
    return;

  bool enable = sel.aditv;
  int orgn = sel.orgn_lnum + sel.orgn_index;

  int st = MIN(idx, orgn);
  int ed = MAX(idx, orgn);
  sel.count = ed - st;

  for (int i = 0; i < st; i++)
    sel.lines[i] = !enable;
  for (int i = st; i < ed; i++)
    sel.lines[i] = enable;
  for (int i = ed; i < sel.max; i++)
    sel.lines[i] = !enable;
}

bool select_alt_origin(int *lnum, int *index)
{
  if (!select_active())
    return false;

  int lnum_was   = sel.orgn_lnum;
  int index_was  = sel.orgn_index;
  sel.orgn_lnum  = *lnum;
  sel.orgn_index = *index;
  *lnum  = lnum_was;
  *index = index_was;
  return true;
}

bool select_has_line(int idx)
{
  if (!select_active())
    return false;
  return (sel.lines[idx]);
}
