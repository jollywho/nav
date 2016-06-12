#ifndef NV_FILTER_H
#define NV_FILTER_H

#include "nav/plugins/plugin.h"

typedef struct Filter Filter;

Filter* filter_new(Handle *);
void filter_destroy(Handle *);
void filter_build(Filter *, const char *);
void filter_update(Filter *);
void filter_apply(Handle *);

#endif
