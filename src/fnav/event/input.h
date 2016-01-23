#ifndef FN_INPUT_H
#define FN_INPUT_H

#include "fnav/tui/cntlr.h"
#include "fnav/ascii.h"

#define OP_NOP          0       /* no pending operation */
#define OP_DELETE       1       /* "d"  delete operator */
#define OP_YANK         2       /* "y"  yank   operator */
#define OP_CHANGE       3       /* "c"  change operator */
#define OP_MARK         4       /* "m"  mark   operator */
#define OP_JUMP         5       /* "'"  jump   operator */

/*
 * Generally speaking, every Normal mode command should either clear any
 * pending operator (with *clearop*()), or set the motion type variable
 * oap->motion_type.
 *
 * When a cursor motion command is made, it is marked as being a character or
 * line oriented motion.  Then, if an operator is in effect, the operation
 * becomes character or line oriented accordingly.
 */

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
  void *obj;
} Oparg;

struct Cmdarg {
  Oparg oap;                    /* Operator arguments */
  pos_T start;                  /* start of the operator */
  pos_T end;                    /* end of the operator */
  long opcount;                 /* count before an operator */
  int key;
  int nkey;
  int mkey;
  int arg;
};

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
void set_oparg(Cmdarg *ca, void *obj, int key);
void clear_oparg(Cmdarg *ca);
bool this_op_pending(void *obj, Cmdarg *arg);

#endif
