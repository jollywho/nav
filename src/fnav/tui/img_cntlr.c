#include <string.h>
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/model.h"
#include "fnav/event/hook.h"
#include "fnav/tui/img_cntlr.h"
#include "fnav/tui/buffer.h"
#include "fnav/tui/layout.h"
#include "fnav/event/shell.h"

String img_exts[] = {"png","jpeg","jpg","gif","bmp"};
#define EXT_ARR_SIZE ARRAY_SIZE(img_exts)
//FORMAT:             0;1;{x};{y};{w};{h};;;;;{filename}\n4;\n3;\n
static const char *DR_ARG = "0;1;%d;%d;%d;%d;;;;;%s\n4;\n3;\n";
//FORMAT:             6;{x};{y};{w};{h}\n4;\n3;\n
static const char *CL_ARG = "6;%d;%d;%d;%d\n4;\n3;\n";
//FORMAT              5;filename
static const char *SZ_ARG = "5;%s\n";
#define WM_IMG "/usr/lib/w3m/w3mimgdisplay"
static const char * const args[]   = {WM_IMG, NULL};
static const char * const t_args[] = {WM_IMG, "-test", NULL};

static void shell_stdout_size_cb(Cntlr *cntlr, String out)
{
  log_msg("IMG", "shell_stdout_size_cb");
  log_msg("IMG", "%s", out);
  Img_cntlr *img = (Img_cntlr*)cntlr->top;

  char *w = strtok(out, " ");
  char *h = strtok(NULL, " ");
  sscanf(w, "%d", &img->width);
  sscanf(h, "%d", &img->height);

  pos_T pos = buf_ofs(img->buf);
  pos_T size = buf_size(img->buf);

  int max_width_pixels  = size.col-2  * img->fontw;
  int max_height_pixels = size.lnum-1 * img->fonth;

  if (img->width > max_width_pixels) {
    img->height = (img->height * max_width_pixels) / img->width;
    img->width = max_width_pixels;
  }
  if (img->height > max_height_pixels) {
    img->width = (img->width * max_height_pixels) / img->height;
    img->height = max_height_pixels;
  }

  char *msg;
  char *cl_msg;
  asprintf(&msg, DR_ARG,
      pos.col * img->fontw, pos.lnum * img->fonth,
      img->width, img->height, img->path);
  asprintf(&cl_msg, CL_ARG,
      pos.col * img->fontw, pos.lnum * img->fonth,
      (size.col * img->fontw), (size.lnum * img->fonth));

  log_msg("SHELL", "%s", msg);

  shell_set_in_buffer(img->sh_clear, cl_msg);
  shell_start(img->sh_clear);
  free(cl_msg);

  shell_set_in_buffer(img->sh_draw, msg);
  shell_start(img->sh_draw);
  free(msg);
}

static void shell_stdout_font_cb(Cntlr *cntlr, String out)
{
  log_msg("IMG", "shell_stdout_font_cb");
  log_msg("IMG", "%s", out);
  Img_cntlr *img = (Img_cntlr*)cntlr->top;

  char *w = strtok(out, " ");
  char *h = strtok(NULL, " ");
  sscanf(w, "%d", &img->fontw);
  sscanf(h, "%d", &img->fonth);

  pos_T lo = layout_size();
  img->fontw = (img->fontw + 2) / lo.col;
  img->fonth = (img->fonth + 2) / lo.lnum;

  shell_args(img->sh_size, (String*)args, shell_stdout_size_cb);
}

static const char *get_path_ext(const char *fspec)
{
  char *e = strrchr (fspec, '.');
  if (e == NULL)
    e = "";
  return ++e;
}

static int valid_ext(const char *path)
{
  for (int i = 0; i < EXT_ARR_SIZE; i++) {
    if (strcmp(get_path_ext(path), img_exts[i]) == 0)
      return 1;
  }
  return 0;
}

static void cursor_change_cb(Cntlr *host, Cntlr *caller)
{
  log_msg("IMG", "cursor_change_cb");
  Img_cntlr *img = (Img_cntlr*)caller->top;
  fn_handle *h = caller->hndl;

  if (img->disabled) return;

  String path = model_curs_value(host->hndl->model, "fullpath");
  String name = model_curs_value(host->hndl->model, "name");

  if (!valid_ext(path)) return;

  img->path = path;
  h->key = name;
  buf_set_status(h->buf, 0, h->key, 0, 0);
  h->buf->attached = false; // override

  char *msg;
  asprintf(&msg, SZ_ARG, img->path);

  shell_set_in_buffer(img->sh_size, msg);
  shell_start(img->sh_size);

  free(msg);
}

static void pipe_attach_cb(Cntlr *host, Cntlr *caller)
{
  log_msg("IMG", "pipe_attach_cb");
  hook_add(caller, host, cursor_change_cb, "cursor_change");
}

static void img_loop(Loop *loop, int ms)
{
  process_loop(loop, ms);
}

Cntlr* img_init(Buffer *buf)
{
  log_msg("IMG", "INIT");
  Img_cntlr *img = malloc(sizeof(Img_cntlr));
  fn_handle *hndl = malloc(sizeof(fn_handle));
  hndl->buf = buf;
  hndl->key = " ";
  img->base.hndl = hndl;

  img->base.name = "img";
  img->base.fmt_name = "   IMG   ";
  img->base.top = img;
  img->buf = buf;
  img->disabled = false;
  buf_set_cntlr(buf, &img->base);
  buf->attached = false; // override

  img->sh_size = shell_init(&img->base);
  shell_args(img->sh_size, (String*)t_args, shell_stdout_font_cb);
  shell_start(img->sh_size);

  img->sh_draw = shell_init(&img->base);
  shell_args(img->sh_draw, (String*)args, NULL);

  img->sh_clear = shell_init(&img->base);
  shell_args(img->sh_clear, (String*)args, NULL);

  hook_init(&img->base);
  hook_add(&img->base, &img->base, pipe_attach_cb, "pipe_attach");

  return &img->base;
}

void img_cleanup(Cntlr *cntlr)
{
  log_msg("IMG", "img_cleanup");
  Img_cntlr *img = (Img_cntlr*)cntlr->top;
  img->disabled = true;
  // hook remove
  // free(img);
}
