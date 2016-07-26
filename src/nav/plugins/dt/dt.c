#include "nav/plugins/dt/dt.h"
#include "nav/tui/buffer.h"
#include "nav/tui/window.h"
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

static bool validate_opts(DT *dt)
{
  //TODO: if no file and tbl doesnt exist, error
  return (dt->path && dt->tbl);
}

static bool dt_getopts(DT *dt, char *line)
{
  if (!line)
    return false;

  Handle *h = dt->base->hndl;
  List *flds = NULL;
  dt->tbl = "filename"; //TODO: use name scheme
  dt->delm = ' ';
  const char *fname = "name";
  char *tmp;

  Cmdline cmd;
  cmdline_build(&cmd, line);

  List *lst = cmdline_lst(&cmd);
  for (int i = 0; i < utarray_len(lst->items); i++) {
    Token *tok = tok_arg(lst, i);
    switch (tok->var.v_type) {
      case VAR_LIST:
        flds = tok->var.vval.v_list;
        fname = list_arg(flds, 0, VAR_STRING);
        break;
      case VAR_PAIR:
        tmp = token_val(&tok->var.vval.v_pair->value, VAR_STRING);
        dt->delm = *tmp;
        break;
      case VAR_STRING:
        tmp = valid_full_path(window_cur_dir(), token_val(tok, VAR_STRING));
        if (tmp)
          dt->path = tmp;
        else
          dt->tbl = token_val(tok, VAR_STRING);
    }
  }
  log_err("DT", "%s %s %c", dt->path, dt->tbl, dt->delm);

  dt->tbl = strdup(dt->tbl);
  bool succ = validate_opts(dt);
  if (!succ)
    goto cleanup;

  if (tbl_mk(dt->tbl)) {
    if (flds) {
      for (int i = 0; i < utarray_len(flds->items); i++)
        tbl_mk_fld(dt->tbl, list_arg(flds, i, VAR_STRING), TYP_STR);
    }
    else
      tbl_mk_fld(dt->tbl, fname, TYP_STR);
  }

  h->fname = tbl_fld(get_tbl(dt->tbl), 0);
  h->kname = h->fname;
  h->tn = dt->tbl;
  h->key_fld = h->fname;
  h->key = "";
cleanup:
  cmdline_cleanup(&cmd);
  return succ;
}

static void pipe_attach_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("DT", "pipe_attach_cb");
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
  int id = id_from_plugin(plugin);
  hook_add_intl(id, plugin, NULL, pipe_attach_cb, "pipe");

  //TODO: fileopen user command
  //au %b right * "doau fileopen"
  //
  //TODO: VFM navscript plugin
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
