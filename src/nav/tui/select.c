#include <malloc.h>
#include "nav/tui/select.h"
#include "nav/tui/buffer.h"
#include "nav/log.h"
#include "nav/macros.h"

typedef struct {
  int orgn_lnum;
  int orgn_index;
  int head;
  int *lines;
  int max;
  int count;
  bool enabled;
  bool active;
  bool aditv;
  Buffer *owner;
} Select;

static Select sel;

void select_toggle(Buffer *buf, int max)
{
  log_msg("SELECT", "toggle");
  if (select_active() && !select_owner(buf)) {
    select_clear(sel.owner);
    buf_refresh(sel.owner);
  }

  sel.owner = buf;
  sel.enabled = !sel.enabled;
  if (!sel.enabled)
    return;

  if (!select_active()) {
    sel.lines = calloc(max, sizeof(int));
    sel.count = 1;
  }

  int idx = buf_index(buf);
  sel.aditv = !sel.lines[idx];

  sel.head = idx;
  sel.max = max;
  sel.active = true;
  sel.orgn_lnum = buf_line(buf);
  sel.orgn_index = buf_top(buf);
  sel.lines[idx] = 1;
}

void select_clear(Buffer *buf)
{
  if (!select_owner(buf))
    return;

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

bool select_owner(Buffer *buf)
{
  return sel.owner == buf;
}

int select_count()
{
  return sel.count;
}

void select_enter(Buffer *buf, int idx)
{
  if (!sel.enabled || !select_active() || !select_owner(buf))
    return;

  bool enable = sel.aditv;
  int orgn = sel.orgn_lnum + sel.orgn_index;

  int st = MIN(sel.head, orgn);
  int ed = MAX(sel.head, orgn);

  for (int i = st; i <= ed; i++)
    sel.lines[i] = !enable;

  st = MIN(idx, orgn);
  ed = MAX(idx, orgn);
  sel.count = (ed - st) + 1;

  for (int i = st; i <= ed; i++)
    sel.lines[i] = enable;

  sel.head = idx;
  sel.lines[idx] = enable;
}

void select_min_origin(Buffer *buf, int *lnum, int *index)
{
  if (sel.orgn_lnum + sel.orgn_index > *lnum + *index)
    return;
  select_alt_origin(buf, lnum, index);
}

bool select_alt_origin(Buffer *buf, int *lnum, int *index)
{
  if (!select_owner(buf) || !select_active())
    return false;

  SWAP(int, *lnum,  sel.orgn_lnum);
  SWAP(int, *index, sel.orgn_index);
  return true;
}

bool select_has_line(Buffer *buf, int idx)
{
  if (!select_owner(buf) || !select_active())
    return false;
  return (sel.lines[idx]);
}
