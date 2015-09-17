#include <malloc.h>

#include "fnav/loop.h"
#include "fnav/event.h"
#include "fnav/log.h"
#include "fnav/buffer.h"

typedef struct queue_item QueueItem;
typedef struct job_item JobItem;
struct queue_item {
  union {
    Queue *queue;
    struct {
      JobItem *jitem;
      QueueItem *parent;
    } item;
  } data;
  QUEUE node;
};

struct job_item {
  Job *job;
  JobArg *arg;
};

Loop *spin_loop;
uv_timer_t spin_timer;
static void process_loop();
static void loop_timeout();

void queue_init()
{
  spin_loop = uv_default_loop();
  uv_timer_init(spin_loop, &spin_timer);
  QUEUE_INIT(&qhead_work.headtail);
  QUEUE_INIT(&qhead_draw.headtail);
}

void queue_push(Queue *queue, Job *job, JobArg *arg)
{
  log_msg("LOOP", "{{push}}");
  QueueItem *i = malloc(sizeof(QueueItem));
  JobItem *ji = malloc(sizeof(JobItem));
  ji->job = job;
  ji->arg = arg;
  i->data.queue = queue;
  i->data.item.jitem = ji;
  QUEUE_INIT(&i->node);
  QUEUE_INSERT_TAIL(&queue->headtail, &i->node);
  QUEUE_EMPTY(&queue->headtail) ? NULL : process_loop(queue);
}

static QueueItem *queue_node_data(QUEUE *q)
{
  return QUEUE_DATA(q, QueueItem, node);
}

static JobItem* queue_pop(Queue *queue)
{
  QUEUE *h = QUEUE_HEAD(&queue->headtail);
  QueueItem *i = queue_node_data(h);
  JobItem *ji = i->data.item.jitem;
  QUEUE_REMOVE(&i->node);
  free(i);
  return ji;
}

void loop_timeout(uv_timer_t *req)
{
  log_msg("LOOP", "++spin timeout++");
  uv_timer_stop(&spin_timer);
  cycle_events();
}

static void process_loop(Queue *queue)
{
  log_msg("LOOP", "|>START->");
  uv_timer_start(&spin_timer, loop_timeout, 10, 0);
  while (!QUEUE_EMPTY(&queue->headtail)) {
    JobItem *j = queue_pop(queue);
    j->arg->fn(j->job, j->arg);
    uv_run(spin_loop, UV_RUN_NOWAIT); // TODO: is this needed?
  }
  log_msg("LOOP", "->STOP>|");
}
