#ifndef FN_EVENT_INPUT_H
#define FN_EVENT_INPUT_H

#include "nav/plugins/plugin.h"
#include "nav/ascii.h"

#define OP_NOP          0       /* no pending operation */
#define OP_YANK         1       /* "y"  yank   operator */
#define OP_MARK         2       /* "m"  mark   operator */
#define OP_JUMP         3       /* "'"  jump   operator */
#define OP_G            4       /* "g"  spcl   operator */
#define OP_DELETE       5       /* "d"  delete operator */
#define OP_CHANGE       6       /* "c"  change operator */

typedef struct {
  int key;                      // current pending operator type
  int regname;                  // register to use for the operator
  int motion_type;              // type of the current cursor motion
  int motion_force;             // force motion type: 'v', 'V' or CTRL-V
  pos_T start;                  // start of the operator
  pos_T end;                    // end of the operator
  pos_T cursor_start;           // cursor position before motion for "gw"

  long line_count;              // number of lines from op_start to op_end
                                // (inclusive)
  bool is_VIsual;               // operator on Visual area
} Oparg;

struct Keyarg {
  Oparg oap;                    /* Operator arguments */
  pos_T start;                  /* start of the operator */
  pos_T end;                    /* end of the operator */
  long opcount;                 /* count before an operator */
  int type;
  int key;
  int nkey;
  int mkey;
  short arg;
};

typedef void (*key_func)(void *obj, Keyarg *arg);

typedef struct {
  int cmd_char;                 /* (first) command character */
  key_func cmd_func;            /* function for this command */
  int cmd_flags;                /* FN_ flags */
  short cmd_arg;                /* value for ca.arg */
} fn_key;

typedef struct {
  int op_char;
  int nchar;
  key_func cmd_func;
} fn_oper;

typedef struct {
  fn_key *tbl;
  short *cmd_idx;
  int maxsize;
  int maxlinear;
} fn_keytbl;

void input_init(void);
void input_cleanup(void);
void input_setup_tbl(fn_keytbl *kt);

bool input_map_exists(int key);
void set_map(char *lhs, char *rhs);
void do_map(int key);

int find_command(fn_keytbl *kt, int cmdchar);
int find_do_cmd(fn_keytbl *kt, Keyarg *ca, void *obj);
int find_do_op(fn_oper *kt, Keyarg *ca, void *obj);
int input_waitkey();
void input_check();
void clearop(Keyarg *ca);
bool op_pending(Keyarg *arg);

struct fn_reg {
  int key;
  char *value;
};

fn_reg* reg_get(int ch);
fn_reg* reg_dcur();
void reg_clear_dcur();
void reg_set(int ch, char *value, char *show);

#endif
