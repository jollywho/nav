#include <pcre.h>

#include "fnav/regex.h"
#include "fnav/log.h"
#include "fnav/model.h"
#include "fnav/tui/window.h"

#define NSUBEXP  10

struct LineMatch {
  UT_array *linenum;
};

LineMatch *pivot_matches;
int pivot_top = 0;
int pivot_lnum = 0;
static int* nearest_match(UT_array *matches, int line);

// compile pcre for each line in all window buffers
// add built list to each buffernode
void regex_build(String line)
{
  log_msg("REGEX", "build");
  log_msg("REGEX", ":%s:", line);
  const char *pcreErrorStr;
  int pcreErrorOffset;

  pcre *comp;
  pcre_extra *extra;
  String regex = strdup(line);

  comp = pcre_compile(regex,
      PCRE_CASELESS,
      &pcreErrorStr,
      &pcreErrorOffset,
      NULL);

  if (comp == NULL) {
    log_msg("REGEX", "COMPILE ERROR: %s, %s", regex, pcreErrorStr);
    return;
  }

  extra = pcre_study(comp, 0, &pcreErrorStr);

  if(pcreErrorStr != NULL) {
    log_msg("REGEX", "COULD NOT STUDY: %s, %s", regex, pcreErrorStr);
    return;
  }

  Buffer *buf = window_get_focus();
  Model *m = buf->hndl->model;
  LineMatch *matches = malloc(sizeof(LineMatch));
  utarray_new(matches->linenum, &ut_int_icd);

  int max = model_count(m);
  for (int i = 0; i < max; i++) {
    int substr[NSUBEXP];
    const char *match;

    String subject = model_str_line(m, i);

    int ret = pcre_exec(comp,
        extra,
        subject,
        strlen(subject),  // length of string
        0,                // Start looking at this point
        0,                // OPTIONS
        substr,
        NSUBEXP);         // Length of subStrVec

    if (ret == 0)
      ret = NSUBEXP / 3;
    if (ret > -1) {
      pcre_get_substring(subject, substr, ret, 0, &(match));

      for(int j = 0; j < ret; j++) {
        if (substr[j*2+1] > 0) {
          utarray_push_back(matches->linenum, &i);
          break;
        }
      }
      pcre_free_substring(match);
    }
  }
  pivot_matches = matches;
  pcre_free(regex);
  if(extra != NULL)
    pcre_free(extra);
}

void regex_destroy(Buffer *buf)
{
  if (buf->matches) {
    utarray_free(buf->matches->linenum);
  }
  free(buf->matches);
}

static int focus_cur_line()
{
  return pivot_top + pivot_lnum;
}

static int line_diff(int to, int from)
{ return to - from; }

static void regex_focus(int to, int from)
{
  log_msg("REGEX", "regex_focus");
  Buffer *buf = window_get_focus();
  buf_move(buf, line_diff(to, from), 0);
}

void regex_pivot()
{
  Buffer *buf = window_get_focus();
  buf_full_invalidate(buf, pivot_top, pivot_lnum);
}

void regex_mk_pivot()
{
  Buffer *buf = window_get_focus();
  pivot_top = buf_top(buf);
  pivot_lnum = buf_line(buf);
}

void regex_keep_pivot()
{
  log_msg("REGEX", "regex_req_enter");
  Buffer *buf = window_get_focus();
  buf_set_linematch(buf, pivot_matches);
}

void regex_hover()
{
  int line = focus_cur_line();

  if (!pivot_matches) return;
  if (utarray_len(pivot_matches->linenum) < 1) return;

  int *ret = nearest_match(pivot_matches->linenum, line);
  if (ret) {
    regex_focus(*ret, line);
  }
  else
    regex_pivot();
}

#define NEXT_OR_WRAP(match,l) \
  l = (int*)utarray_next(match->linenum, l); \
  l = l ? l : (int*)utarray_next(match->linenum, l);
#define PREV_OR_WRAP(match,l) \
  l = (int*)utarray_prev(match->linenum, l); \
  l = l ? l : (int*)utarray_prev(match->linenum, l);

// pivot buffernode focus to closest match.
// varies by direction: '/', '?'
void regex_next(int line)
{
  log_msg("REGEX", "regex_next");
  Buffer *buf = window_get_focus();
  LineMatch *matches = buf->matches;

  if (!matches) return;
  if (utarray_len(matches->linenum) < 1) return;

  int *ret = nearest_match(matches->linenum, line);
  NEXT_OR_WRAP(matches, ret);
  regex_focus(*ret, line);
}

void regex_prev(int line)
{
  log_msg("REGEX", "regex_prev");
  Buffer *buf = window_get_focus();
  LineMatch *matches = buf->matches;

  if (!matches) return;
  if (utarray_len(matches->linenum) < 1) return;

  int *ret = nearest_match(matches->linenum, line);
  PREV_OR_WRAP(matches, ret);
  regex_focus(*ret, line);
}

static int* nearest_match(UT_array *matches, int line)
{
  log_msg("REGEX", "find_next");
  int *it = NULL;
  while ((it = (int*)utarray_next(matches, it))) {
    if (*it == line) {
      break;
    }
    if (*it > line) {
      break;
    }
  }
  return it;
}
