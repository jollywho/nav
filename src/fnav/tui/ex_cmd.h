#ifndef FN_TUI_EX_CMD_H
#define FN_TUI_EX_CMD_H

#include "fnav/tui/buffer.h"
#include "fnav/compl.h"

// these need to align with state_symbol array
#define EX_CMD_STATE 1
#define EX_REG_STATE 0
#define EX_OFF_STATE -1

#define EX_EMPTY  1
#define EX_LEFT   2
#define EX_RIGHT  4
#define EX_NEW    8
#define EX_FRESH  16
#define EX_QUIT   32
#define EX_CYCLE  64
#define EX_HIST   128
#define EX_EXEC   256
#define EX_POP    (EX_LEFT|EX_EMPTY)
#define EX_PUSH   (EX_RIGHT|EX_NEW)
#define EX_CLEAR  (EX_LEFT|EX_RIGHT|EX_EMPTY|EX_CYCLE|EX_HIST)

typedef struct {
  fn_context *cx;
  int st;
} cmd_part;

void ex_cmd_init();
void ex_cmd_cleanup();
void start_ex_cmd(int ex_state);
void stop_ex_cmd();

void ex_input(int key);
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
int ex_cmd_curidx(List *list);
char* ex_cmd_line();

void ex_cmd_push(fn_context *cx);
cmd_part* ex_cmd_pop(int count);
void ex_cmd_set(int pos);

#endif
