#include <malloc.h>
#include "fnav/table.h"
#include "fnav/tui/op_cntlr.h"

void op_callback(Op_cntlr *cntlr, Model *m)
{
  // check key
  // get curs value of default field
  // validate path
  // char *path = model_curs_value(h->model, "fullpath");
  // run_proc(cntlr, "mpv", path);
}

static void run_proc(Op_cntlr *cntlr, const String file, const String path)
{
  char *rv[3] = { file, path, NULL };
  // if singleton && proc.pid,
  //   kill pid
  // if ensure && pid
  //  skip
  // SAVE PROC NAME TO LIST??
  uv_process_t *proc = malloc(sizeof(uv_process_t*));
  uv_process_options_t *opts = malloc(sizeof(uv_process_t*));
  opts->file = file;
  opts->args = rv;
  opts->flags = UV_PROCESS_DETACHED;
  uv_spawn(&cntlr->loop.uv, proc, opts);
}

void op_loop(Loop *loop, int ms)
{
}

void op_init(Cntlr *attach)
{
  Op_cntlr *cntlr = malloc(sizeof(Op_cntlr*));
  loop_add(&cntlr->loop, op_loop);
  uv_timer_init(&cntlr->loop.uv, &cntlr->loop.delay);
  if (tbl_mk("op_procs")) {
    tbl_mk_fld("op_procs", "name", typSTRING);
    tbl_mk_fld("op_procs", "single", typSTRING);
    tbl_mk_fld("op_procs", "ensure", typSTRING);
    tbl_mk_fld("op_procs", "uv_proc", typVOID);
    tbl_mk_fld("op_procs", "uv_opts", typVOID);
  }
}

void op_cleanup(Op_cntlr *cntlr);
