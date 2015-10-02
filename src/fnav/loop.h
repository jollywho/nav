#ifndef FN_CORE_LOOP_H
#define FN_CORE_LOOP_H

#include <stdarg.h>
#include "fnav/table.h"

typedef struct queue Queue;
struct queue {
  QUEUE headtail;
  void *data;
};

typedef struct loop {
  uv_loop_t uv;
  Queue *events;
  uv_signal_t children_watcher;
  uv_timer_t delay;
  uv_timer_t children_kill_timer;
  size_t children_stop_requests;
  bool running;
} Loop;

#define EVENT_HANDLER_MAX_ARGC 4
typedef void (*argv_callback)(void **argv);
typedef struct message {
  argv_callback handler;
  void *argv[EVENT_HANDLER_MAX_ARGC];
} Event;

#define queue_put(q, h, ...) \
  queue_put_event(q, event_create(1, h, __VA_ARGS__));

#define CREATE_EVENT(queue, handler, argc, ...)                  \
  do {                                                           \
    if (queue) {                                                 \
      queue_put((queue), (handler), argc, __VA_ARGS__);          \
    }                                                            \
  } while (0)

#define VA_EVENT_INIT(event, p, h, a)                   \
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

static inline Event event_create(int priority, argv_callback cb, int argc, ...)
{
  Event event;
  VA_EVENT_INIT(&event, priority, cb, argc);
  return event;
}

uint64_t global_input_time;
void queue_push(Queue *queue, Event event);
void queue_put_event(Queue *queue, Event event);
bool queue_empty(Queue *queue);
void loop_init(Loop *loop);

#endif
