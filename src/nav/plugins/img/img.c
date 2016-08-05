#include <string.h>
#include "nav/plugins/img/img.h"
#include "nav/log.h"
#include "nav/table.h"
#include "nav/model.h"
#include "nav/cmdline.h"
#include "nav/event/hook.h"
#include "nav/tui/buffer.h"
#include "nav/tui/layout.h"
#include "nav/event/shell.h"
#include "nav/event/fs.h"

#define WM_IMG "/usr/lib/w3m/w3mimgdisplay"

//FORMAT:                    0;1;{x};{y};{w};{h};;;;;{filename}\n4;\n3;\n
static const char *DR_ARG = "0;1;%d;%d;%d;%d;;;;;%s\n4;\n3;\n";
//FORMAT:                    6;{x};{y};{w};{h}\n4;\n3;\n
static const char *CL_ARG = "6;%d;%d;%d;%d\n4;\n3;\n";
//FORMAT:                    5;filename
static const char *SZ_ARG = "5;%s\n";
static const char *const args[]   = {WM_IMG, NULL};
static const char *const t_args[] = {WM_IMG, "-test", NULL};
static const char *img_exts[] = {"png","jpeg","jpg","gif","bmp"};

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

  //TODO: use buffer size * font, not given image
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

static void shell_stdout_size_cb(Plugin *plugin, char *out)
{
  log_msg("IMG", "shell_stdout_size_cb");
  log_msg("IMG", "%s", out);
  Img *img = (Img*)plugin->top;
  if (!strchr(out, ' '))
    return;

  char *w = strtok(out, " ");
  char *h = strtok(NULL, " ");
  sscanf(w, "%d", &img->width);
  sscanf(h, "%d", &img->height);
  img->img_set = true;
  img_draw(plugin);
}

static void shell_stdout_font_cb(Plugin *plugin, char *out)
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

  shell_args(img->sh_size, (char**)args, shell_stdout_size_cb);
}

static int valid_ext(const char *path)
{
  for (int i = 0; i < LENGTH(img_exts); i++) {
    if (strcmp(file_ext(path), img_exts[i]) == 0)
      return 1;
  }
  return 0;
}

static int create_msg(Plugin *host, Plugin *caller, HookArg *hka)
{
  Img *img = (Img*)caller->top;
  Handle *h = caller->hndl;

  char *path = model_curs_value(host->hndl->model, "fullpath");
  char *name = model_curs_value(host->hndl->model, "name");

  if (!valid_ext(path))
    return 0;

  img->path = path;
  h->key = name;
  buf_set_status(h->buf, 0, h->key, 0);
  h->buf->attached = false; // override

  free(img->sz_msg);
  asprintf(&img->sz_msg, SZ_ARG, img->path);

  return 1;
}

static void cursor_change_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("IMG", "cursor_change_cb");
  Img *img = (Img*)caller->top;

  if (img->disabled)
    return;

  if (create_msg(host, caller, NULL)) {
    shell_set_in_buffer(img->sh_size, img->sz_msg);
    shell_start(img->sh_size);
  }
  else if (img->img_set) { /* refresh */
    shell_set_in_buffer(img->sh_clear, img->cl_msg);
    shell_start(img->sh_clear);
  }
}

//TODO: add to bottom of draw queue
static void try_refresh(Plugin *host, Plugin *none, HookArg *hka)
{
  Img *img = (Img*)host->top;
  if (!img->img_set)
    return;

  shell_set_in_buffer(img->sh_size, img->sz_msg);
  shell_start(img->sh_size);
}

static void pipe_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("IMG", "pipe_cb");
  if (strcmp(caller->name, "fm"))
    return;
  int id = id_from_plugin(host);
  hook_add_intl(id, caller, host, cursor_change_cb, EVENT_CURSOR_CHANGE);
  cursor_change_cb(caller, host, NULL);
}

static void pipe_remove_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  hook_rm_intl(caller, host, cursor_change_cb, EVENT_CURSOR_CHANGE);
}

void img_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("IMG", "INIT");
  Img *img = malloc(sizeof(Img));
  img->base = plugin;
  Handle *hndl = malloc(sizeof(Handle));
  hndl->buf = buf;
  hndl->key = " ";
  plugin->hndl = hndl;

  plugin->fmt_name = "IMG";
  plugin->top = img;
  img->buf = buf;
  img->disabled = false;
  img->img_set = false;
  img->sz_msg = malloc(1);
  img->cl_msg = malloc(1);
  img->img_msg = malloc(1);
  buf_set_plugin(buf, plugin, SCR_NULL);

  img->sh_size = shell_new(plugin);
  shell_args(img->sh_size, (char**)t_args, shell_stdout_font_cb);
  shell_start(img->sh_size);

  img->sh_draw = shell_new(plugin);
  shell_args(img->sh_draw, (char**)args, NULL);

  img->sh_clear = shell_new(plugin);
  shell_args(img->sh_clear, (char**)args, NULL);

  hook_add_intl(buf->id, plugin, NULL,   pipe_cb,        EVENT_PIPE);
  hook_add_intl(buf->id, plugin, NULL,   pipe_remove_cb, EVENT_PIPE_REMOVE);
  hook_add_intl(buf->id, plugin, plugin, try_refresh,    EVENT_WINDOW_RESIZE);

  int wnum;
  if (arg && str_num(arg, &wnum)) {
    Plugin *rhs = plugin_from_id(wnum);
    if (rhs && wnum != buf->id)
      send_hook_msg(EVENT_PIPE, plugin, rhs, NULL);
  }
}

static bool img_clean(Img *img)
{
  return !(img->sh_clear->blocking ||
           img->sh_size->blocking  ||
           img->sh_draw->blocking);
}

void img_delete(Plugin *plugin)
{
  log_msg("IMG", "delete");
  Img *img = plugin->top;
  Handle *h = img->base->hndl;

  if (img->img_set) {
    shell_set_in_buffer(img->sh_clear, img->cl_msg);
    shell_start(img->sh_clear);
  }
  img->disabled = true;

  DO_EVENTS_UNTIL(img_clean(img));
  shell_delete(img->sh_clear);
  shell_delete(img->sh_size);
  shell_delete(img->sh_draw);
  free(h);
  free(img->cl_msg);
  free(img->sz_msg);
  free(img->img_msg);
  free(img);
}
