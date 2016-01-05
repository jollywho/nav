
#include "fnav/tui/sh_cntlr.h"
#include "fnav/tui/sbuffer.h"
#include "fnav/model.h"
#include "fnav/log.h"

Cntlr* sh_new(Buffer *buf)
{
  log_msg("SH_CNTLR", "init");
  Sh_cntlr *sh = malloc(sizeof(Sh_cntlr));
  sh->base.top = sh;
  sh->hndl = malloc(sizeof(fn_handle));
  sh->base.hndl = sh->hndl;
  model_init(sh->base.hndl);
  sh->hndl->buf = buf;

  sbuffer_init(buf);
  //sh->sh = shell_init(&sh->base, sh->hndl);
  //int width = buf_size(buf).col;
  //int height = buf_size(buf).lnum;
  //shell_start(sh->sh);
  //sbuffer_readtest(sh->sh->proc->in->fd);
  //pty_process_resize(&sh->sh->ptyproc, width, height);
  return &sh->base;
}

void sh_delete(Sh_cntlr *cntlr)
{
}
