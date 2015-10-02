#include <malloc.h>
#include <ncurses.h>

#include "fnav/loop.h"
#include "fnav/event.h"
#include "fnav/log.h"
#include "fnav/buffer.h"

typedef struct queue_item QueueItem;
typedef struct job_item JobItem;
struct queue_item {
  JobItem *item;
  QUEUE node;
};

struct job_item {
  Job *job;
  JobArg *arg;
};

Loop *spin_loop;
QUEUE qmain;
static void process_mainloop();
static void process_loop();
static void loop_timeout();
bool running;

void queue_init()
{
  log_msg("INIT", "queue");
  spin_loop = uv_default_loop();
  QUEUE_INIT(&qmain);
  running = false;
}

void queue_push(Loop *loop, Job *job, JobArg *arg)
{
  log_msg("LOOP", "{{push}}");
  QueueItem *item = malloc(sizeof(QueueItem));
  JobItem *i = malloc(sizeof(JobItem));
  i->job = job;
  i->arg = arg;
  item->item = i;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_TAIL(&qmain, &item->node);
  if (!running) {
    process_mainloop();
    log_msg("LOOP", "<OFF>");
  }
}

static QueueItem *queue_node_data(QUEUE *q)
{
  return QUEUE_DATA(q, QueueItem, node);
}

static JobItem* queue_pop()
{
  QUEUE *h = QUEUE_HEAD(&qmain);
  QueueItem *i = queue_node_data(h);
  QUEUE_REMOVE(&i->node);
  JobItem *j = i->item;
  free(i);
  return j;
}

uint64_t os_hrtime(void)
{
  return uv_hrtime();
}

static void process_mainloop()
{
  log_msg("LOOP", "<MAIN>");
  int remaining = 0;
  uint64_t before = (remaining > 0) ? os_hrtime() : 0;
  while (!QUEUE_EMPTY(&qmain)) {
    JobItem *j = queue_pop();
    j->arg->fn(j->job, j->arg);
    free(j);
    cycle_events(remaining);
    doupdate();
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
