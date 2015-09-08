#include <malloc.h>

#include "fnav/loop.h"
#include "fnav/event.h"
#include "fnav/log.h"

QUEUE headtail;
Loop *spin_loop;
int item_count;
uv_timer_t spin_timer;
static void process_loop();
static void loop_timeout();

void queue_init()
{
  item_count = 0;
  spin_loop = uv_default_loop();
  uv_timer_init(spin_loop, &spin_timer);
  QUEUE_INIT(&headtail);
}

void queue_push(Job *job, void **args)
{
  log_msg("LOOP", "{{push}}");
  JobItem *jobitem = malloc(sizeof(JobItem));
  QueueItem *i = malloc(sizeof(QueueItem));
  jobitem->job = job;
  jobitem->args = args;
  i->item = jobitem;
  QUEUE_INIT(&i->node);
  QUEUE_INSERT_TAIL(&headtail, &i->node);
  item_count++;
  item_count > 0 ? process_loop() : NULL;
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

void loop_timeout(uv_timer_t *req)
{
  log_msg("LOOP", "++spin timeout++");
  uv_stop(spin_loop);
  uv_timer_stop(&spin_timer);
  cycle_events();
}

static void process_loop()
{
  log_msg("LOOP", "|>START->");
  uv_timer_start(&spin_timer, loop_timeout, 10, 0);
  while(item_count > 0)
  {
    JobItem *j = queue_pop();
    j->job->fn(j->job, j->args);
    uv_run(spin_loop, UV_RUN_ONCE);
  }
  log_msg("LOOP", "->STOP>|");
}
