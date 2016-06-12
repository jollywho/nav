#ifndef NV_EVENT_INPUT_H
#define NV_EVENT_INPUT_H

#include "nav/plugins/plugin.h"
#include "nav/ascii.h"

#define OP_NOP          0       /* no pending operation */
#define OP_YANK         1       /* "y"  yank   operator */
#define OP_MARK         2       /* "m"  mark   operator */
#define OP_JUMP         3       /* "'"  jump   operator */
#define OP_G            4       /* "g"  spcl   operator */
#define OP_DELETE       5       /* "d"  delete operator */
#define OP_WIN          6       /* "^w" window operator */

typedef void (*key_func)(void *obj, Keyarg *arg);

typedef struct {
  int cmd_char;                 /* (first) command character */
  key_func cmd_func;            /* function for this command */
  int cmd_flags;                /* NCH* and EVENT */
  short cmd_arg;                /* value for ca.arg */
} nv_key;

typedef struct {
  int op_char;
  int nchar;
  key_func cmd_func;
} nv_oper;

typedef struct {
  nv_key *tbl;
  short *cmd_idx;
  int maxsize;
  int maxlinear;
} nv_keytbl;

struct nv_reg {
  int key;
  varg_T value;
};

typedef struct {
  int key;                      // current pending operator type
  int regname;                  // register to use for the operator
  int motion_type;              // type of the current cursor motion
  int motion_force;             // force motion type: 'v', 'V' or CTRL-V
  pos_T cursor_start;           // cursor position before motion for "gw"

  long line_count;              // number of lines from op_start to op_end
                                // (inclusive)
  bool is_VIsual;               // operator on Visual area
  bool is_unused;               // op cmd rejected key
} Oparg;

struct Keyarg {
  nv_keytbl *optbl;             /* table that set operator */
  Oparg oap;                    /* Operator arguments */
  long opcount;                 /* count before an operator */
  int type;
  int key;
  int nkey;
  int mkey;
  short arg;
  char *utf8;
};

void input_init(void);
void input_cleanup(void);
void input_setup_tbl(nv_keytbl *kt);

void unset_map(char *from);
void set_map(char *lhs, char *rhs);

bool try_do_map(Keyarg *ca);
int find_command(nv_keytbl *kt, int cmdchar);
int find_do_key(nv_keytbl *kt, Keyarg *ca, void *obj);
void input_check();
void oper(void *, Keyarg *ca);
void clearop(Keyarg *ca);
bool op_pending(Keyarg *arg);

nv_reg* reg_get(int ch);
nv_reg* reg_dcur();
void reg_clear_dcur();
void reg_set(int ch, varg_T);
void reg_yank(char *);

#endif
