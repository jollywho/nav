#include <sys/wait.h>
#include <errno.h>

#include "nav/lib/utarray.h"
#include "nav/plugins/op/op.h"
#include "nav/log.h"
#include "nav/model.h"
#include "nav/option.h"
#include "nav/event/hook.h"
#include "nav/event/event.h"
#include "nav/event/fs.h"
#include "nav/cmdline.h"
#include "nav/cmd.h"

static Op op;
static UT_array *procs; // replace with opgrp

typedef struct {
  uv_process_t proc;
  uv_process_options_t opts;
} Op_proc;

static UT_icd proc_icd = { sizeof(Op_proc),  NULL };

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

static void add_pid(char *name, int pid)
{
  log_msg("OP", "add pid %d", pid);
  char *pidstr;
  asprintf(&pidstr, "%d", pid);
  trans_rec *r = mk_trans_rec(tbl_fld_count("op_procs"));
  edit_trans(r, "group", name,   NULL);
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
  Op_proc *proc = hndl->data;
  log_msg("OP", "del proc %d", proc->proc.pid);
}

static void create_proc(Op_group *opgrp, char *path)
{
  log_msg("OP", "create_proc");

  log_msg("OP", "%s", opgrp->before);
  utarray_extend_back(procs);
  Op_proc *proc = (Op_proc*)utarray_back(procs);
  proc->opts.flags = UV_PROCESS_DETACHED;
  proc->opts.exit_cb = exit_cb;

  char *line = strdup(opgrp->before);
  char *rv = do_expansion(line);
  char* args[4];
  args[0] = p_sh;
  args[1] = "-c";
  args[2] = rv;
  args[3] = NULL;
  proc->opts.file = args[0];
  proc->opts.args = args;
  log_msg("OP", "%s", rv);

  if (!op.ready) {
    log_msg("OP", "kill");
    Op_proc *prev = (Op_proc*)utarray_front(procs);
    if (prev) {
      uv_kill(prev->proc.pid, SIGKILL);
      prev->proc.data = proc;
      uv_close((uv_handle_t*)&prev->proc, del_proc);
    }
  }

  log_msg("OP", "spawn");
  uv_signal_start(&mainloop()->children_watcher, chld_handler, SIGCHLD);
  uv_disable_stdio_inheritance();
  int ret = uv_spawn(eventloop(), &proc->proc, &proc->opts);
  op.ready = false;

  if (ret < 0) {
    log_msg("?", "file: |%s|, %s", path, uv_strerror(ret));
    //remove from opgrp
  }

  uv_unref((uv_handle_t*)&proc->proc);
  free(rv);
  free(line);
}

static char* expand_field(char *name, char *key)
{
  log_msg("OP", "expand_field");
  char *val = model_str_expansion(name, NULL);
  if (val)
    return val;

  ventry *vent = fnd_val("op_procs", "group", key);
  if (!vent)
    return NULL;

  char *ret = rec_fld(vent->rec, "pid");
  log_msg("OP", "%d", tbl_ent_count(vent));
  return ret;
}

static void fileopen_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("OP", "fileopen_cb");
  char *path = hka->arg;
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

  Exparg exparg = {.expfn = expand_field, .key = grp->key};
  set_exparg(&exparg);
  Cmdstr bfcmd;

  char *line = grp->opgrp->before;
  cmd_eval(&bfcmd, line);
  log_msg("OP", "ret: %s", bfcmd.ret);
  free(bfcmd.ret);
  set_exparg(NULL);

  return;
  //
  // before_cb:
  //   if result is pid#, ski to (A)
  //   else result string sent to create_proc
  //   (A) pid added to %:prev and %:all

  create_proc(grp->opgrp, path);
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

static void* op_kill(List *args, Cmdarg *ca)
{
  log_msg("OP", "kill");
  int pid;
  char *pidstr = list_arg(args, 1, VAR_STRING);
  if (!pidstr || !str_num(pidstr, &pid))
    return 0;

  ventry *vent = fnd_val("op_procs", "pid", pidstr);
  if (!vent) {
    log_msg("OP", "pid %s not found", pidstr);
    return 0;
  }
  uv_kill(pid, SIGKILL);
  //default handling should cause correct closing
  return 0;
}

Op_group* op_newgrp(const char *before, const char *after)
{
  Op_group *opgrp = malloc(sizeof(Op_group));
  utarray_new(opgrp->procs, &proc_icd);
  opgrp->before = strdup(before);
  opgrp->after = strdup(after);
  return opgrp;
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
  hook_add_intl(plugin, plugin, fileopen_cb,  "fileopen");
  hook_add_intl(plugin, plugin, execopen_cb,  "execopen");
  hook_add_intl(plugin, plugin, execclose_cb, "execclose");
  hook_set_tmp("fileopen");
  hook_set_tmp("execopen");
  hook_set_tmp("execclose");
  if (tbl_mk("op_procs")) {
    tbl_mk_fld("op_procs", "group", typSTRING);
    tbl_mk_fld("op_procs", "pid",   typSTRING);
  }

  Cmd_T killcmd = {"kill",0, op_kill, 0};
  cmd_add(&killcmd);
}

void op_delete(Plugin *cntlr)
{
  log_msg("OP", "op_cleanup");
}
