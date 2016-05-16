#include <sys/wait.h>
#include <errno.h>

#include "nav/lib/utarray.h"
#include "nav/tui/window.h"
#include "nav/plugins/op/op.h"
#include "nav/log.h"
#include "nav/model.h"
#include "nav/option.h"
#include "nav/event/hook.h"
#include "nav/event/event.h"
#include "nav/event/fs.h"
#include "nav/cmdline.h"
#include "nav/cmd.h"
#include "nav/util.h"

static void create_proc(fn_group *, char *);

static Op op;

typedef struct {
  uv_process_t proc;
  uv_process_options_t opts;
  fn_group *grp;
} Op_proc;

static char* expand_field(void *owner, const char *name)
{
  log_msg("OP", "expand_field");
  fn_group *grp = owner;

  int isfld = !strcmp(name, "prev") || !strcmp(name, "next");
  ventry *vent = fnd_val("op_procs", "group", grp->key);

  if (isfld && vent) {
    ventry *head = ent_head(vent);
    char *ret = rec_fld(head->rec, "pid");
    log_msg("OP", "%s", ret);
    return strdup(ret);
  }

  return strdup("");
}

static void add_pid(char *name, int pid)
{
  log_msg("OP", "add pid %d", pid);
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

static void run_group(fn_group *grp, char *line, bool proc)
{
  Cmdstr cmd;
  set_fldvar(grp, "prev", expand_field);
  set_fldvar(grp, "next", expand_field);

  Cmdstr *cmdptr = proc ? &cmd : NULL;
  cmd_eval(cmdptr, line);

  if (proc && cmd.ret.type == STRING) {
    create_proc(grp, cmd.ret.val.v_str);
    free(cmd.ret.val.v_str);
  }
}

static void exit_cb(uv_process_t *req, int64_t exit_status, int term_signal)
{
  log_msg("OP", "exit_cb");
  Op_proc *proc = req->data;
  fn_group *grp = proc->grp;
  remove_pid(grp->key, proc->proc.pid);
  uv_close((uv_handle_t*) req, del_proc);

  if (!proc->proc.status)
    run_group(grp, grp->opgrp->after, false);
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

static void create_proc(fn_group *grp, char *line)
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
  proc->opts.cwd = window_cur_dir();

  uv_signal_start(&mainloop()->children_watcher, chld_handler, SIGCHLD);
  int ret = uv_spawn(eventloop(), &proc->proc, &proc->opts);

  if (ret < 0) {
    log_msg("?", "file: |%s|, %s", line, uv_strerror(ret));
    free(proc);
    return;
  }
  uv_unref((uv_handle_t*)&proc->proc);
  add_pid(grp->key, proc->proc.pid);
}

static void fileopen_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("OP", "fileopen_cb");
  char *path = hka->arg;
//FIXME: should use first item in selection or get_syn each item
  char *name = model_curs_value(host->hndl->model, "name");
  log_msg("OP", "path %s %s", path, name);
  log_msg("OP", "ext %s ", file_ext(name));
  fn_syn *syn = get_syn(file_ext(path));
  if (!syn)
    return;

  fn_group *grp = syn->group;
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

static Cmdret op_kill(List *args, Cmdarg *ca)
{
  log_msg("OP", "kill");
  int pid;
  char *pidstr = list_arg(args, 1, VAR_STRING);
  if (!pidstr || !str_num(pidstr, &pid))
    return NORET;

  ventry *vent = fnd_val("op_procs", "pid", pidstr);
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

void op_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("OP", "INIT");
  op.base = plugin;
  plugin->top = &op;
  plugin->name = "op";
  hook_add_intl(-1, plugin, plugin, fileopen_cb,  "fileopen");
  hook_add_intl(-1, plugin, plugin, execopen_cb,  "execopen");
  hook_add_intl(-1, plugin, plugin, execclose_cb, "execclose");
  hook_set_tmp("fileopen");
  hook_set_tmp("execopen");
  hook_set_tmp("execclose");
  if (tbl_mk("op_procs")) {
    tbl_mk_fld("op_procs", "group", TYP_STR);
    tbl_mk_fld("op_procs", "pid",   TYP_STR);
    //tbl_mk_fld("op_procs", "status", typSTRING);
  }

  Cmd_T killcmd  = {"kill",  0, op_kill,  0};
  cmd_add(&killcmd);
}

void op_delete(Plugin *plugin)
{
}
