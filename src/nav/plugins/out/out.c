#include "nav/plugins/out/out.h"
#include "nav/tui/buffer.h"
#include "nav/event/event.h"
#include "nav/event/hook.h"
#include "nav/log.h"
#include "nav/table.h"
#include "nav/model.h"
#include "nav/util.h"

static Out out;
static void out_signal_model(void **);

void out_init()
{
  log_msg("OUT", "init");
  if (tbl_mk("out")) {
    tbl_mk_fld("out", "pid",  TYP_STR);
    tbl_mk_fld("out", "fd",   TYP_STR);
    tbl_mk_fld("out", "line", TYP_STR);
  }
  fn_handle *hndl = malloc(sizeof(fn_handle));
  hndl->tn = "out";
  hndl->key_fld = "line";
  hndl->key = "";
  hndl->fname = "line";
  hndl->kname = "fd";
  out.hndl = hndl;
}

void out_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("OUT", "new");

  out.base = plugin;
  plugin->top = &out;
  plugin->name = "out";
  plugin->fmt_name = "OUT";

  fn_handle *hndl = out.hndl;
  hndl->buf = buf;
  plugin->hndl = hndl;
  model_init(hndl);
  model_open(hndl);

  buf_set_plugin(buf, plugin, SCR_OUT);
  buf_set_status(buf, 0, arg, 0);
  out.opened = true;
  out_signal_model(NULL);
}

void out_delete(Plugin *plugin)
{
  out.opened = false;
  fn_handle *h = plugin->hndl;
  model_close(h);
  model_cleanup(h);
}

static void out_signal_model(void **data)
{
  if (out.opened) {
    fn_handle *h = out.hndl;
    model_flush(h, true);
    model_recv(h->model);
    buf_move(h->buf, model_count(h->model), 0);
  }
}

void out_recv(int pid, int fd, char *out)
{
  if (!out)
    return;

  if (fd == 1)
    log_msg("OUT", "[%d|%d]: %s", pid, fd, out);
  else
    log_err("OUT", "[%d|%d]: %s", pid, fd, out);

  char *pidstr;
  asprintf(&pidstr, "%d", pid);
  char *fdstr;
  asprintf(&fdstr, "%d", fd);

  char c;
  int pos = 0;
  int prev = 0;
  while ((c = out[pos++])) {
    if (c != '\n')
      continue;
    int len = (pos - prev) - 1;
    char buf[len];
    strncpy(buf, &out[prev], len);
    buf[len] = '\0';
    prev = pos;

    trans_rec *r = mk_trans_rec(tbl_fld_count("out"));
    edit_trans(r, "pid",   pidstr, NULL);
    edit_trans(r, "fd",    fdstr, NULL);
    edit_trans(r, "line",  buf,   NULL);
    CREATE_EVENT(eventq(), commit, 2, "out", r);
  }
  free(pidstr);
  free(fdstr);

  CREATE_EVENT(eventq(), out_signal_model, 0, NULL);
}
