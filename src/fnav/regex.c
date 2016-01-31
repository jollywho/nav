#include <pcre.h>

#include "fnav/regex.h"
#include "fnav/log.h"
#include "fnav/model.h"
#include "fnav/tui/window.h"

#define NSUBEXP  10

#define NEXT_OR_WRAP(match,l) \
  l = (int*)utarray_next(match, l); \
  l = l ? l : (int*)utarray_next(match, l);
#define PREV_OR_WRAP(match,l) \
  l = (int*)utarray_prev(match, l); \
  l = l ? l : (int*)utarray_prev(match, l);

struct LineMatch {
  UT_array *lines;
  fn_handle *hndl;
  String regex;
  pcre *comp;
  pcre_extra *extra;
  int pivot_top;
  int pivot_lnum;
};

LineMatch* regex_new(fn_handle *hndl)
{
  LineMatch *lm = malloc(sizeof(LineMatch));
  memset(lm, 0, sizeof(LineMatch));
  lm->hndl = hndl;
  lm->regex = strdup("");
  utarray_new(lm->lines, &ut_int_icd);
  return lm;
}

void regex_destroy(fn_handle *hndl)
{
  LineMatch *lm = hndl->buf->matches;
  free(lm->regex);
  free(lm);
}

static void regex_compile(LineMatch *lm)
{
  const char *pcreErrorStr;
  int pcreErrorOffset;

  lm->comp = pcre_compile(lm->regex,
      PCRE_CASELESS,
      &pcreErrorStr,
      &pcreErrorOffset,
      NULL);

  if (lm->comp == NULL) {
    log_msg("REGEX", "COMPILE ERROR: %s, %s", lm->regex, pcreErrorStr);
    return;
  }

  lm->extra = pcre_study(lm->comp, 0, &pcreErrorStr);

  if(pcreErrorStr != NULL) {
    log_msg("REGEX", "COULD NOT STUDY: %s, %s", lm->regex, pcreErrorStr);
    return;
  }
}

void regex_build(LineMatch *lm, String line)
{
  log_msg("REGEX", "build");
  log_msg("REGEX", ":%s:", line);

  if (line) {
    regex_del_matches(lm);
    pcre_free(lm->regex);
    lm->regex = strdup(line);
  }

  regex_compile(lm);

  utarray_new(lm->lines, &ut_int_icd);

  int max = model_count(lm->hndl->model);
  for (int i = 0; i < max; i++) {
    int substr[NSUBEXP];
    const char *match;

    String subject = model_str_line(lm->hndl->model, i);

    int ret = pcre_exec(lm->comp,
        lm->extra,
        subject,
        strlen(subject),  // length of string
        0,                // Start looking at this point
        0,                // OPTIONS
        substr,
        NSUBEXP);         // Length of substr

    if (ret == 0)
      ret = NSUBEXP / 3;
    if (ret > -1) {
      pcre_get_substring(subject, substr, ret, 0, &(match));

      for(int j = 0; j < ret; j++) {
        if (substr[j*2+1] > 0) {
          utarray_push_back(lm->lines, &i);
          break;
        }
      }
      pcre_free_substring(match);
    }
  }
  pcre_free(lm->extra);
  pcre_free(lm->comp);
}

void regex_del_matches(LineMatch *lm)
{
  log_msg("REGEX", "regex_del_matches");
  if (!lm->lines)
    return;
  utarray_free(lm->lines);
  lm->lines = NULL;
}

static int focus_cur_line(LineMatch *lm)
{
  return lm->pivot_top + lm->pivot_lnum;
}

static int line_diff(int to, int from)
{
  return to - from;
}

static void get_or_make_matches(LineMatch *lm)
{
  if (!lm->lines) {
    regex_mk_pivot(lm);
    regex_build(lm, NULL);
  }
}

static int* nearest_next_match(UT_array *matches, int line)
{
  log_msg("REGEX", "nearest_next_match");
  int *it = NULL;
  while ((it = (int*)utarray_next(matches, it))) {
    if (*it == line || *it > line)
      break;
  }
  return it;
}

static void regex_focus(LineMatch *lm, int to, int from)
{
  log_msg("REGEX", "regex_focus");
  buf_move(lm->hndl->buf, line_diff(to, from), 0);
}

void regex_pivot(LineMatch *lm)
{
  log_msg("REGEX", "regex_pivot");
  Buffer *buf = lm->hndl->buf;
  buf_move_invalid(buf, lm->pivot_top, lm->pivot_lnum);
}

void regex_mk_pivot(LineMatch *lm)
{
  log_msg("REGEX", "regex_mk_pivot");
  lm->pivot_top = buf_top(lm->hndl->buf);
  lm->pivot_lnum = buf_line(lm->hndl->buf);
}

void regex_hover(LineMatch *lm)
{
  int line = focus_cur_line(lm);

  if (!lm->lines || utarray_len(lm->lines) < 1)
    return;

  int *ret = nearest_next_match(lm->lines, line);
  if (ret)
    regex_focus(lm, *ret, line);
  else {
    NEXT_OR_WRAP(lm->lines, ret);
    regex_focus(lm, *ret, line);
  }
}

// pivot buffernode focus to closest match.
// varies by direction: '/', '?'
void regex_next(LineMatch *lm, int line)
{
  log_msg("REGEX", "regex_next");
  get_or_make_matches(lm);
  if (!lm->lines || utarray_len(lm->lines) < 1)
    return;

  int *ret = nearest_next_match(lm->lines, line);
  if (ret && *ret == line)
    NEXT_OR_WRAP(lm->lines, ret);
  regex_focus(lm, *ret, line);
}

void regex_prev(LineMatch *lm, int line)
{
  log_msg("REGEX", "regex_prev");
  get_or_make_matches(lm);
  if (!lm->lines || utarray_len(lm->lines) < 1)
    return;

  int *ret = nearest_next_match(lm->lines, line);
  if (ret && *ret >= line)
    PREV_OR_WRAP(lm->lines, ret);
  regex_focus(lm, *ret, line);
}
