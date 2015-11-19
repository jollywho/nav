#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/model.h"
#include "fnav/event/hook.h"
#include "fnav/tui/img_cntlr.h"
#include "fnav/tui/buffer.h"
#include "fnav/tui/layout.h"

String img_exts[] = {"png","jpeg","jpg","gif","bmp"};

static void cursor_change_cb(Cntlr *host, Cntlr *caller)
{
  log_msg("IMG", "cursor_change_cb");
  Img_cntlr *img = (Img_cntlr*)caller->top;

  pos_T pos = buf_ofs(img->buf);
  pos_T size = buf_size(img->buf);
  String path = model_curs_value(host->hndl->model, "fullpath");

  char *argscl;
  asprintf(&argscl, "%s %d %d %d %d",
      "/home/chi/wimgCL.sh", pos.col*2 , pos.lnum + 1, size.col*2, size.lnum);
  system(argscl);
  free(argscl);

  char *args;
  asprintf(&args, "%s %d %d %d %d %s",
      "/home/chi/wimage.sh", pos.col*2 , pos.lnum + 1, size.col+1, size.lnum+1, path);
  system(args);
  free(args);
}

static void pipe_attach_cb(Cntlr *host, Cntlr *caller)
{
  log_msg("IMG", "pipe_attach_cb");
  hook_add(caller, host, cursor_change_cb, "cursor_change");
}

void img_loop(Loop *loop, int ms)
{
  process_loop(loop, ms);
}

void img_testrun(Cntlr *cntlr)
{
  log_msg("IMG", "***test");
}

Cntlr* img_init(Buffer *buf)
{
  log_msg("IMG", "INIT");
  Img_cntlr *img = malloc(sizeof(Img_cntlr));
  img->base.name = "img";
  img->base.fmt_name = "   IMG   ";
  img->base.top = img;
  img->buf = buf;
  buf_set_cntlr(buf, &img->base);
  buf->attached = false; // override

  loop_add(&img->loop, img_loop);
  uv_timer_init(&img->loop.uv, &img->loop.delay);
  hook_init(&img->base);
  hook_add(&img->base, &img->base, pipe_attach_cb, "pipe_attach");

  return &img->base;
}
