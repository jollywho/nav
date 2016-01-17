#ifndef FN_TUI_MENU_H
#define FN_TUI_MENU_H

#include "fnav/cmd.h"

typedef struct Menu Menu;

Menu* menu_new();
void menu_delete(Menu *mnu);

void menu_start(Menu *mnu);
void menu_restart(Menu *mnu);
void menu_stop(Menu *mnu);

void menu_rebuild(Menu *mnu);
void menu_update(Menu *mnu, Cmdline *cmd);

void menu_draw(Menu *mnu);
String menu_next(Menu *mnu, int dir);

void path_list(String line);

#endif
