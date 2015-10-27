
#include "fnav/tui/sh_cntlr.h"
#include "fnav/tui/sbuffer.h"
#include "fnav/model.h"
#include "fnav/log.h"

void sh_init(Buffer *buf)
{
  log_msg("SH_CNTLR", "init");
  Sh_cntlr *sh = malloc(sizeof(Sh_cntlr));
  sh->hndl = malloc(sizeof(fn_handle));
  sh->base.hndl = sh->hndl;
  model_init(sh->base.hndl);
  sh->hndl->buf = buf;

  sbuffer_init(buf);
  sh->sh = shell_init(&sh->base, sh->hndl);
  int width = buf_size(buf).col;
  int height = buf_size(buf).lnum;
  shell_start(sh->sh);
//  pty_process_resize(&sh->sh->ptyproc, width, height);
}

void sh_cleanup(Sh_cntlr *cntlr)
{
}
