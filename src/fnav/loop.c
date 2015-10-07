#include <malloc.h>
#include <ncurses.h>

#include "fnav/loop.h"
#include "fnav/event.h"
#include "fnav/log.h"
#include "fnav/buffer.h"

typedef struct queue_item QueueItem;
typedef struct job_item JobItem;
struct queue_item {
  Event item;
  QUEUE node;
};

typedef struct {
  Loop *loop;
  loop_cb cb;
} loopbind;

// TODO: fix from testing size; dynamic array.
loopbind loop_pool[5];
int loop_count;

static Queue *queue_new()
{
  Queue *rv = malloc(sizeof(Queue));
  QUEUE_INIT(&rv->headtail);
  return rv;
}

void loop_init(Loop *loop, loop_cb cb)
{
  log_msg("INIT", "new loop");
  uv_loop_init(&loop->uv);
  uv_timer_init(&loop->uv, &loop->delay);
  loop_pool[loop_count].loop = loop;
  loop_pool[loop_count].cb = cb;
  loop_count++;
  loop->events = queue_new();
}

void doloops(int ms)
{
  int remaining = ms;
  uint64_t before = (remaining > 0) ? os_hrtime() : 0;
  for (int i = 0; i < loop_count; i++) {
    loop_pool[i].cb(loop_pool[i].loop, ms);
    if (remaining == 0) {
      break;
    }
    else if (remaining > 0) {
      uint64_t now = os_hrtime();
      remaining -= (int) ((now - before) / 1000000);
      before = now;
      if (remaining <= 0) {
        break;
      }
    }
  }
}

void queue_push(Queue *queue, Event event)
{
  QueueItem *item = malloc(sizeof(QueueItem));
  item->item = event;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_TAIL(&queue->headtail, &item->node);
}

void queue_put_event(Queue *queue, Event event)
{
  queue_push(queue, event);
}

bool queue_empty(Queue *queue)
{
  return QUEUE_EMPTY(&queue->headtail);
}

static QueueItem *queue_node_data(QUEUE *queue)
{
  return QUEUE_DATA(queue, QueueItem, node);
}

static Event queue_pop(Queue *queue)
{
  QUEUE *h = QUEUE_HEAD(&queue->headtail);
  QueueItem *item = queue_node_data(h);
  QUEUE_REMOVE(&item->node);
  Event e;

  e = item->item;
  free(item);
  return e;
}

static void timeout_cb(uv_timer_t *handle)
{
}

void queue_process_events(Queue *queue, int ms)
{
  int remaining = ms;
  while (!queue_empty(queue)) {
    uint64_t before = (remaining > 0) ? os_hrtime() : 0;
    Event e = queue_pop(queue);
    if (e.handler) {
      e.handler(e.argv);
    }
    if (remaining == 0) {
      break;
    }
    else if (remaining > 0) {
      uint64_t now = os_hrtime();
      remaining -= (int) ((now - before) / 1000000);
      before = now;
      if (remaining <= 0) {
        break;
      }
    }
  }
}

void loop_process_events(Loop *loop, int ms)
{
  uv_run_mode mode = UV_RUN_ONCE;

  if (ms > 0) {
    uv_timer_start(&loop->delay, timeout_cb, (uint64_t)ms, (uint64_t)ms);
    uv_unref((uv_handle_t*)&loop->delay);
  }
  else if (ms == 0)
    mode = UV_RUN_NOWAIT;

  uv_run(&loop->uv, mode);

  if (ms > 0) {
    uv_timer_stop(&loop->delay);
  }
  queue_process_events(loop->events, ms);
}

void process_loop(Loop *loop, int ms)
{
  loop_process_events(loop, ms);
}
