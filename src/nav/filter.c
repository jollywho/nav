#include "nav/filter.h"
#include "nav/table.h"
#include "nav/model.h"
#include "nav/tui/buffer.h"
#include "nav/log.h"

struct Filter {
  fn_handle *hndl;
  Pattern *pat;
  char *line;
};

Filter* filter_new(fn_handle *hndl)
{
  Filter *fil = malloc(sizeof(Filter));
  fil->hndl = hndl;
  fil->pat = NULL;
  fil->line = strdup("");
  return fil;
}

void filter_destroy(fn_handle *hndl)
{
  Filter *fil = hndl->buf->filter;
  if (fil->pat)
    regex_pat_delete(fil->pat);
  free(fil->line);
  free(fil);
}

void filter_build(Filter *fil, const char *line)
{
  log_msg("FILTER", "build");
  if (fil->pat)
    regex_pat_delete(fil->pat);

  fil->pat = regex_pat_new(line);
  SWAP_ALLOC_PTR(fil->line, strdup(line));

  Model *m = fil->hndl->model;

  model_clear_filter(m);

  int max = model_count(m);
  int count = 0;
  for (int i = max; i > 0; i--) {
    char *str = model_str_line(m, i-1);
    if (!regex_match(fil->pat, str)) {
      model_filter_line(m, i-1);
      count++;
    }
  }
  buf_signal_filter(fil->hndl->buf, count);
}

void filter_update(Filter *fil)
{
  Model *m = fil->hndl->model;
  model_sort(m, (sort_t){-1,0});
}

void filter_apply(fn_handle *hndl)
{
  Filter *fil = hndl->buf->filter;
  if (strlen(fil->line) < 1)
    return;
  filter_build(fil, fil->line);
}
