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

static void process_mainloop();
static void process_loop();

void loop_init(Loop *loop)
{
  log_msg("INIT", "queue");
  uv_loop_init(&loop->uv);
  QUEUE_INIT(&loop->events->headtail);
}

void queue_push(Queue *queue, Event event)
{
  log_msg("LOOP", "{{push}}");
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

void queue_process_events(Queue *queue)
{
  while (!QUEUE_EMPTY(&queue->headtail)) {
    Event e = queue_pop(queue);
    if (e.handler) {
      e.handler(e.argv);
    }
  }
}

void loop_process_events(Loop *loop, int ms)
{
  log_msg("EVENT", "<<enable>>");
  uv_run_mode mode = UV_RUN_ONCE;
  if (ms > 0) {
    uv_timer_start(&loop->delay, timeout_cb, (uint64_t)ms, (uint64_t)ms);
  }
  else if(ms == 0)
    mode = UV_RUN_NOWAIT;

  uv_run(&loop->uv, mode);
  queue_process_events(loop->events);
}

// TODO: 30ms timeslice total
// MAIN LOOP:
// polling input
//  |on input
//    set initial time
// draw timer:
//  |on queue
//    loop
//
// uv_check cb loop:
//  check loop:
//    |calc remaining
//      timer start, set to remaining time
//      loop
//    |calc neg or 0
//      restart main loop
static void process_loop(Loop *loop)
{
  log_msg("LOOP", "<MAIN>");
  int remaining = global_input_time;
  uint64_t before = (remaining > 0) ? os_hrtime() : 0;
  while (!QUEUE_EMPTY(&loop->events->headtail)) {
    loop_process_events(loop, remaining);
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
  log_msg("LOOP", "<subend>");
}
