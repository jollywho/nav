#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <ncurses.h>

#include "fnav/ascii.h"
#include "fnav/tui/buffer.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/table.h"
#include "fnav/tui/pane.h"
#include "fnav/log.h"

enum Type { OPERATOR, MOTION };

typedef struct {
  String name;
  enum Type type;
  void (*f)();
} Key;

typedef struct {
  pos_T start;                  /* start of the operator */
  pos_T end;                    /* end of the operator */
  long opcount;                 /* count before an operator */
  int arg;
} Cmdarg;

typedef void (*key_func)(Cntlr *cntlr, Cmdarg *arg);

Cmd default_lst[] = {
  { "open",  0,  NULL },
  { "cd",    0,  NULL },
  { "list",  0,  NULL },
};

String init_dir = "/home/chi/test";
static void fm_mv();
static void fm_page();
static void fm_mv();
static void fm_left();
static void fm_right();
static void fm_g_cmd();

static const struct fm_cmd {
  int cmd_char;                 /* (first) command character */
  key_func cmd_func;            /* function for this command */
  int cmd_flags;                /* FN_ flags */
  short cmd_arg;                /* value for ca.arg */
} fm_cmds[] =
{
  {Ctrl_J,  fm_page,        0,                 FORWARD},
  {Ctrl_K,  fm_page,        0,                 BACKWARD},
  {'h',     fm_left,        0,                 0},
  {'l',     fm_right,       0,                 0},
  {'j',     fm_mv,          0,                 FORWARD},
  {'k',     fm_mv,          0,                 BACKWARD},
  {'g',     fm_g_cmd,       0,                 BACKWARD},
  {'G',     fm_g_cmd,       0,                 FORWARD},
};

void cntlr_cancel(Cntlr *cntlr)
{
  log_msg("FM", "<|_CANCEL_|>");
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  self->op_count = 1;
  self->mo_count = 1;
  self->fs->cancel = true;
}

void fm_read_scan()
{
  log_msg("FM", "read");
}

void fm_after_scan()
{
  log_msg("FM", "async done");
}

void cntlr_focus(Cntlr *cntlr)
{
  log_msg("FM", "update dir");
  buf_refresh(cntlr->hndl->buf);
}

static void fm_left(Cntlr *cntlr)
{
  log_msg("FM", "cmd left");
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  self->cur_dir = fs_parent_dir(self->cur_dir);
  cntlr->hndl->fval = self->cur_dir;
  buf_set(cntlr->hndl, "name");
  fs_open(self->fs, self->cur_dir);
}

static void fm_right(Cntlr *cntlr)
{
  log_msg("FM", "cmd right");
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  if (isdir(buf_rec(cntlr->hndl))) {
    self->cur_dir = buf_val(cntlr->hndl, "fullpath");
    cntlr->hndl->fval = self->cur_dir;
    buf_set(cntlr->hndl, "name");
    fs_open(self->fs, self->cur_dir);
  }
}

static void fm_mv(Cntlr *cntlr, Cmdarg *arg)
{
  log_msg("FM", "cmd mv");
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  buf_mv(cntlr->hndl->buf, 0, arg->arg);
  log_msg("FM", "set cur: %s", self->cur_dir);
  log_msg("FM", "waiting on job...");
}

static void fm_page(Cntlr *cntlr, Cmdarg *arg)
{
  log_msg("FM", "cmd page");
  buf_mv(cntlr->hndl->buf, 0, arg->arg * buf_pgsize(cntlr->hndl));
  log_msg("FM", "waiting on job...");
}

static void fm_g_cmd(Cntlr *cntlr, Cmdarg *arg)
{
  log_msg("FM", "cmd _g");
  buf_mv(cntlr->hndl->buf, 0, arg->arg * buf_entsize(cntlr->hndl));
  log_msg("FM", "waiting on job...");
}

/* Number of commands in nv_cmds[]. */
#define FM_CMDS_SIZE ARRAY_SIZE(fm_cmds)

/* Sorted index of commands in nv_cmds[]. */
static short nv_cmd_idx[FM_CMDS_SIZE];

/* The highest index for which
 * nv_cmds[idx].cmd_char == nv_cmd_idx[nv_cmds[idx].cmd_char] */
static int nv_max_linear;

/*
 * Compare functions for qsort() below, that checks the command character
 * through the index in nv_cmd_idx[].
 */
