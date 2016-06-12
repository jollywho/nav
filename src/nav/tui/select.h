#ifndef NV_TUI_SELECT_H
#define NV_TUI_SELECT_H

#include "nav/plugins/plugin.h"

void select_toggle(Buffer *, int max);
void select_clear(Buffer *);
bool select_active();
int select_count();
bool select_owner(Buffer *);
void select_enter(Buffer *, int idx);
void select_min_origin(Buffer *, int *lnum, int *index);
bool select_alt_origin(Buffer *, int *lnum, int *index);
bool select_has_line(Buffer *, int idx);

#endif
