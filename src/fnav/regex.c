#include <pcre.h>

#include "fnav/regex.h"
#include "fnav/log.h"
#include "fnav/tui/window.h"

struct Regexmatch {
  pcre *compiled;
  pcre_extra *extra;
  String regex;
};

Regexmatch g_reg;

// compile pcre for each line in all window buffers
// add built list to each buffernode
void regex_build(String line)
{
  log_msg("REGEX", "build");
  const char *pcreErrorStr;
  int pcreErrorOffset;
  g_reg.regex = line;
  g_reg.compiled = pcre_compile(line, 0, &pcreErrorStr, &pcreErrorOffset, NULL);
  if (g_reg.compiled == NULL) {
    log_msg("REGEX", "COMPILE ERROR: %s, %s", line, pcreErrorStr);
  }
  g_reg.extra = pcre_study(g_reg.compiled, 0, &pcreErrorStr);
}

// stop pivot
void regex_req_enter()
{
}

// use active buffernode's built match.
// find next entry closest to cursor positon in forward direction,
// wrapping on edges.
void regex_next()
{
}

// pivot buffernode focus to next match
void regex_focus()
{
}
