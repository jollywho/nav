#ifndef FN_FILTER_H
#define FN_FILTER_H

#include "nav/plugins/plugin.h"

typedef struct Filter Filter;

Filter* filter_new(fn_handle *);
void filter_destroy(fn_handle *);
void filter_build(Filter *, const char *);
void filter_update(Filter *);
void filter_apply(fn_handle *);

#endif
