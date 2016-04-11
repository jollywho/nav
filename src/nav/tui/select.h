#ifndef FN_TUI_SELECT_H
#define FN_TUI_SELECT_H

#include "nav/nav.h"

void select_toggle(int lnum, int index, int max);
void select_clear();
bool select_active();
int select_count();
void select_enter(int idx);
void select_min_origin(int *lnum, int *index);
bool select_alt_origin(int *lnum, int *index);
bool select_has_line(int idx);

#endif
