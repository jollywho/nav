#include <malloc.h>
#include <ncurses.h>

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
static void process_mainloop();
static void process_loop();
static void loop_timeout();
bool running;

void queue_init()
{
  log_msg("INIT", "queue");
  spin_loop = uv_default_loop();
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

uint64_t os_hrtime(void)
{
  return uv_hrtime();
}

static void process_mainloop(Queue *queue)
{
  log_msg("LOOP", "<MAIN>");
  while (!QUEUE_EMPTY(&queue->headtail)) {
    QueueItem *i = queue_pop(queue);
    Queue *q = i->data.queue;
    free(i);
    process_loop(q);
    doupdate();
  }
}

static void process_loop(Queue *queue)
{
  log_msg("LOOP", "<subloop>");
  int remaining = 0;
  uint64_t before = (remaining > 0) ? os_hrtime() : 0;
  while (!QUEUE_EMPTY(&queue->headtail)) {
    QueueItem *i = queue_pop(queue);
    JobItem *j = i->data.item.jitem;
    free(i);
    j->arg->fn(j->job, j->arg);
    free(j);
    cycle_events(remaining);
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
    log_msg("LOOP", "<subend finished>");
  }
  log_msg("LOOP", "<subend>");
}
