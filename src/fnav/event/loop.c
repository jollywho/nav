#include <malloc.h>
#include <ncurses.h>

#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/log.h"
#include "fnav/tui/buffer.h"

typedef struct queue_item queue_item;
struct queue_item {
  Event item;
  QUEUE node;
};

#define TIMEOUT 10
static void prepare_events(uv_prepare_t *handle);

static void queue_new(Queue *queue)
{
  QUEUE_INIT(&queue->headtail);
}

void loop_add(Loop *loop)
{
  log_msg("INIT", "new loop");
  uv_loop_init(&loop->uv);
  uv_prepare_init(&loop->uv, &loop->event_prepare);
  uv_timer_init(&loop->uv, &loop->children_kill_timer);
  uv_signal_init(&loop->uv, &loop->children_watcher);
  SLIST_INIT(&loop->children);
  loop->uv.data = &loop;
  queue_new(&loop->eventq);
  queue_new(&loop->drawq);
  loop->running = false;
}

void queue_push(Queue *queue, Event event)
{
  queue_item *item = malloc(sizeof(queue_item));
  item->item = event;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_TAIL(&queue->headtail, &item->node);
}

void queue_put_event(Queue *queue, Event event)
{
  queue_push(queue, event);
  if (!mainloop()->running) {
    uv_prepare_start(&mainloop()->event_prepare, prepare_events);
  }
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

static void prepare_events(uv_prepare_t *handle)
{
  if (mainloop()->running) return;
  mainloop()->running = true;
  while (1) {
    queue_process_events(eventq(), TIMEOUT);
    queue_process_events(drawq(),  TIMEOUT);
    if (queue_empty(eventq()) && queue_empty(drawq())) {
      uv_prepare_stop(handle);
      break;
    }
    else {
      uv_run(eventloop(), UV_RUN_NOWAIT);
    }
  }
  mainloop()->running = false;
}
