#include <malloc.h>
#include "loop.h"

QUEUE headtail;

void queue_init()
{
  QUEUE_INIT(&headtail);
}

void queue_push(Job job)
{
  QueueItem *item = malloc(sizeof(QueueItem));
  item->data = &job;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_TAIL(&headtail, &item->node);
}

static QueueItem *queue_node_data(QUEUE *q)
{
  return QUEUE_DATA(q, QueueItem, node);
}

static Job *queue_pop()
{
  QUEUE *h = QUEUE_HEAD(&headtail);
  QueueItem *i = queue_node_data(h);
  Job *job = i->data;
  QUEUE_REMOVE(&i->node);
  free(i);
  return job;
}
