#include <malloc.h>

#include "nav/event/event.h"
#include "nav/event/input.h"
#include "nav/log.h"

typedef struct queue_item queue_item;
struct queue_item {
  Event item;
  QUEUE node;
};

static Loop main_event_loop;

void event_init(void)
{
  log_msg("INIT", "event");
  uv_loop_init(eventloop());
  uv_prepare_init(eventloop(), &mainloop()->event_prepare);
  uv_timer_init(eventloop(), &mainloop()->children_kill_timer);
  uv_signal_init(eventloop(), &mainloop()->children_watcher);
  SLIST_INIT(&mainloop()->children);
  eventloop()->data = eventloop();
  QUEUE_INIT(&eventq()->headtail);
  QUEUE_INIT(&drawq()->headtail);
  mainloop()->running = false;
}

void event_cleanup(void)
{
  uv_loop_close(eventloop());
}

uint64_t os_hrtime(void)
{
  return uv_hrtime();
}

Loop* mainloop(void)
{
  return &main_event_loop;
}

uv_loop_t *eventloop(void)
{
  return &mainloop()->uv;
}

Queue* drawq(void)
{
  return &mainloop()->drawq;
}

Queue* eventq(void)
{
  return &mainloop()->eventq;
}

void event_input(void)
{
  input_check();
}

void start_event_loop(void)
{
  log_msg("EVENT", "::::started::::");
  uv_run(eventloop(), UV_RUN_DEFAULT);
  log_msg("EVENT", "::::exited:::::");
}

void stop_event_loop(void)
{
  uv_stop(&main_event_loop.uv);
}

void queue_push(Queue *queue, Event event)
{
  queue_item *item = malloc(sizeof(queue_item));
  item->item = event;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_TAIL(&queue->headtail, &item->node);
}

bool queue_empty(Queue *queue)
{
  return QUEUE_EMPTY(&queue->headtail);
}

static queue_item *queue_node_data(QUEUE *queue)
{
  return QUEUE_DATA(queue, queue_item, node);
}

static Event queue_pop(Queue *queue)
{
  QUEUE *h = QUEUE_HEAD(&queue->headtail);
  queue_item *item = queue_node_data(h);
  QUEUE_REMOVE(&item->node);
  Event e;

  e = item->item;
  free(item);
  return e;
}

void queue_process_events(Queue *queue, int ms)
{
  int remaining = ms;
  uint64_t before = (remaining > 0) ? os_hrtime() : 0;
  while (!queue_empty(queue)) {
    if (remaining > 0) {
      Event e = queue_pop(queue);
      if (e.handler) {
        e.handler(e.argv);
      }
      uint64_t now = os_hrtime();
      remaining -= (int) ((now - before) / 1000000);
      if (remaining <= 0) {
        break;
      }
    }
  }
}

bool mainloop_busy()
{
  return !(queue_empty(eventq()) && queue_empty(drawq()));
}

static void prepare_events(uv_prepare_t *handle)
{
  log_msg("", "--event--");
  if (mainloop()->running)
    return;

  mainloop()->running = true;
  while (1) {
    queue_process_events(eventq(), TIMEOUT);
    queue_process_events(drawq(),  TIMEOUT);

    if (!mainloop_busy()) {
      uv_prepare_stop(handle);
      break;
    }
    else {
      uv_run(eventloop(), UV_RUN_NOWAIT);
    }
  }
  mainloop()->running = false;
}

void event_wakeup(void)
{
  prepare_events(&mainloop()->event_prepare);
}

void queue_put_event(Queue *queue, Event event)
{
  queue_push(queue, event);
  if (!mainloop()->running) {
    uv_prepare_start(&mainloop()->event_prepare, prepare_events);
  }
}
