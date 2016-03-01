#ifndef FN_TUI_MENU_H
#define FN_TUI_MENU_H

#include "fnav/cmd.h"

typedef struct Menu Menu;

Menu* menu_new();
void menu_delete(Menu *mnu);

void menu_start(Menu *mnu);
void menu_restart(Menu *mnu);
void menu_stop(Menu *mnu);
void menu_toggle_hints(Menu *mnu);
bool menu_hints_enabled(Menu *mnu);
void menu_input(Menu *mnu, int key);

void menu_rebuild(Menu *mnu);
void menu_update(Menu *mnu, Cmdline *cmd);

void menu_draw(Menu *mnu);
char* menu_next(Menu *mnu, int dir);

void path_list(List *args);
void menu_ch_dir(void **args);

#endif
