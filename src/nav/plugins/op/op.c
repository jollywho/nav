#include <sys/wait.h>
#include "nav/lib/utarray.h"
#include "nav/plugins/op/op.h"
#include "nav/plugins/term/term.h"
#include "nav/log.h"
#include "nav/model.h"
#include "nav/option.h"
#include "nav/event/hook.h"
#include "nav/event/event.h"
#include "nav/event/shell.h"
#include "nav/event/fs.h"
#include "nav/cmdline.h"
#include "nav/cmd.h"
#include "nav/util.h"
#include "nav/compl.h"

static void create_proc(nv_group *, char *);

static Op op;

typedef struct {
  uv_process_t proc;
  uv_process_options_t opts;
  nv_group *grp;
} Op_proc;

void pid_list()
{
  record_list("op_procs", "pid", "group");
}

void op_set_exec_line(char *line, void *grp)
{
  SWAP_ALLOC_PTR(op.last_line, strdup(line));
  op.lastgrp = grp;
}

static void add_pid(char *name, int pid)
{
  log_msg("OP", "add pid %d", pid);
  op.last_pid = pid;
  char *pidstr;
  asprintf(&pidstr, "%d", pid);
  trans_rec *r = mk_trans_rec(tbl_fld_count("op_procs"));
  edit_trans(r, "group", name,    NULL);
  edit_trans(r, "pid",   pidstr,  NULL);
  CREATE_EVENT(eventq(), commit, 2, "op_procs", r);
  free(pidstr);
}

static void remove_pid(char *name, int pid)
{
  log_msg("OP", "remove pid %d", pid);
  char *pidstr;
  asprintf(&pidstr, "%d", pid);
  tbl_del_val("op_procs", "pid", pidstr);
  free(pidstr);
}

static void del_proc(uv_handle_t *hndl)
{
  free(hndl->data);
}

static void run_group(nv_group *grp, char *line, bool proc)
{
  Cmdstr cmd;
  op.curgrp = grp;

  Cmdstr *cmdptr = proc ? &cmd : NULL;
  cmd_eval(cmdptr, line);

  if (proc && cmd.ret.type == STRING) {
    create_proc(grp, cmd.ret.val.v_str);
    free(cmd.ret.val.v_str);
  }

  op.curgrp = NULL;
}

static void exit_cb(uv_process_t *req, int64_t exit_status, int term_signal)
{
  log_msg("OP", "exit_cb");
  Op_proc *proc = req->data;
  nv_group *grp = proc->grp;
  remove_pid(grp->key, proc->proc.pid);
  uv_close((uv_handle_t*) req, del_proc);
  op.last_status = exit_status;

  if (!proc->proc.status)
    run_group(grp, grp->opgrp->after, false);
}

static void chld_handler(uv_signal_t *handle, int signum)
{
  log_msg("OP", "chldhand");
  int stat = 0;
  int pid;

  do
    pid = waitpid(-1, &stat, WNOHANG);
  while (pid < 0 && errno == EINTR);

  if (pid <= 0)
    return;

  Term *it;
  SLIST_FOREACH(it, &mainloop()->subterms, ent) {
    Term *term = it;
    if (term->pid == pid) {
      if (WIFEXITED(stat))
        term->status = WEXITSTATUS(stat);
      else if (WIFSIGNALED(stat))
        term->status = WTERMSIG(stat);

      term_close(it);
      break;
    }
  }
}

static void create_proc(nv_group *grp, char *line)
{
  log_msg("OP", "create_proc %s", line);

  Op_proc *proc = malloc(sizeof(Op_proc));
  memset(proc, 0, sizeof(Op_proc));
  proc->opts.flags = UV_PROCESS_DETACHED;
  proc->opts.exit_cb = exit_cb;
  proc->grp = grp;

  char* args[4];
  args[0] = get_opt_str("shell");
  args[1] = "-c";
  args[2] = line;
  args[3] = NULL;
  proc->opts.file = args[0];
  proc->opts.args = args;
  proc->proc.data = proc;
  proc->opts.cwd = fs_pwd();

  int ret = uv_spawn(eventloop(), &proc->proc, &proc->opts);

  if (ret < 0) {
    log_msg("?", "file: |%s|, %s", line, uv_strerror(ret));
    free(proc);
    return;
  }

  op_set_exec_line(line, grp);
  uv_unref((uv_handle_t*)&proc->proc);
  add_pid(grp->key, proc->proc.pid);
}

