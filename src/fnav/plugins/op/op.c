#include <sys/wait.h>
#include <errno.h>

#include "fnav/lib/utarray.h"
#include "fnav/plugins/op/op.h"
#include "fnav/log.h"
#include "fnav/model.h"
#include "fnav/event/hook.h"
#include "fnav/event/event.h"

static Op op;
static UT_array *procs;

typedef struct {
  uv_process_t proc;
  uv_process_options_t opts;
} op_proc;

static UT_icd proc_icd = { sizeof(op_proc),  NULL };

static void exit_cb(uv_process_t *req, int64_t exit_status, int term_signal)
{
  log_msg("OP", "exit_cb");
  uv_close((uv_handle_t*) req, NULL);
  op.ready = true;
  utarray_erase(procs, 0, 1);
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

  if (pid <= 0)
    return;
}

static void del_proc(uv_handle_t *hndl)
{
  utarray_erase(procs, 0, 1);
}

static void create_proc(Op *op, char *path)
{
  log_msg("OP", "create_proc");
  utarray_extend_back(procs);
  op_proc *proc = (op_proc*)utarray_back(procs);
  proc->opts.flags = UV_PROCESS_DETACHED;
  proc->opts.exit_cb = exit_cb;
  char *rv[] = {"mpv", "--fs", path, NULL};
  proc->opts.file = rv[0];
  proc->opts.args = rv;
  if (!op->ready) {
    log_msg("OP", "kill");
    op_proc *prev = (op_proc*)utarray_front(procs);
    if (prev) {
      uv_kill(prev->proc.pid, SIGKILL);
      uv_close((uv_handle_t*)&prev->proc, del_proc);
    }
  }

  log_msg("OP", "spawn");
  uv_signal_start(&mainloop()->children_watcher, chld_handler, SIGCHLD);
  uv_disable_stdio_inheritance();
  int ret = uv_spawn(eventloop(), &proc->proc, &proc->opts);
  op->ready = false;

  if (ret < 0)
    log_msg("?", "file: |%s|, %s", path, uv_strerror(ret));


  uv_unref((uv_handle_t*) &proc->proc);
}

static void fileopen_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("OP", "fileopen_cb");
  Op *op = (Op*)caller->top;

  char *path = model_curs_value(host->hndl->model, "fullpath");

  create_proc(op, path);
  system("mpv_i");
}

void op_new(Plugin *plugin, Buffer *buf, void *arg)
{
  log_msg("OP", "INIT");
  utarray_new(procs, &proc_icd);
  op.base = plugin;
  plugin->top = &op;
  plugin->name = "op";
  op.ready = true;
  hook_init_host(plugin);
  hook_add_intl(plugin, plugin, fileopen_cb, "fileopen");
  hook_set_tmp("fileopen");
}

void op_delete(Plugin *cntlr)
{
  log_msg("OP", "op_cleanup");
}
