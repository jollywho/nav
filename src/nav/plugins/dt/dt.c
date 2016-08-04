#include "nav/plugins/dt/dt.h"
#include "nav/tui/buffer.h"
#include "nav/util.h"
#include "nav/log.h"
#include "nav/model.h"
#include "nav/cmdline.h"
#include "nav/event/fs.h"
#include "nav/event/hook.h"
#include "nav/option.h"

static void dt_signal_model(void **data)
{
  DT *dt = data[0];
  Handle *h = dt->base->hndl;
  model_flush(h, true);
  model_recv(h->model);
  buf_move(h->buf, 0, 0);
}

static char* read_line(DT *dt, FILE *f)
{
  int length = 0, size = 128;
  char *string = malloc(size);

  while (1) {
    int c = getc(f);
    if (c == EOF || c == '\n' || c == '\0')
      break;
    if (c == '\r')
      continue;

    string[length++] = c;
    if (length >= size)
      string = realloc(string, size *= 2);
  }
  string[length] = '\0';
  return string;
}

static void dt_readfile(DT *dt)
{
  FILE *f = fopen(dt->path, "rw");
  if (!f)
    return;

  Handle *h = dt->base->hndl;
  int fcount = tbl_fld_count(h->tn);
  Table *t = get_tbl(h->tn);

  while (!feof(f)) {
    trans_rec *r = mk_trans_rec(fcount);
    char *line = read_line(dt, f);
    char *str = line;
    char *next = line;

    for (int i = 0; i < fcount; i++) {
      if (str && fcount - i > 1) {
        next = strchr(str, dt->delm);
        if (next)
          *next++ = '\0';
      }
      char *val = str ? str : "";
      edit_trans(r, tbl_fld(t, i), val, NULL);
      str = next;
    }
    CREATE_EVENT(eventq(), commit, 2, h->tn, r);
    free(line);
  }
  CREATE_EVENT(eventq(), dt_signal_model, 1, dt);
  fclose(f);
}

//new dt [table] [file] [delim:ch] #open table; open file into table
//table[!] name [fields...]        #mk or del table
static bool dt_getopts(DT *dt, char *line)
{
  bool succ = false;
  if (!line)
    return succ;

  log_err("DT", "%s", line);

  Handle *h = dt->base->hndl;
  dt->delm = ' ';

  Cmdline cmd;
  cmdline_build(&cmd, line);
  int argidx = 0;

  List *lst = cmdline_lst(&cmd);
  char *tn = list_arg(lst, argidx++, VAR_STRING);
  if (!get_tbl(tn))
    goto cleanup;

  dt->path = list_arg(lst, argidx++, VAR_STRING);
  if (!dt->path)
    goto cleanup;

  dt->path = fs_trypath(dt->path);

  Pair *p = list_arg(lst, argidx, VAR_PAIR);
  if (p) {
    char *val = token_val(&p->value, VAR_STRING);
    if (val)
      dt->delm = *val;
  }

  h->fname = tbl_fld(get_tbl(tn), 0);
  h->kname = h->fname;
  h->tn = strdup(tn);
  h->key_fld = h->fname;
  h->key = "";
  succ = true;
cleanup:
  cmdline_cleanup(&cmd);
  return succ;
}

static void pipe_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("DT", "pipe_cb");
  //TODO:
  //buf_select fields -> delimited string -> DT reader
  //pipe arg (dict fld:val)               -> DT reader

  DT *dt = host->top;
  Handle *h = caller->hndl;
  Model *m = h->model;;

  int fcount = tbl_fld_count(h->tn);
  Table *t = get_tbl(h->tn);
  trans_rec *r = mk_trans_rec(fcount);

  for (int i = 0; i < fcount; i++) {
    char *fld = tbl_fld(t, i);
    char *val = model_curs_value(m, fld);
    edit_trans(r, fld, val ? val : "", NULL);
  }

  CREATE_EVENT(eventq(), commit, 2, h->tn, r);
  CREATE_EVENT(eventq(), dt_signal_model, 1, dt);
}

static void opt_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  char *name = hka->arg;
  log_msg("DT", "opt_cb %s", name);
  if (!strcmp(name, "table")) {
    char *tbl = get_opt_str("table");
    log_msg("DT", "new table name %s", tbl);
    //close table
    //reopen file + read
  }
}

void dt_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("DT", "init");

  DT *dt = calloc(1, sizeof(DT));
  dt->base = plugin;
  plugin->top = dt;
  plugin->name = "dt";
  plugin->fmt_name = "DT";
  Handle *hndl = calloc(1, sizeof(Handle));
  hndl->buf = buf;
  plugin->hndl = hndl;

  add_opt(&plugin->opts, "table", OPTION_STRING);
  hook_add_intl(buf->id, plugin, NULL, opt_cb,  EVENT_OPTION_SET);

  if (!dt_getopts(dt, arg))
    return buf_set_plugin(buf, plugin, SCR_NULL);

  buf_set_plugin(buf, plugin, SCR_SIMPLE);
  buf_set_status(buf, 0, hndl->tn, 0);

  model_init(hndl);
  model_open(hndl);
  dt_readfile(dt);
  hook_add_intl(buf->id, plugin, NULL, pipe_cb, EVENT_PIPE);
}

void dt_delete(Plugin *plugin)
{
  log_msg("DT", "delete");
  DT *dt = plugin->top;
  Handle *h = plugin->hndl;
  if (h->model) {
    model_close(h);
    model_cleanup(h);
  }
  free(dt->path);
  free(h->tn);
  free(h);
  free(dt);
}
