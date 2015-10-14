#ifndef FN_CORE_LOOP_H
#define FN_CORE_LOOP_H

#include <stdarg.h>
#include <sys/queue.h>
#include <uv.h>

#include "fnav/lib/queue.h"
#include "fnav/tui/cntlr.h"

typedef struct queue Queue;
struct queue {
  QUEUE headtail;
};

typedef struct loop Loop;
typedef void(*loop_cb)(Loop *loop, int ms);
struct loop {
  uv_loop_t uv;
  Queue events;
  uv_signal_t children_watcher;
  uv_timer_t delay;
  uv_timer_t children_kill_timer;
  size_t children_stop_requests;
  bool running;
  loop_cb cb;
  SLIST_ENTRY(loop) ent;
};

#define EVENT_HANDLER_MAX_ARGC 4
typedef struct message {
  argv_callback handler;
  void *argv[EVENT_HANDLER_MAX_ARGC];
} Event;

#define queue_put(q, h, ...) \
  queue_put_event(q, event_create(h, __VA_ARGS__));

#define CREATE_EVENT(queue, handler, argc, ...)                  \
  queue_put((queue), (handler), argc, __VA_ARGS__);          \

#define VA_EVENT_INIT(event, h, a)                   \
  do {                                                  \
    (event)->handler = h;                               \
    if (a) {                                            \
      va_list args;                                     \
      va_start(args, a);                                \
      for (int i = 0; i < a; i++) {                     \
        (event)->argv[i] = va_arg(args, void *);        \
      }                                                 \
      va_end(args);                                     \
    }                                                   \
  } while (0)

static inline Event event_create(argv_callback cb, int argc, ...)
{
  Event event;
  VA_EVENT_INIT(&event, cb, argc);
  return event;
}

void loop_init();
void loop_destoy();
void loop_add(Loop *loop, loop_cb cb);
void loop_remove(Loop *loop);

void queue_push(Queue *queue, Event event);
void queue_put_event(Queue *queue, Event event);
bool queue_empty(Queue *queue);
void process_loop(Loop *loop, int ms);
void doloops(int ms);

#endif