static int nv_compare(const void *s1, const void *s2)
{
  int c1, c2;

  /* The commands are sorted on absolute value. */
  c1 = fm_cmds[*(const short *)s1].cmd_char;
  c2 = fm_cmds[*(const short *)s2].cmd_char;
  if (c1 < 0)
    c1 = -c1;
  if (c2 < 0)
    c2 = -c2;
  return c1 - c2;
}

/*
 * Initialize the nv_cmd_idx[] table.
 */
void init_cmds(void)
{
  /* Fill the index table with a one to one relation. */
  for (short int i = 0; i < (short int)FM_CMDS_SIZE; ++i) {
    nv_cmd_idx[i] = i;
  }

  /* Sort the commands by the command character.  */
  qsort(&nv_cmd_idx, FM_CMDS_SIZE, sizeof(short), nv_compare);

  /* Find the first entry that can't be indexed by the command character. */
  short int i;
  for (i = 0; i < (short int)FM_CMDS_SIZE; ++i) {
    if (i != fm_cmds[nv_cmd_idx[i]].cmd_char) {
      break;
    }
  }
  nv_max_linear = i - 1;
}

/*
 * Search for a command in the commands table.
 * Returns -1 for invalid command.
 */
static int find_command(int cmdchar)
{
  int i;
  int idx;
  int top, bot;
  int c;

  /* A multi-byte character is never a command. */
  if (cmdchar >= 0x100)
    return -1;

  /* We use the absolute value of the character.  Special keys have a
   * negative value, but are sorted on their absolute value. */
  if (cmdchar < 0)
    cmdchar = -cmdchar;

  /* If the character is in the first part: The character is the index into
   * nv_cmd_idx[]. */
  if (cmdchar <= nv_max_linear)
    return nv_cmd_idx[cmdchar];

  /* Perform a binary search. */
  bot = nv_max_linear + 1;
  top = FM_CMDS_SIZE - 1;
  idx = -1;
  while (bot <= top) {
    i = (top + bot) / 2;
    c = fm_cmds[nv_cmd_idx[i]].cmd_char;
    if (c < 0)
      c = -c;
    if (cmdchar == c) {
      idx = nv_cmd_idx[i];
      break;
    }
    if (cmdchar > c)
      bot = i + 1;
    else
      top = i - 1;
  }
  return idx;
}

int cntlr_input(Cntlr *cntlr, int key)
{
  Cmdarg ca;
  int idx = find_command(key);
  ca.arg = fm_cmds[idx].cmd_arg;
  if (idx >= 0) {
    fm_cmds[idx].cmd_func(cntlr, &ca);
  }
  return 0;
}

void init_fm_hndl(FM_cntlr *fm, Pane *p, Cntlr *c, String val)
{
  fn_handle *hndl = malloc(sizeof(fn_handle));
  hndl->tname = "fm_files";
  hndl->buf = buf_init();
  hndl->fname = "dir";
  hndl->fval = val;
  c->hndl = hndl;
  c->cmd_lst = default_lst;
  c->_focus = cntlr_focus;
  c->_cancel = cntlr_cancel;
  c->_input = cntlr_input;
  c->top = fm;
  pane_add(p, c);
  buf_set(c->hndl, "name");
}

FM_cntlr* fm_cntlr_init(Pane *p)
{
  log_msg("INIT", "FM_CNTLR");
  init_cmds(); //TODO: cleanup loose parts

  FM_cntlr *fm = malloc(sizeof(FM_cntlr));
  fm->op_count = 1;
  fm->mo_count = 1;
  fm->cur_dir = init_dir;

  tbl_mk("fm_files");
  tbl_mk_fld("fm_files", "name", typSTRING);
  tbl_mk_fld("fm_files", "dir", typSTRING);
  tbl_mk_fld("fm_files", "fullpath", typSTRING);

  tbl_mk("fm_stat");
  tbl_mk_fld("fm_stat", "fullpath", typSTRING);
  tbl_mk_fld("fm_stat", "update", typVOID);
  tbl_mk_fld("fm_stat", "stat", typVOID);
  init_fm_hndl(fm, p, &fm->base, fm->cur_dir);

  fm->fs = fs_init(&fm->base, fm->base.hndl, fm_read_scan);
  fs_open(fm->fs, fm->cur_dir);

//  init_fm_hndl(fm, p, &fm->parent, fm->cur_dir);
//  init_fm_hndl(fm, p, &fm->child, fm->cur_dir);

  return fm;
}

void fm_cntlr_cleanup(FM_cntlr *cntlr)
{
  free(cntlr);
  // TODO: careful cleanup + cancel any pending cb
}
