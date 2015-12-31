#ifndef FN_TUI_EX_CMD_H
#define FN_TUI_EX_CMD_H

#include "fnav/tui/buffer.h"

//these need to align with state_symbol array
#define EX_CMD_STATE 0
#define EX_REG_STATE 1

void start_ex_cmd(int ex_state);
void stop_ex_cmd();
void ex_input(int key);
void cmdline_refresh();
char ex_cmd_curch();
int ex_cmd_curpos();

#endif
