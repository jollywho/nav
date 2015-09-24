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
uv_timer_t rotate_timer;
static void process_mainloop();
static void process_loop();
static void loop_timeout();
bool running;

void queue_init()
{
  log_msg("INIT", "queue");
  spin_loop = uv_default_loop();
  uv_timer_init(spin_loop, &spin_timer);
  uv_timer_init(spin_loop, &rotate_timer);
  QUEUE_INIT(&qhead_main.headtail);
  QUEUE_INIT(&qhead_work.headtail);
  QUEUE_INIT(&qhead_draw.headtail);
  running = false;
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
  QueueItem *im = malloc(sizeof(QueueItem));
  im->data.queue = queue;
  QUEUE_INIT(&i->node);
  QUEUE_INIT(&im->node);
  QUEUE_INSERT_TAIL(&queue->headtail, &i->node);
  QUEUE_INSERT_TAIL(&qhead_main.headtail, &im->node);
  if (!running) {
    process_mainloop(&qhead_main);
    uv_timer_stop(&rotate_timer);
    log_msg("LOOP", "<OFF>");
  }
}

static QueueItem *queue_node_data(QUEUE *q)
{
  return QUEUE_DATA(q, QueueItem, node);
}

static QueueItem* queue_pop(Queue *queue)
{
  QUEUE *h = QUEUE_HEAD(&queue->headtail);
  QueueItem *i = queue_node_data(h);
  QUEUE_REMOVE(&i->node);
  return i;
}

void queue_timeout(uv_timer_t *req)
{
  log_msg("LOOP", "+(queue rotate)+");
  process_mainloop(&qhead_main);
}

void loop_timeout(uv_timer_t *req)
{
  log_msg("LOOP", "++spin timeout++");
  uv_timer_stop(&spin_timer);
  cycle_events();
}

static void process_mainloop(Queue *queue)
{
  log_msg("LOOP", "<MAIN>");
  uv_timer_start(&spin_timer, loop_timeout, 10, 0);
  while (!QUEUE_EMPTY(&queue->headtail)) {
    QueueItem *i = queue_pop(queue);
    Queue *q = i->data.queue;
    free(i);
    process_loop(q);
  }
}

static void process_loop(Queue *queue)
{
  log_msg("LOOP", "<subloop>");
  uv_timer_start(&rotate_timer, queue_timeout, 20, 0);
  while (!QUEUE_EMPTY(&queue->headtail)) {
    QueueItem *i = queue_pop(queue);
    JobItem *j = i->data.item.jitem;
    free(i);
    j->arg->fn(j->job, j->arg);
    free(j);
    uv_run(spin_loop, UV_RUN_NOWAIT);
  }
  log_msg("LOOP", "<subend>");
}
