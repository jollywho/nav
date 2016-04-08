#include <malloc.h>
#include "nav/tui/select.h"
#include "nav/log.h"

typedef struct {
  int origin;
  int *lines;
  int max;
  bool enabled;
  bool active;
  bool aditv;
} Select;

static Select sel;

void select_toggle(int cur, int max)
{
  log_msg("SELECT", "toggle");
  sel.enabled = !sel.enabled;
  if (!sel.enabled)
    return;
  if (!sel.active)
    sel.lines = calloc(max, sizeof(int));

  sel.aditv = !sel.lines[cur];

  sel.max = max;
  sel.active = true;
  sel.origin = cur;
  sel.lines[cur] = 1;
}

void select_clear()
{
  if (sel.lines)
    free(sel.lines);
  sel.lines = NULL;
  sel.enabled = false;
  sel.active = false;
}

bool select_active()
{
  return sel.active;
}

void select_enter(int idx)
{
  if (!sel.enabled || !select_active())
    return;

  bool enable = sel.aditv;

  int st = MIN(idx, sel.origin);
  int ed = MAX(idx, sel.origin);

  for (int i = 0; i < st; i++)
    sel.lines[i] = !enable;
  for (int i = st; i < ed; i++)
    sel.lines[i] = enable;
  for (int i = ed; i < sel.max; i++)
    sel.lines[i] = !enable;
}

bool select_has_line(int idx)
{
  if (!select_active())
    return false;
  return (sel.lines[idx]);
}
