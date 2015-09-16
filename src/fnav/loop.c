#include <malloc.h>

#include "fnav/loop.h"
#include "fnav/event.h"
#include "fnav/log.h"
#include "fnav/buffer.h"

QUEUE headtail;
QUEUE draw_headtail;
Loop *spin_loop;
Loop *draw_loop;
int item_count;
int draw_item_count;
uv_timer_t spin_timer;
uv_timer_t draw_timer;
static void process_loop();
static void loop_timeout();
void process_loop_draw();

void queue_init()
{
  item_count = 0;
  draw_item_count = 0;
  spin_loop = uv_default_loop();
  draw_loop = uv_default_loop();
  uv_timer_init(spin_loop, &spin_timer);
  uv_timer_init(draw_loop, &draw_timer);
  QUEUE_INIT(&headtail);
  QUEUE_INIT(&draw_headtail);
}

void queue_push_buf(fn_buf *buf)
{
  log_msg("LOOP", "{{push}}");
  QueueItem *i = malloc(sizeof(QueueItem));
  i->buf = buf;
  QUEUE_INIT(&i->node);
  QUEUE_INSERT_TAIL(&draw_headtail, &i->node);
  draw_item_count++;
  draw_item_count + item_count > 1 ? NULL : process_loop_draw();
}

void queue_push(Job *job, JobArg *arg)
{
  log_msg("LOOP", "{{push}}");
  JobItem *jobitem = malloc(sizeof(JobItem));
  QueueItem *i = malloc(sizeof(QueueItem));
  jobitem->job = job;
  jobitem->arg = arg;
  i->item = jobitem;
  QUEUE_INIT(&i->node);
  QUEUE_INSERT_TAIL(&headtail, &i->node);
  item_count++;
  draw_item_count + item_count > 1 ? NULL : process_loop();
}

static QueueItem *queue_node_data(QUEUE *q)
{
  return QUEUE_DATA(q, QueueItem, node);
}

static JobItem* queue_pop()
{
  QUEUE *h = QUEUE_HEAD(&headtail);
  QueueItem *i = queue_node_data(h);
  JobItem *j = i->item;
  QUEUE_REMOVE(&i->node);
  item_count--;
  free(i);
  return j;
}

static fn_buf* queue_pop_buf()
{
  QUEUE *h = QUEUE_HEAD(&draw_headtail);
  QueueItem *i = queue_node_data(h);
  fn_buf *buf = i->buf;
  QUEUE_REMOVE(&i->node);
  draw_item_count--;
  free(i);
  return buf;
}

void loop_timeout(uv_timer_t *req)
{
  log_msg("LOOP", "++spin timeout++");
  uv_timer_stop(&spin_timer);
  cycle_events();
}

void draw_timeout(uv_timer_t *req)
{
  uv_timer_stop(&spin_timer);
  item_count > 1 ? process_loop() : NULL;
}

void process_loop_draw()
{
  uv_timer_start(&spin_timer, draw_timeout, 5, 0);
  while(draw_item_count > 0)
  {
    fn_buf *buf = queue_pop_buf();
    buf_draw(buf);
    uv_run(draw_loop, UV_RUN_NOWAIT); // TODO: is this needed?
  }
}

void draw_cycle(uv_timer_t *req)
{
  draw_item_count > 1 ? process_loop_draw() : NULL;
}

static void process_loop()
{
  log_msg("LOOP", "|>START->");
  uv_timer_start(&spin_timer, loop_timeout, 10, 0);
  uv_timer_start(&draw_timer, process_loop_draw, 30, 0);
  while(item_count > 0)
  {
    JobItem *j = queue_pop();
    j->job->fn(j->job, j->arg);
    uv_run(spin_loop, UV_RUN_NOWAIT); // TODO: is this needed?
  }
  log_msg("LOOP", "->STOP>|");
}
