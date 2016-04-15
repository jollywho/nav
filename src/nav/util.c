#include <wchar.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "nav/util.h"
#include "nav/macros.h"

#define init_mb(state) memset(&state, 0, sizeof(state))
#define reset_mbytes(state) init_mb(state)
#define check_mbytes(wch,buffer,length,state) \
	(int) mbrtowc(&wch, buffer, length, &state)

static wchar_t wstr[CCHARW_MAX + 1];
static cchar_t wbuf[PATH_MAX];

wchar_t* str2wide(char *src)
{
  int len = mbstowcs(NULL, src, 0) + 1;
  wchar_t *dest = malloc((len+1)*sizeof(wchar_t));
  mbstowcs(dest, src, len);
  return dest;
}

char* wide2str(wchar_t *src)
{
  int len = wcstombs(NULL, src, 0) + 1;
  char *dest = malloc((len+1)*sizeof(char));
  wcstombs(dest, src, len);
  if (!dest)
    dest = "";
  return dest;
}

int cell_len(char *str)
{
  unsigned len = strlen(str);
  unsigned j;
  wchar_t wch;
  size_t rc;
  int width;
  int cnt = 0;
  mbstate_t state;
  reset_mbytes(state);
  for (j = 0; j < len; j++) {
    rc = check_mbytes(wch, str + j, len - j, state);
    if (rc == (size_t) -1 || rc == (size_t) -2)
      break;
    j += rc - 1;
    if ((width = wcwidth(wch)) < 0)
      break;
    cnt += width;
  }
  return cnt;
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

char* strncat_shell(char *dest, const char *src, int n)
{
  char c;
  int len = 0;
  *(dest++) = '\'';
  while (len < n) {
    c = src[len++];
    switch(c) {
      case '\'':
        *(dest++) = '\'';
        *(dest++) = '\\';
        *(dest++) = '\'';
      default:
        *(dest++) = c;
    }
  }
  *(dest++) = '\'';
  return dest;
}

void trans_char(char *src, char from, char to)
{
  while (*src) {
    if (*src == from)
      *src = to;
    src++;
  }
}

char* lines2argv(char *src)
{
  int i, escs, lines;
  for (i = lines = escs = 0; src[i]; i++) {
    if (src[i] == '\n')
      lines+=3;
    else if (src[i] == '\'')
      escs+=3;
  }
  int len = i + escs + lines + 2;
  char *dest = malloc(len+1);
  char *buf = dest;

  char delim = '\n';
  if (lines == 0) {
    i++;
    delim = '\0';
  }

  int prev = 0;
  for (int j = 0; j < i; j++) {
    if (src[j] != delim)
      continue;

    if (prev > 0)
      *(buf++) = ' ';

    int pos = (j - prev);
    buf = strncat_shell(buf, &src[prev], pos);
    prev = j + 1;

    /* break before next iteration */
    if (src[j] == '\0')
      break;
  }
  *buf = '\0';

  return dest;
}

void del_param_list(char **params, int argc)
{
  if (argc > 0) {
    for (int i = 0; i < argc; i++)
      free(params[i]);
    free(params);
  }
}

char* strip_quotes(char *_str)
{
  if (*_str == '"')
    _str++;

  char *str = strdup(_str);
  int i;
  for (i = 0; str[i] != '\0'; ++i);

  if (str[i-1] == '"')
    str[i-1] = '\0';

  return str;
}

bool fuzzy_match(char *s, const char *accept)
{
  char *sub = s;
  for (int i = 0; accept[i]; i++) {
    int c = TOUPPER_ASC(accept[i]);
    char *next = strchr(sub, c);
    if (!next) {
      c = TOLOWER_ASC(accept[i]);
      next = strchr(sub, c);
    }
    if (!next)
      return false;
    sub = next;
  }
  return true;
}
