#include <stdlib.h>

#include <uv.h>

#include "fnav/event/event.h"
#include "fnav/event/process.h"
#include "fnav/event/uv_process.h"
#include "fnav/event/pty_process.h"
#include "fnav/log.h"

// {SIGNAL}_TIMEOUT is the time (in nanoseconds) that a process has to cleanly
// exit before we send SIGNAL to it
#define TERM_TIMEOUT 1000000000
#define KILL_TIMEOUT (TERM_TIMEOUT * 2)

#define CLOSE_PROC_STREAM(proc, stream)                             \
  do {                                                              \
    if (proc->stream && !proc->stream->closed) {                    \
      stream_close(proc->stream, NULL);                             \
    }                                                               \
  } while (0)



int got_int = 0;

static void on_process_stream_close(Stream *stream, void *data);
static void on_process_exit(Process *proc);
static void decref(Process *proc);
static void process_close(Process *proc);
static void children_kill_cb(uv_timer_t *handle);

bool process_spawn(Process *proc)
{
  log_msg("PROCESS", "spawn");
  if (proc->in) {
    uv_pipe_init(&proc->loop->uv, &proc->in->uv.pipe, 0);
  }

  if (proc->out) {
    uv_pipe_init(&proc->loop->uv, &proc->out->uv.pipe, 0);
  }

  if (proc->err) {
    uv_pipe_init(&proc->loop->uv, &proc->err->uv.pipe, 0);
  }

  bool success;
  switch (proc->type) {
    case kProcessTypeUv:
      success = uv_process_spawn((UvProcess *)proc);
      break;
    case kProcessTypePty:
      success = pty_process_spawn((PtyProcess *)proc);
      break;
    default:
      abort();
  }

  if (!success) {
    if (proc->in) {
      uv_close((uv_handle_t *)&proc->in->uv.pipe, NULL);
    }
    if (proc->out) {
      uv_close((uv_handle_t *)&proc->out->uv.pipe, NULL);
    }
    if (proc->err) {
      uv_close((uv_handle_t *)&proc->err->uv.pipe, NULL);
    }
    process_close(proc);
    proc->status = -1;
    return false;
  }

  void *data = proc->data;

  if (proc->in) {
    stream_init(NULL, proc->in, -1, (uv_stream_t *)&proc->in->uv.pipe, data);
    proc->in->internal_data = proc;
    proc->in->internal_close_cb = on_process_stream_close;
    proc->refcount++;
  }

  if (proc->out) {
    stream_init(NULL, proc->out, -1, (uv_stream_t *)&proc->out->uv.pipe, data);
    proc->out->internal_data = proc;
    proc->out->internal_close_cb = on_process_stream_close;
    proc->refcount++;
  }

  if (proc->err) {
    stream_init(NULL, proc->err, -1, (uv_stream_t *)&proc->err->uv.pipe, data);
    proc->err->internal_data = proc;
    proc->err->internal_close_cb = on_process_stream_close;
    proc->refcount++;
  }

  proc->internal_exit_cb = on_process_exit;
  proc->internal_close_cb = decref;
  proc->refcount++;
  SLIST_INSERT_HEAD(&proc->loop->children, proc, ent);
  return true;
}

void process_teardown(Loop *loop)
{
  log_msg("PROCESS", "teardown");
  Process *it;
  SLIST_FOREACH(it, &loop->children, ent) {
    log_msg("PROCESS", "*********");
    Process *proc = it;
    uv_kill(proc->pid, SIGTERM);
    proc->term_sent = true;
    process_stop(proc);
  }

  // Wait until all children exit
  //PROCESS_EVENTS_UNTIL(loop, loop->events, -1, SLIST_EMPTY(&loop->children));
  pty_process_teardown(loop);
}

// Wrappers around `stream_close` that protect against double-closing.
void process_close_streams(Process *proc) 
{
  log_msg("PROCESS", "process_close_streams");
  process_close_in(proc);
  process_close_out(proc);
  process_close_err(proc);
}

void process_close_in(Process *proc) 
{
  log_msg("PROCESS", "process_close_in");
  CLOSE_PROC_STREAM(proc, in);
}

void process_close_out(Process *proc) 
{
  log_msg("PROCESS", "process_close_out");
  CLOSE_PROC_STREAM(proc, out);
}

void process_close_err(Process *proc) 
{
  CLOSE_PROC_STREAM(proc, err);
}

