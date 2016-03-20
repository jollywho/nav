#include <wchar.h>
#include <limits.h>
#include <string.h>
#include <malloc.h>
#include "nav/util.h"

#define init_mb(state) memset(&state, 0, sizeof(state))
#define reset_mbytes(state) init_mb(state)
#define count_mbytes(buffer,length,state) mbrlen(buffer,length,&state)
#define check_mbytes(wch,buffer,length,state) \
	(int) mbrtowc(&wch, buffer, length, &state)

static wchar_t wstr[CCHARW_MAX + 1];
static cchar_t wbuf[PATH_MAX];
static Exparg *gexp;

void set_exparg(Exparg *exparg)
{
  gexp = exparg;
}

bool exparg_isset()
{
  return gexp;
}

void draw_wide(WINDOW *win, int row, int col, char *src, int max)
{
  unsigned len = strlen(src);
  unsigned j, k;
  wchar_t wch;
  int l = 0;
  size_t rc;
  int width;
  int cnt = 0;
  mbstate_t state;
  reset_mbytes(state);
  for (j = k = 0; j < len; j++) {
    rc = check_mbytes(wch, src + j, len - j, state);
    if (rc == (size_t) -1 || rc == (size_t) -2)
      break;
    j += rc - 1;
    if ((width = wcwidth(wch)) < 0)
      break;
    cnt += width;
    if ((width > 0 && l > 0) || l == CCHARW_MAX) {
      wstr[l] = L'\0';
      l = 0;
      if (setcchar(wbuf + k, wstr, 0, 0, NULL) != OK)
        break;
      ++k;
    }
    if (width == 0 && l == 0)
      wstr[l++] = L' ';
    wstr[l++] = wch;
    if (cnt > max) {
      wstr[l-1] = L'…';
      break;
    }
  }
  if (l > 0) {
    wstr[l] = L'\0';
    if (setcchar(wbuf + k, wstr, 0, 0, NULL) == OK)
      ++k;
  }
  wstr[0] = L'\0';
  setcchar(wbuf + k, wstr, 0, 0, NULL);
  mvwadd_wchstr(win, row, col, wbuf);
}

void readable_fs(double size/*in bytes*/, char buf[])
{
  int i = 0;
  const char *units[] = {"B", "K", "M", "G", "T", "P"};
  while (size > 1024) {
    size /= 1024;
    i++;
  }
  if (size < 10)
    sprintf(buf, "%3.2f%s", size, units[i]);
  else if (size < 100)
    sprintf(buf, "%2.1f%s", size, units[i]);
  else
    sprintf(buf, "%4.0f%s", size, units[i]);
}

void expand_escapes(char *dest, const char *src)
{
  char c;
  while ((c = *(src++))) {
    switch(c) {
      case ' ':
        *(dest++) = '\\';
        *(dest++) = ' ';
        break;
      default:
        *(dest++) = c;
    }
  }
  *dest = '\0';
}

void del_param_list(char **params, int argc)
{
  if (argc > 0) {
    for (int i = 0; i < argc; i++)
      free(params[i]);
    free(params);
  }
}

char* do_expansion(char *src, Exparg *arg)
{
  char *line = strdup(src);
  if (!strstr(line, "%:"))
    return line;

  char *head = strtok(line, "%:");
  char *name = strtok(NULL, "%:");

  char *quote = "\"";
  char *delim = strchr(name, '"');
  //FIXME: misses remaining string after first delimit
  //some token/cmdstr context would be nice
  if (!delim) {
    delim = " ";
    quote = "";
  }

  name = strtok(name, delim);
  char *tail = strtok(NULL, delim);

  Exparg *exparg = arg;
  if (!exparg)
    exparg = gexp;

  char *body = exparg->expfn(name, exparg->key);
  if (!body)
    return line;

  if (!tail)
    tail = "";

  char *out;
  asprintf(&out, "%s\"%s\"%s%s", head, body, quote, tail);
  free(line);
  return out;
}
