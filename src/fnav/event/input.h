#ifndef FN_INPUT_H
#define FN_INPUT_H

#include "fnav/tui/cntlr.h"

typedef struct {
  pos_T start;                  /* start of the operator */
  pos_T end;                    /* end of the operator */
  long opcount;                 /* count before an operator */
  int arg;
} Cmdarg;

typedef void (*key_func)(void *obj, Cmdarg *arg);

typedef struct {
  int cmd_char;                 /* (first) command character */
  key_func cmd_func;            /* function for this command */
  int cmd_flags;                /* FN_ flags */
  short cmd_arg;                /* value for ca.arg */
} fn_key;

typedef struct {
  fn_key *tbl;
  short *cmd_idx;
  int maxsize;
  int maxlinear;
} fn_keytbl;

void input_init(void);
void input_cleanup(void);
void input_setup_tbl(fn_keytbl *kt);
int find_command(fn_keytbl *kt, int cmdchar);
void input_check();

#endif