//TODO: get_syn each item in buffer list. clump items together when opened.
static void fileopen_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("OP", "fileopen_cb");
  char *name = model_curs_value(host->hndl->model, "name");
  char *path = model_curs_value(host->hndl->model, "fullpath");
  log_msg("OP", "path %s %s", path, name);
  log_msg("OP", "ext %s ", file_ext(name));
  nv_syn *syn = get_syn(file_ext(path));
  if (!syn)
    return;

  nv_group *grp = syn->group;
  log_msg("OP", "%s", grp->key);
  if (!grp->opgrp)
    return;

  run_group(grp, grp->opgrp->before, true);
}

static void execopen_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("OP", "exec_cb");
  if (!hka->arg)
    return;
  add_pid("", *(int*)hka->arg);
}

static void execclose_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("OP", "exec_cb");
  if (!hka->arg)
    return;
  remove_pid("", *(int*)hka->arg);
}

Cmdret op_kill(List *args, Cmdarg *ca)
{
  log_msg("OP", "kill");
  int pid;
  char *pidstr = list_arg(args, 1, VAR_STRING);
  if (!pidstr || !str_num(pidstr, &pid))
    return NORET;

  Ventry *vent = fnd_val("op_procs", "pid", pidstr);
  if (!vent) {
    log_msg("OP", "pid %s not found", pidstr);
    return NORET;
  }

  uv_kill(pid, SIGKILL);
  return NORET;
}

Op_group* op_newgrp(const char *before, const char *after)
{
  Op_group *opgrp = malloc(sizeof(Op_group));
  opgrp->before = strip_quotes(before);
  opgrp->after = strip_quotes(after);
  return opgrp;
}

void op_delgrp(Op_group *opgrp)
{
  if (!opgrp)
    return;

  free(opgrp->before);
  free(opgrp->after);
  free(opgrp);
}

void* op_active_group()
{
  return op.curgrp;
}

void op_set_exit_status(int status)
{
  op.last_status = status;
}

char* op_pid_last()
{
  char *str;
  asprintf(&str, "%d", op.last_pid);
  return str;
}

char* op_status_last()
{
  char *str;
  asprintf(&str, "%d", op.last_status);
  return str;
}

int op_repeat_last_exec()
{
  if (op.lastgrp)
    run_group(op.lastgrp, op.last_line, true);
  else
    return shell_exec(op.last_line, fs_pwd());

  return op.last_pid;
}

void op_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("OP", "INIT");
  op.base = plugin;
  op.curgrp = NULL;
  op.last_pid = 0;
  op.last_line = strdup("");
  plugin->top = &op;
  hook_add_intl(-1, plugin, plugin, fileopen_cb,  EVENT_FILEOPEN);
  hook_add_intl(-1, plugin, plugin, execopen_cb,  EVENT_EXEC_OPEN);
  hook_add_intl(-1, plugin, plugin, execclose_cb, EVENT_EXEC_CLOSE);
  hook_set_tmp(EVENT_FILEOPEN);
  hook_set_tmp(EVENT_EXEC_OPEN);
  hook_set_tmp(EVENT_EXEC_CLOSE);
  if (tbl_mk("op_procs")) {
    tbl_mk_fld("op_procs", "group", TYP_STR);
    tbl_mk_fld("op_procs", "pid",   TYP_STR);
  }
  uv_signal_start(&mainloop()->children_watcher, chld_handler, SIGCHLD);
}

void op_delete(Plugin *plugin)
{
}
