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
//FORMAT:             0;1;{x};{y};{w};{h};;;;;{filename}\n4;\n3;\n
const char *DR_ARG = "0;1;%d;%d;%d;%d;;;;;%s\n4;\n3;\n";
//FORMAT:             6;{x};{y};{w};{h}\n4;\n3;\n
const char *CL_ARG = "6;%d;%d;%d;%d\n4;\n3;\n";
//FORMAT              5;filename
const char *SZ_ARG = "5;%s\n";
const char *WM_IMG = "/usr/lib/w3m/w3mimgdisplay";

static void exit_cb(uv_process_t *req, int64_t exit_status, int term_signal)
{
  log_msg("IMG", "exit_cb");
  // queue draw here unless a way is found to keep program running
}

static void exec_wm(Img_cntlr *img, String cmd)
{
  log_msg("IMG", "inp: %s", cmd);
}

void shell_output_cb(Cntlr *cntlr, String out)
{
  log_msg("SHELL", "shell_output_cb");
  log_msg("SHELL", "%s", out);
  Img_cntlr *img = (Img_cntlr*)cntlr->top;
  //char *w = strtok(out, " ");
  //char *h = strtok(NULL, " ");
  //sscanf(w, "%d", &img->fontw);
  //sscanf(h, "%d", &img->fonth);
}

static void cursor_change_cb(Cntlr *host, Cntlr *caller)
{
  log_msg("IMG", "cursor_change_cb");
  Img_cntlr *img = (Img_cntlr*)caller->top;

  if (img->disabled) return;

  pos_T pos = buf_ofs(img->buf);
  pos_T size = buf_size(img->buf);
  String path = model_curs_value(host->hndl->model, "fullpath");

  // init a shell for every call.
  // save cmd arg to img cntlr for refresh calls like sigwinch
  // set specific callback for each command
  //  e.g. size_cb, font_cb
  // in shell, don't init rstream if readout cb is null
  //
  // img_init:
  //  shell: font
  //
  // cursor_change_cb:
  //  shell: size
  //  shell: clear
  //  shell: draw
  //
  // shell should be somewhat generalized for op cntlr use.
  // hooks needs extended for multiple callers per host to support
  // one to many image buffers. also buffer's need to register numbers.

  char *args[3] =  {(String)WM_IMG, NULL, NULL};
  shell_start(img->sh_draw, args);

  char *cmd;
  asprintf(&cmd, DR_ARG, pos.col*11, pos.lnum*11,size.col*11,size.lnum*11, path);
  shell_write(img->sh_draw, cmd);
  free(cmd);
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
  img->disabled = false;
  buf_set_cntlr(buf, &img->base);
  buf->attached = false; // override

  img->sh_size = shell_init(&img->base, shell_output_cb);
  img->sh_draw = shell_init(&img->base, NULL);

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
