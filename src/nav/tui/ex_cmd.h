#ifndef NV_TUI_EX_CMD_H
#define NV_TUI_EX_CMD_H

#include "nav/tui/buffer.h"
#include "nav/compl.h"

// these need to align with state_symbol array
#define EX_OFF_STATE 10
#define EX_REG_STATE 0
#define EX_CMD_STATE 1
#define EX_FIL_STATE 2

#define EX_EMPTY  1
#define EX_FRESH  16
#define EX_QUIT   32
#define EX_CYCLE  64
#define EX_HIST   128
#define EX_EXEC   256
#define EX_CLEAR  (EX_EMPTY|EX_CYCLE|EX_HIST)

void ex_cmd_init();
void ex_cmd_cleanup();
void start_ex_cmd(char, int);
void stop_ex_cmd();
void ex_cmdinvert();

void ex_input(Keyarg *ca);
void cmdline_resize();
void cmdline_refresh();
void ex_cmd_populate(const char *);

char ex_cmd_curch();
int ex_cmd_curpos();
Cmdstr* ex_cmd_curcmd();
Cmdstr* ex_cmd_prevcmd();
Token* ex_cmd_curtok();
char* ex_cmd_curstr();
List* ex_cmd_curlist();
int ex_cmd_state();
char* ex_cmd_line();

int ex_cmd_height();

#endif
