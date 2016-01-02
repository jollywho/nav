#ifndef FN_TUI_EX_CMD_H
#define FN_TUI_EX_CMD_H

#include "fnav/tui/buffer.h"
#include "fnav/compl.h"

// these need to align with state_symbol array
#define EX_CMD_STATE 0
#define EX_REG_STATE 1

#define EX_EMPTY  1
#define EX_LEFT   2
#define EX_RIGHT  4
#define EX_NEW    8
#define EX_FRESH  16
#define EX_POP    (EX_LEFT|EX_EMPTY)
#define EX_PUSH   (EX_RIGHT|EX_NEW)

typedef struct {
  fn_context *cx;
  int st;
} cmd_part;

void start_ex_cmd(int ex_state);
void stop_ex_cmd();

void ex_input(int key);
void cmdline_refresh();

char ex_cmd_curch();
int ex_cmd_curpos();
Token* ex_cmd_curtok();
String ex_cmd_curstr();
int ex_cmd_state();

void ex_cmd_push(fn_context *cx);
cmd_part* ex_cmd_pop();

#endif
