#ifndef FN_EVENT_EVENT_H
#define FN_EVENT_EVENT_H

#include <stdarg.h>
#include <stdbool.h>
#include <uv.h>
#include <sys/queue.h>
#include "nav/lib/queue.h"

typedef void (*argv_callback)(void **argv);
typedef struct queue Queue;
struct queue {
  QUEUE headtail;
};

typedef struct loop Loop;
struct loop {
  uv_loop_t uv;
  Queue eventq;
  Queue drawq;
  SLIST_HEAD(WatcherPtr, process) children;
  SLIST_HEAD(WatcherTerm, term) subterms;
  uv_signal_t children_watcher;
  uv_prepare_t event_prepare;
  uv_timer_t children_kill_timer;
  size_t children_stop_requests;
  bool running;
};

#define EVENT_HANDLER_MAX_ARGC 4
typedef struct message {
  argv_callback handler;
  void *argv[EVENT_HANDLER_MAX_ARGC];
} Event;

#define queue_put(q, h, ...) \
  queue_put_event(q, event_create(h, __VA_ARGS__));

#define CREATE_EVENT(queue, handler, argc, ...)         \
  queue_put((queue), (handler), argc, __VA_ARGS__);     \

#define VA_EVENT_INIT(event, h, a)                      \
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

#define TIMEOUT 10

#define DO_EVENTS_UNTIL(cond)                 \
  while (!(cond)) {                           \
    queue_process_events(eventq(), TIMEOUT);  \
    queue_process_events(drawq(),  TIMEOUT);  \
    uv_run(eventloop(), UV_RUN_NOWAIT);       \
  }                                           \

bool mainloop_busy();
void event_cycle_once();

void queue_push(Queue *queue, Event event);
void queue_put_event(Queue *queue, Event event);
bool queue_empty(Queue *queue);
void queue_process_events(Queue *queue, int ms);

void event_init(void);
void event_cleanup(void);
void stop_event_loop(void);
void start_event_loop(void);
uv_loop_t *eventloop(void);
Loop* mainloop(void);
uint64_t os_hrtime(void);
void event_input(void);
Queue* drawq(void);
Queue* eventq(void);

#endif
