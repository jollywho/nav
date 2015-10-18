#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pty.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <uv.h>

#include "fnav/event/loop.h"
#include "fnav/event/process.h"
#include "fnav/event/pty_process.h"
#include "fnav/log.h"

static void init_termios(struct termios *termios);
static void init_child(PtyProcess *ptyproc);
static void chld_handler(uv_signal_t *handle, int signum);
static bool set_duplicating_descriptor(int fd, uv_pipe_t *pipe);

bool pty_process_spawn(PtyProcess *ptyproc)
{
  log_msg("PTY_PROCESS", "spawn");
  static struct termios termios;
  if (!termios.c_cflag) {
    init_termios(&termios);
  }

  Process *proc = (Process *)ptyproc;
  uv_signal_start(&proc->loop->children_watcher, chld_handler, SIGCHLD);
  ptyproc->winsize = (struct winsize){ptyproc->height, ptyproc->width, 0, 0};
  uv_disable_stdio_inheritance();
  int master;
  int pid = forkpty(&master, NULL, &termios, &ptyproc->winsize);

  if (pid < 0) {
    log_msg("pty_process", "forkpty failed: %s", strerror(errno));
    return false;
  } else if (pid == 0) {
    init_child(ptyproc);
    abort();
  }

  // make sure the master file descriptor is non blocking
  int master_status_flags = fcntl(master, F_GETFL);
  if (master_status_flags == -1) {
    log_msg("pty_process","Failed to get master descriptor status flags: %s", strerror(errno));
    goto error;
  }
  if (fcntl(master, F_SETFL, master_status_flags | O_NONBLOCK) == -1) {
    log_msg("pty_process","Failed to make master descriptor non-blocking: %s", strerror(errno));
    goto error;
  }

  if (proc->in && !set_duplicating_descriptor(master, &proc->in->uv.pipe)) {
    goto error;
  }
  if (proc->out && !set_duplicating_descriptor(master, &proc->out->uv.pipe)) {
    goto error;
  }

  ptyproc->tty_fd = master;
  proc->pid = pid;
  return true;

error:
  close(master);
  kill(pid, SIGKILL);
  waitpid(pid, NULL, 0);
  return false;
}

void pty_process_resize(PtyProcess *ptyproc, uint16_t width, uint16_t height)
{
  ptyproc->winsize = (struct winsize){height, width, 0, 0};
  ioctl(ptyproc->tty_fd, TIOCSWINSZ, &ptyproc->winsize);
}

void pty_process_close_master(PtyProcess *ptyproc) 
{
  if (ptyproc->tty_fd >= 0) {
    close(ptyproc->tty_fd);
    ptyproc->tty_fd = -1;
  }
}

void pty_process_close(PtyProcess *ptyproc)
{
  pty_process_close_master(ptyproc);
  Process *proc = (Process *)ptyproc;
  if (proc->internal_close_cb) {
    proc->internal_close_cb(proc);
  }
}

void pty_process_teardown(Loop *loop)
{
  uv_signal_stop(&loop->children_watcher);
}

static void init_child(PtyProcess *ptyproc)
{
  unsetenv("COLUMNS");
  unsetenv("LINES");
  unsetenv("TERMCAP");
  unsetenv("COLORTERM");
  unsetenv("COLORFGBG");

  signal(SIGCHLD, SIG_DFL);
  signal(SIGHUP, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
  signal(SIGALRM, SIG_DFL);

  setenv("TERM", ptyproc->term_name ? ptyproc->term_name : "ansi", 1);
  execvp(ptyproc->process.argv[0], ptyproc->process.argv);
  log_msg("pty_process", "execvp failed: %s\n", strerror(errno));
}

static void init_termios(struct termios *termios)
{
  // Taken from pangoterm
  termios->c_iflag = ICRNL|IXON;
  termios->c_oflag = OPOST|ONLCR;
#ifdef TAB0
  termios->c_oflag |= TAB0;
#endif
  termios->c_cflag = CS8|CREAD;
  termios->c_lflag = ISIG|ICANON|IEXTEN|ECHO|ECHOE|ECHOK;

  cfsetspeed(termios, 38400);

#ifdef IUTF8
  termios->c_iflag |= IUTF8;
#endif
#ifdef NL0
  termios->c_oflag |= NL0;
#endif
#ifdef CR0
  termios->c_oflag |= CR0;
#endif
#ifdef BS0
  termios->c_oflag |= BS0;
#endif
#ifdef VT0
  termios->c_oflag |= VT0;
#endif
#ifdef FF0
  termios->c_oflag |= FF0;
#endif
#ifdef ECHOCTL
  termios->c_lflag |= ECHOCTL;
#endif
#ifdef ECHOKE
  termios->c_lflag |= ECHOKE;
#endif

  termios->c_cc[VINTR]    = 0x1f & 'C';
  termios->c_cc[VQUIT]    = 0x1f & '\\';
  termios->c_cc[VERASE]   = 0x7f;
  termios->c_cc[VKILL]    = 0x1f & 'U';
  termios->c_cc[VEOF]     = 0x1f & 'D';
  termios->c_cc[VEOL]     = _POSIX_VDISABLE;
  termios->c_cc[VEOL2]    = _POSIX_VDISABLE;
  termios->c_cc[VSTART]   = 0x1f & 'Q';
  termios->c_cc[VSTOP]    = 0x1f & 'S';
  termios->c_cc[VSUSP]    = 0x1f & 'Z';
  termios->c_cc[VREPRINT] = 0x1f & 'R';
  termios->c_cc[VWERASE]  = 0x1f & 'W';
  termios->c_cc[VLNEXT]   = 0x1f & 'V';
  termios->c_cc[VMIN]     = 1;
  termios->c_cc[VTIME]    = 0;
}

static bool set_duplicating_descriptor(int fd, uv_pipe_t *pipe)
{
  int fd_dup = dup(fd);
  if (fd_dup < 0) {
    log_msg("pty_process","Failed to dup descriptor %d: %s", fd, strerror(errno));
    return false;
  }
  int uv_result = uv_pipe_open(pipe, fd_dup);
  if (uv_result) {
    log_msg("pty_process","Failed to set pipe to descriptor %d: %s",
         fd_dup, uv_strerror(uv_result));
    close(fd_dup);
    return false;
  }
  return true;
}

static void chld_handler(uv_signal_t *handle, int signum)
{
  int stat = 0;
  int pid;

  do {
    pid = waitpid(-1, &stat, WNOHANG);
  } while (pid < 0 && errno == EINTR);

  if (pid <= 0) {
    return;
  }

  Loop *loop = handle->loop->data;

  Process *it;
  SLIST_FOREACH(it, &loop->children, ent) {
    Process *proc = it;
    if (proc->pid == pid) {
      if (WIFEXITED(stat)) {
        proc->status = WEXITSTATUS(stat);
      } else if (WIFSIGNALED(stat)) {
        proc->status = WTERMSIG(stat);
      }
      proc->internal_exit_cb(proc);
      break;
    }
  }
}
