#include <pcre.h>

#include "fnav/regex.h"
#include "fnav/log.h"
#include "fnav/model.h"
#include "fnav/tui/window.h"

#define NSUBEXP  10

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

  comp = pcre_compile(regex, 0, &pcreErrorStr, &pcreErrorOffset, NULL);

  if (comp == NULL) {
    log_msg("REGEX", "COMPILE ERROR: %s, %s", regex, pcreErrorStr);
    return;
  }

  extra = pcre_study(comp, 0, &pcreErrorStr);

  if(pcreErrorStr != NULL) {
    log_msg("REGEX", "COULD NOT STUDY: %s, %s", regex, pcreErrorStr);
    return;
  }

  Model *m = window_get_focus()->buf->hndl->model;
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
        log_msg("REGEX", "match %s", subject);
      }
      pcre_free_substring(match);
    }
  }
  pcre_free(regex);
  if(extra != NULL)
    pcre_free(extra);
}

// stop pivot
void regex_req_enter()
{
  log_msg("REGEX", "regex_req_enter");
}

// use active buffernode's built match.
// find next entry closest to cursor positon in forward direction,
// wrapping on edges.
void regex_next()
{
  log_msg("REGEX", "regex_next");
}

// pivot buffernode focus to next match
void regex_focus()
{
  log_msg("REGEX", "regex_focus");
}
