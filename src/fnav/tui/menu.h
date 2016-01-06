#ifndef FN_TUI_MENU_H
#define FN_TUI_MENU_H

#include "fnav/cmd.h"

typedef struct Menu Menu;

Menu* menu_start();
void menu_stop(Menu *mnu);
void menu_update(Menu *mnu, Cmdline *cmd);
void menu_draw(Menu *mnu);
void menu_restart(Menu *mnu);
String menu_next(Menu *mnu, int dir);

#endif
