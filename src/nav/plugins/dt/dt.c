#include "nav/plugins/dt/dt.h"
#include "nav/tui/buffer.h"
#include "nav/tui/window.h"
#include "nav/util.h"
#include "nav/log.h"
#include "nav/model.h"
#include "nav/cmdline.h"
#include "nav/event/fs.h"
#include "nav/event/hook.h"

static void dt_signal_model(void **data)
{
  DT *dt = data[0];
  Handle *h = dt->base->hndl;
  model_flush(h, true);
  model_recv(h->model);
  buf_move(h->buf, 0, 0);
}

static char* read_line(DT *dt)
{
  int length = 0, size = 128;
  char *string = malloc(size);

  while (1) {
    int c = getc(dt->f);
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
  dt->f = fopen(dt->path, "rw");
  if (!dt->f)
    return;

  int fcount = tbl_fld_count(dt->tbl);
  Table *t = get_tbl(dt->tbl);

  while (!feof(dt->f)) {
    trans_rec *r = mk_trans_rec(fcount);
    char *line = read_line(dt);
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
    CREATE_EVENT(eventq(), commit, 2, dt->tbl, r);
    free(line);
  }
  CREATE_EVENT(eventq(), dt_signal_model, 1, dt);
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
  dt->tbl = list_arg(lst, argidx++, VAR_STRING);
  if (!get_tbl(dt->tbl))
    goto cleanup;

  dt->path = list_arg(lst, argidx++, VAR_STRING);
  dt->path = valid_full_path(window_cur_dir(), dt->path);

  Pair *p = list_arg(lst, argidx, VAR_PAIR);
  if (p) {
    char *val = token_val(&p->value, VAR_STRING);
    if (val)
      dt->delm = *val;
  }

  dt->tbl = strdup(dt->tbl);
  h->fname = tbl_fld(get_tbl(dt->tbl), 0);
  h->kname = h->fname;
  h->tn = dt->tbl;
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
  Model *m = caller->hndl->model;

  int fcount = tbl_fld_count(dt->tbl);
  Table *t = get_tbl(dt->tbl);
  trans_rec *r = mk_trans_rec(fcount);

  for (int i = 0; i < fcount; i++) {
    char *fld = tbl_fld(t, i);
    char *val = model_curs_value(m, fld);
    edit_trans(r, fld, val ? val : "", NULL);
  }

  CREATE_EVENT(eventq(), commit, 2, dt->tbl, r);
  CREATE_EVENT(eventq(), dt_signal_model, 1, dt);
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

  if (!dt_getopts(dt, arg))
    return buf_set_plugin(buf, plugin, SCR_NULL);

  buf_set_plugin(buf, plugin, SCR_SIMPLE);
  buf_set_status(buf, 0, dt->tbl, 0);

  model_init(hndl);
  model_open(hndl);
  dt_readfile(dt);
  hook_add_intl(buf->id, plugin, NULL, pipe_cb, "pipe");

  //TODO: VFM navscript plugin
  //au %b right * "doau fileopen"
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
  free(dt->tbl);
  free(h);
  free(dt);
}
