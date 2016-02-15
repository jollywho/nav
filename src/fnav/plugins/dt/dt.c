#include <unistd.h>

#include "fnav/plugins/dt/dt.h"
#include "fnav/tui/buffer.h"
#include "fnav/event/event.h"
#include "fnav/event/hook.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/model.h"

static void dt_readfile(DT *dt);

void dt_new(Plugin *plugin, Buffer *buf, void *arg)
{
  log_msg("DT", "init");
  if (!arg)
    return;

  DT *dt = malloc(sizeof(DT));
  dt->base = plugin;
  plugin->top = dt;
  plugin->name = "dt";
  plugin->fmt_name = "DT";
  fn_handle *hndl = malloc(sizeof(fn_handle));
  hndl->buf = buf;
  plugin->hndl = hndl;
  hndl->tn = "filename";
  hndl->buf = buf;
  hndl->key_fld = "line";
  hndl->key = "0";
  hndl->fname = "name";
  hndl->kname = "line";
  dt->filename = strdup(arg);

  if (tbl_mk("filename")) {
    tbl_mk_fld("filename", "name", typSTRING);
    tbl_mk_fld("filename", "line", typSTRING);
  }

  model_init(hndl);
  model_open(hndl);
  dt->m = hndl->model;
  buf_set_plugin(buf, plugin);
  buf_set_status(buf, 0, arg, 0);
  hook_init_host(plugin);

  // open file
  // parse with given or default schema
  // create table fields from schema
  // send to model
  dt_readfile(dt);
}

void dt_delete(Plugin *plugin)
{
  log_msg("DT", "delete");
  DT *dt = plugin->top;
  free(dt);
}

static void dt_signal_model(void **data)
{
  DT *dt = data[0];
  model_recv(dt->m);
}

static void dt_readfile(DT *dt)
{
  dt->f = fopen(dt->filename, "rw");
  char * line = NULL;
  size_t len = 0;
  ssize_t size;

  while ((size = getline(&line, &len, dt->f)) != -1) {
    trans_rec *r = mk_trans_rec(tbl_fld_count("filename"));
    edit_trans(r, "name", (String)line, NULL);
    edit_trans(r, "line", (String)"0",  NULL);
    CREATE_EVENT(eventq(), commit, 2, "filename", r);
  }
  free(line);
  CREATE_EVENT(eventq(), dt_signal_model, 1, dt);
}
