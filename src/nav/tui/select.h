#ifndef FN_TUI_SELECT_H
#define FN_TUI_SELECT_H

#include "nav/nav.h"

void select_toggle(int cur, int max);
void select_clear();
bool select_active();
void select_enter(int idx);
bool select_has_line(int idx);

#endif
