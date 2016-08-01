#include <pcre.h>
#include "nav/lib/utarray.h"

#include "nav/tui/buffer.h"
#include "nav/regex.h"
#include "nav/log.h"
#include "nav/model.h"

#define NSUBEXP  5

struct Pattern {
  pcre *pcre;
  pcre_extra *extra;
};

struct LineMatch {
  UT_array *lines;
  Handle *hndl;
  int pivot_top;
  int pivot_lnum;
  char *gcomp;
};

static char* gcomp; /* shared regex for all buffers */
static int gregsign;

LineMatch* regex_new(Handle *hndl)
{
  LineMatch *lm = malloc(sizeof(LineMatch));
  memset(lm, 0, sizeof(LineMatch));
  lm->hndl = hndl;
  utarray_new(lm->lines, &ut_int_icd);
  return lm;
}

void regex_destroy(Handle *hndl)
{
  LineMatch *lm = hndl->buf->matches;
  if (lm->lines)
    utarray_free(lm->lines);
  free(lm);
}

void regex_setsign(int sign)
{
  gregsign = sign;
}

static void regex_compile(const char *comp, pcre **pcre, pcre_extra **extra)
{
  const char *pcreErrorStr;
  int pcreErrorOffset;

  *pcre = pcre_compile(comp,
      PCRE_CASELESS,
      &pcreErrorStr,
      &pcreErrorOffset,
      NULL);

  if (pcre == NULL) {
    log_msg("REGEX", "COMPILE ERROR: %s, %s", comp, pcreErrorStr);
    return;
  }

  *extra = pcre_study(*pcre, 0, &pcreErrorStr);

  if(pcreErrorStr != NULL) {
    log_msg("REGEX", "COULD NOT STUDY: %s, %s", comp, pcreErrorStr);
    return;
  }
}

char* regex_str(LineMatch *lm)
{
  if (lm->gcomp)
    return lm->gcomp;
  return gcomp;
}

int regex_match_count(LineMatch *lm)
{
  if (!lm->lines)
    return -1;
  return utarray_len(lm->lines);
}

void regex_build(LineMatch *lm, const char *line)
{
  log_msg("REGEX", "build");
  log_msg("REGEX", ":%s:", line);
  pcre *pcre = NULL;
  pcre_extra *extra = NULL;

  if (line)
    SWAP_ALLOC_PTR(gcomp, strdup(line));
  if (!gcomp)
    return;
  lm->gcomp = gcomp;

  regex_compile(gcomp, &pcre, &extra);

  regex_del_matches(lm);
  utarray_new(lm->lines, &ut_int_icd);

  int max = model_count(lm->hndl->model);
  for (int i = 0; i < max; i++) {
    int substr[NSUBEXP];
    const char *match;

    char* subject = model_str_line(lm->hndl->model, i);

    int ret = pcre_exec(pcre,
        extra,
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
  pcre_free(extra);
  pcre_free(pcre);
}

void regex_del_matches(LineMatch *lm)
{
  log_msg("REGEX", "regex_del_matches");
  if (lm->lines)
    utarray_free(lm->lines);
  lm->lines = NULL;
}

Pattern* regex_pat_new(const char *regex)
{
  Pattern *pat = malloc(sizeof(Pattern));
  regex_compile(regex, &pat->pcre, &pat->extra);
  return pat;
}

void regex_pat_delete(Pattern *pat)
{
  pcre_free(pat->extra);
  pcre_free(pat->pcre);
  free(pat);
}

bool regex_match(Pattern *pat, const char *line)
{
  if (!line)
    return false;

  int substr[NSUBEXP];
  const char *match;
  bool succ = false;
  int ret = pcre_exec(pat->pcre,
      pat->extra,
      line,
      strlen(line),     // length of string
      0,                // Start looking at this point
      0,                // OPTIONS
      substr,
      NSUBEXP);         // Length of substr
  if (ret == 0)
    ret = NSUBEXP / 3;
  if (ret > -1) {
    pcre_get_substring(line, substr, ret, 0, &(match));
    succ = true;
    pcre_free_substring(match);
  }
  return succ;
}

static int focus_cur_line(LineMatch *lm)
{
  return lm->pivot_top + lm->pivot_lnum;
}

static void ensure_global_match(LineMatch *lm)
{
  if (!lm->lines || lm->gcomp != gcomp) {
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

static void regex_focus(LineMatch *lm, int to)
{
  log_msg("REGEX", "regex_focus");
  Buffer *buf = lm->hndl->buf;
  int dif = to - buf_index(buf);
  if (dif != 0)
    buf_move(buf, dif, 0);
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

  if (!lm->lines || utarray_len(lm->lines) < 1) {
    regex_pivot(lm);
    return;
  }

  int *ret = nearest_next_match(lm->lines, line);
  if (ret)
    regex_focus(lm, *ret);
  else {
    ret = (int*)utarray_next(lm->lines, ret); \
    ret = ret ? ret : (int*)utarray_next(lm->lines, ret);
    regex_focus(lm, *ret);
  }
}

// pivot buffernode focus to closest match.
// varies by direction: '/', '?'
// return next match index.
int regex_next(LineMatch *lm, int line, int dir)
{
  log_msg("REGEX", "regex_next");
  ensure_global_match(lm);
  if (!lm->lines || utarray_len(lm->lines) < 1)
    return -1;

  int *ret = nearest_next_match(lm->lines, line);
  if (!ret)
    ret = (int*)utarray_front(lm->lines);

  if (ret && *ret == line) {
    if (gregsign * dir > 0) {
      ret = (int*)utarray_next(lm->lines, ret); \
      ret = ret ? ret : (int*)utarray_next(lm->lines, ret);
    }
    else {
      ret = (int*)utarray_prev(lm->lines, ret); \
      ret = ret ? ret : (int*)utarray_prev(lm->lines, ret);
    }
  }
  if (ret) {
    regex_focus(lm, *ret);
    return utarray_eltidx(lm->lines, ret);
  }
  return -1;
}
