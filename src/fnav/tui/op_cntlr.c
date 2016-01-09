#include <sys/wait.h>
#include <errno.h>
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/model.h"
#include "fnav/event/hook.h"
#include "fnav/tui/op_cntlr.h"

//TODO: we want a single mpv process shared across all cntlr instances
//so basically, create an op_cntlr once on the first init call. return
//a pointer to this after. reference count on cleanup and do actual
//cleanup on count of 0. this mess can be properly managed when the real
//cntlr is made.

Op_cntlr *op;
int refs = 0;
uv_process_t proc;
uv_process_options_t opts;

static void exit_cb(uv_process_t *req, int64_t exit_status, int term_signal) {
  log_msg("OP", "exit_cb");
  uv_close((uv_handle_t*) req, NULL);
  Op_cntlr *op = (Op_cntlr*)req->data;
  op->ready = true;
  system("pkill compton");
}

static void chld_handler(uv_signal_t *handle, int signum)
{
  log_msg("OP", "chldhand");
  int stat = 0;
  int pid;

  do {
    pid = waitpid(-1, &stat, WNOHANG);
  } while (pid < 0 && errno == EINTR);

  if (pid <= 0) {
    return;
  }

  //Loop *loop = handle->loop->data;

  //Process *it;
  //SLIST_FOREACH(it, &loop->children, ent) {
  //  Process *proc = it;
  //  if (proc->pid == pid) {
  //    if (WIFEXITED(stat)) {
  //      proc->status = WEXITSTATUS(stat);
  //    } else if (WIFSIGNALED(stat)) {
  //      proc->status = WTERMSIG(stat);
  //    }
  //    proc->internal_exit_cb(proc);
  //    break;
  //  }
  //}
}

static void create_proc(Op_cntlr *op, String path)
{
  log_msg("OP", "create_proc");
  opts.flags = UV_PROCESS_DETACHED;
  opts.exit_cb = exit_cb;
  char *rv[] = {"mpv", "--fs", path, NULL};
  opts.file = rv[0];
  opts.args = rv;
  proc.data = op;

  if (!op->ready) {
    log_msg("OP", "kill");
    uv_kill(proc.pid, SIGKILL);
    uv_close((uv_handle_t*)&proc, NULL);
    uv_run(&op->loop.uv, UV_RUN_NOWAIT);
  }
  log_msg("OP", "spawn");
  uv_signal_start(&op->loop.children_watcher, chld_handler, SIGCHLD);
  uv_disable_stdio_inheritance();
  int ret = uv_spawn(&op->loop.uv, &proc, &opts);
  op->ready = false;
  if (ret < 0) {
    log_msg("?", "file: |%s|, %s", path, uv_strerror(ret));
  }
  uv_unref((uv_handle_t*) &proc);
}

const char *file_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

static void fileopen_cb(Cntlr *host, Cntlr *caller)
{
  log_msg("OP", "fileopen_cb");
  Op_cntlr *op = (Op_cntlr*)caller->top;

  String path = model_curs_value(host->hndl->model, "fullpath");
  //const char* ext = file_ext(path);
  //ventry *head = fnd_val("op_procs", "ext", (char*)ext);

  create_proc(op, path);
  system("mpv_i");
}

void op_loop(Loop *loop, int ms)
{
  process_loop(loop, ms);
}

static void pipe_attach_cb(Cntlr *host, Cntlr *caller)
{
  log_msg("OP", "pipe_attach_cb");
  hook_add(caller, host, fileopen_cb, "fileopen");
}

Cntlr* op_new()
{
  log_msg("OP", "INIT");
  if (!refs) {
    op = malloc(sizeof(Op_cntlr));
    op->base.top = op;
    op->ready = true;
    loop_add(&op->loop, op_loop);
    uv_timer_init(&op->loop.uv, &op->loop.delay);
    if (tbl_mk("op_procs")) {
      tbl_mk_fld("op_procs", "ext", typSTRING);
      tbl_mk_fld("op_procs", "file", typSTRING);
      tbl_mk_fld("op_procs", "single", typSTRING);
      tbl_mk_fld("op_procs", "ensure", typSTRING);
      tbl_mk_fld("op_procs", "args", typSTRING);
      tbl_mk_fld("op_procs", "uv_proc", typVOID);
      tbl_mk_fld("op_procs", "uv_opts", typVOID);
    }
    hook_init(&op->base);
  }
  refs++;
  hook_add(&op->base, &op->base, pipe_attach_cb, "pipe_attach");
  return &op->base;
}

void op_delete(Cntlr *cntlr)
{
  log_msg("OP", "op_cleanup");
}
