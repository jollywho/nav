#include <string.h>
#include "fnav/plugins/img/img.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/model.h"
#include "fnav/event/hook.h"
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

static void img_draw(Plugin *plugin)
{
  log_msg("IMG", "img_draw");
  Img *img = (Img*)plugin->top;

  pos_T pos = buf_ofs(img->buf);
  pos_T size = buf_size(img->buf);

  int max_width_pixels  = size.col  * img->fontw;
  int max_height_pixels = size.lnum * img->fonth;

  if (img->width > max_width_pixels) {
    img->height = (img->height * max_width_pixels) / img->width;
    img->width = max_width_pixels;
  }
  if (img->height > max_height_pixels) {
    img->width = (img->width * max_height_pixels) / img->height;
    img->height = max_height_pixels;
  }

  free(img->cl_msg);
  free(img->img_msg);
  asprintf(&img->cl_msg, CL_ARG,
      pos.col * img->fontw + 1, pos.lnum * img->fonth,
      (size.col * img->fontw), (size.lnum * img->fonth));
  asprintf(&img->img_msg, DR_ARG,
      pos.col * img->fontw, pos.lnum * img->fonth,
      img->width, img->height, img->path);

  shell_set_in_buffer(img->sh_clear, img->cl_msg);
  shell_start(img->sh_clear);

  shell_set_in_buffer(img->sh_draw, img->img_msg);
  shell_start(img->sh_draw);
}

static void shell_stdout_size_cb(Plugin *plugin, String out)
{
  log_msg("IMG", "shell_stdout_size_cb");
  log_msg("IMG", "%s", out);
  Img *img = (Img*)plugin->top;

  char *w = strtok(out, " ");
  char *h = strtok(NULL, " ");
  sscanf(w, "%d", &img->width);
  sscanf(h, "%d", &img->height);
  img->img_set = true;
  img_draw(plugin);
}

static void shell_stdout_font_cb(Plugin *plugin, String out)
{
  log_msg("IMG", "shell_stdout_font_cb");
  log_msg("IMG", "%s", out);
  Img *img = (Img*)plugin->top;

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
  for (int i = 0; i < (int)EXT_ARR_SIZE; i++) {
    if (strcmp(get_path_ext(path), img_exts[i]) == 0)
      return 1;
  }
  return 0;
}

static int create_msg(Plugin *host, Plugin *caller, void *data)
{
  Img *img = (Img*)caller->top;
  fn_handle *h = caller->hndl;

  String path = model_curs_value(host->hndl->model, "fullpath");
  String name = model_curs_value(host->hndl->model, "name");

  if (!valid_ext(path)) return 0;

  img->path = path;
  h->key = name;
  buf_set_status(h->buf, 0, h->key, 0, 0);
  h->buf->attached = false; // override

  free(img->sz_msg);
  asprintf(&img->sz_msg, SZ_ARG, img->path);

  return 1;
}

static void cursor_change_cb(Plugin *host, Plugin *caller, void *data)
{
  log_msg("IMG", "cursor_change_cb");
  Img *img = (Img*)caller->top;

  if (img->disabled) return;

  if (create_msg(host, caller, NULL)) {
    shell_set_in_buffer(img->sh_size, img->sz_msg);
    shell_start(img->sh_size);
  }
}

static void try_refresh(Plugin *host, Plugin *none, void *data)
{
  Img *img = (Img*)host->top;
  if (!img->img_set) return;
  shell_set_in_buffer(img->sh_clear, img->cl_msg);
  shell_start(img->sh_clear);

  shell_set_in_buffer(img->sh_size, img->sz_msg);
  shell_start(img->sh_size);
}

static void pipe_attach_cb(Plugin *host, Plugin *caller, void *data)
{
  log_msg("IMG", "pipe_attach_cb");
  hook_add(caller, host, cursor_change_cb, "cursor_change");
}

Plugin* img_new(Buffer *buf)
{
  log_msg("IMG", "INIT");
  Img *img = malloc(sizeof(Img));
  fn_handle *hndl = malloc(sizeof(fn_handle));
  hndl->buf = buf;
  hndl->key = " ";
  img->base.hndl = hndl;

  img->base.name = "img";
  img->base.fmt_name = "   IMG   ";
  img->base.top = img;
  img->buf = buf;
  img->disabled = false;
  img->img_set = false;
  img->sz_msg = malloc(1);
  img->cl_msg = malloc(1);
  img->img_msg = malloc(1);
  buf_set_plugin(buf, &img->base);
  buf_set_pass(buf);

  img->sh_size = shell_new(&img->base);
  shell_args(img->sh_size, (String*)t_args, shell_stdout_font_cb);
  shell_start(img->sh_size);

  img->sh_draw = shell_new(&img->base);
  shell_args(img->sh_draw, (String*)args, NULL);

  img->sh_clear = shell_new(&img->base);
  shell_args(img->sh_clear, (String*)args, NULL);

  hook_init(&img->base);
  hook_add(&img->base, &img->base, pipe_attach_cb, "pipe_attach");
  hook_add(&img->base, &img->base, try_refresh, "window_resize");

  return &img->base;
}

void img_delete(Plugin *plugin)
{
  log_msg("IMG", "img_cleanup");
  Img *img = (Img*)plugin->top;
  shell_set_in_buffer(img->sh_clear, img->cl_msg);
  shell_start(img->sh_clear);
  img->disabled = true;
  // hook remove
  // free(img);
}