/// Ask a process to terminate and eventually kill if it doesn't respond
void process_stop(Process *proc)
{
  log_msg("PROCESS", "process_stop");
  if (proc->stopped_time) {
    return;
  }

  proc->stopped_time = os_hrtime();
  switch (proc->type) {
    case kProcessTypeUv:
      // Close the process's stdin. If the process doesn't close its own
      // stdout/stderr, they will be closed when it exits(possibly due to being
      // terminated after a timeout)
      process_close_in(proc);
      break;
    case kProcessTypePty:
      // close all streams for pty processes to send SIGHUP to the process
      process_close_streams(proc);
      pty_process_close_master((PtyProcess *)proc);
      break;
    default:
      abort();
  }

  Loop *loop = proc->loop;
  if (!loop->children_stop_requests++) {
    // When there's at least one stop request pending, start a timer that
    // will periodically check if a signal should be send to a to the job
    log_msg("PROCESS", "Starting job kill timer");
    uv_timer_start(&loop->children_kill_timer, children_kill_cb, 100, 100);
  }
}

/// Iterates the process list sending SIGTERM to stopped processes and SIGKILL
/// to those that didn't die from SIGTERM after a while(exit_timeout is 0).
static void children_kill_cb(uv_timer_t *handle)
{
  log_msg("PROCESS", "children_kill_cb");
  Loop *loop = handle->loop->data;
  uint64_t now = os_hrtime();

  Process *it;
  SLIST_FOREACH(it, &loop->children, ent) {
    Process *proc = it;
    if (!proc->stopped_time) {
      continue;
    }
    uint64_t elapsed = now - proc->stopped_time;

    if (!proc->term_sent && elapsed >= TERM_TIMEOUT) {
      log_msg("process", "Sending SIGTERM to pid %d", proc->pid);
      uv_kill(proc->pid, SIGTERM);
      proc->term_sent = true;
    } else if (elapsed >= KILL_TIMEOUT) {
      log_msg("process", "Sending SIGKILL to pid %d", proc->pid);
      uv_kill(proc->pid, SIGKILL);
    }
  }
}

static void process_close_event(void **argv)
{
  log_msg("PROCESS", "process_close_event");
  Process *proc = argv[0];
  if (proc->type == kProcessTypePty) {
    free(((PtyProcess *)proc)->term_name);
  }
  if (proc->cb) {
    proc->cb(proc, proc->status, proc->data);
  }
  if (proc->fin_cb) {
    proc->fin_cb(proc, proc->data);
  }
}

static void decref(Process *proc)
{
  log_msg("PROCESS", "decref");
  if (--proc->refcount != 0) {
    return;
  }

  Loop *loop = proc->loop;
  Process *node = NULL;
  Process *it;
  SLIST_FOREACH(it, &loop->children, ent) {
    if (it == proc) {
      node = it;
      break;
    }
  }
  SLIST_REMOVE(&loop->children, node, process, ent);
  CREATE_EVENT(proc->events, process_close_event, 1, proc);
}

static void process_close(Process *proc)
{
  log_msg("PROCESS", "process_close");
  proc->closed = true;
  switch (proc->type) {
    case kProcessTypeUv:
      uv_process_close((UvProcess *)proc);
      break;
    case kProcessTypePty:
      pty_process_close((PtyProcess *)proc);
      break;
    default:
      abort();
  }
}

static void process_close_handles(void **argv)
{
  log_msg("PROCESS", "process_close_handles");
  Process *proc = argv[0];
  process_close_streams(proc);
  process_close(proc);
}

static void on_process_exit(Process *proc)
{
  log_msg("PROCESS", "on_process_exit");
  Loop *loop = proc->loop;
  if (proc->stopped_time && loop->children_stop_requests
      && !--loop->children_stop_requests) {
    // Stop the timer if no more stop requests are pending
    log_msg("process", "Stopping process kill timer");
    uv_timer_stop(&loop->children_kill_timer);
  }
  // Process handles are closed in the next event loop tick. This is done to
  // give libuv more time to read data from the OS after the process exits(If
  // process_close_streams is called with data still in the OS buffer, we lose
  // it)
  CREATE_EVENT(proc->events, process_close_handles, 1, proc);
}

static void on_process_stream_close(Stream *stream, void *data)
{
  log_msg("PROCESS", "on_process_stream_close");
  Process *proc = data;
  decref(proc);
}
