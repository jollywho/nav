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

struct loop_list {
  SLIST_HEAD(looplist, loop) p;
};

struct loop_list loop_pool;

void loop_init()
{
  log_msg("INIT", "loop pool");
  SLIST_INIT(&loop_pool.p);
}

void loop_destoy()
{
  SLIST_REMOVE_HEAD(&loop_pool.p, ent);
}

static void queue_new(Queue *queue)
{
  QUEUE_INIT(&queue->headtail);
}

void loop_add(Loop *loop, loop_cb cb)
{
  log_msg("INIT", "new loop");
  uv_loop_init(&loop->uv);
  uv_timer_init(&loop->uv, &loop->delay);
  SLIST_INIT(&loop->children);
  SLIST_INSERT_HEAD(&loop_pool.p, loop, ent);
  loop->cb = cb;
  queue_new(&loop->events);
}

void loop_remove(Loop *lp)
{
  SLIST_REMOVE(&loop_pool.p, lp, loop, ent);
  uv_timer_stop(&lp->delay);
  uv_loop_close(&lp->uv);
}

void doloops(int ms)
{
  int remaining = ms;
  uint64_t before = (remaining > 0) ? os_hrtime() : 0;
  Loop *it;
  SLIST_FOREACH(it, &loop_pool.p, ent) {
    it->cb(it, ms);
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
  queue_item *item = malloc(sizeof(queue_item));
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
  }
  else if (ms == 0)
    mode = UV_RUN_NOWAIT;

  mode = UV_RUN_NOWAIT; // FIXME: UV_RUN_ONCE is blocking
  uv_run(&loop->uv, mode);

  if (ms > 0) {
  }
    uv_timer_stop(&loop->delay);
  queue_process_events(&loop->events, ms);
}

void process_loop(Loop *loop, int ms)
{
  loop_process_events(loop, ms);
}
