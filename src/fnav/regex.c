#include <pcre.h>

#include "fnav/regex.h"
#include "fnav/log.h"
#include "fnav/model.h"
#include "fnav/tui/window.h"

#define NSUBEXP  10

struct LineMatch {
  UT_array *linenum;
};

int pivot_top = 0;
int pivot_lnum = 0;

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
        log_msg("REGEX", "PCRE SUBSTR: %s", &match[j]);
        if (strlen(&match[j]) > 0)
          utarray_push_back(matches->linenum, &i);
      }
      pcre_free_substring(match);
    }
  }
  buf_set_linematch(buf, matches);
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
  Buffer *buf = window_get_focus();
  return buf_line(buf) + buf_top(buf);
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

void regex_start_pivot()
{
  Buffer *buf = window_get_focus();
  pivot_top = buf_top(buf);
  pivot_lnum = buf_line(buf);
}

void regex_stop_pivot()
{
  log_msg("REGEX", "regex_req_enter");
  //pivot_line = focus_cur_line();
}

// pivot buffernode focus to closest match.
// varies by direction: '/', '?'
void regex_next()
{
  log_msg("REGEX", "regex_next");
  Buffer *buf = window_get_focus();
  int line = focus_cur_line();
  LineMatch *matches = buf->matches;

  if (!matches) return;
  if (utarray_len(matches->linenum) < 1) return;
  int *next = (int*)utarray_front(matches->linenum);
  if (!next) return;
  regex_focus(*next, line);
}

int intsort(const void *a, const void*b) {
  int _a = *(int*)a;
  int _b = *(int*)b;
  return _a - _b;
}

void regex_prev()
{
}

static void find_next(UT_array *linenum)
{
  log_msg("REGEX", "regex_focus");
  // get end index of matches
  // if line > end, divide indices in half as new line
  // if line < end, that is the result
  //
}
