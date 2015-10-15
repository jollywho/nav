#ifndef FN_EVENT_PTY_PROCESS_H
#define FN_EVENT_PTY_PROCESS_H

#include <sys/ioctl.h>

#include "fnav/event/process.h"

typedef struct pty_process {
  Process process;
  char *term_name;
  uint16_t width, height;
  struct winsize winsize;
  int tty_fd;
} PtyProcess;

static inline PtyProcess pty_process_init(Loop *loop, void *data)
{
  PtyProcess rv;
  rv.process = process_init(loop, kProcessTypePty, data);
  rv.term_name = NULL;
  rv.width = 80;
  rv.height = 24;
  rv.tty_fd = -1;
  return rv;
}

bool pty_process_spawn(PtyProcess *ptyproc);
void pty_process_resize(PtyProcess *ptyproc, uint16_t width, uint16_t height);
void pty_process_close_master(PtyProcess *ptyproc);
void pty_process_close(PtyProcess *ptyproc);
void pty_process_teardown(Loop *loop);

#endif
