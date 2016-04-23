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

void conspath_buf(char *buf, char *base, char *name)
{
  strcpy(buf, base);
  strcat(buf, "/");
  strcat(buf, name);
}

char* escape_shell(char *src)
{
  const char escs[] = "() []";
  char c;
  int len = 0;

  char *str = src;
  while ((c = *(str++))) {
    for (int i = 0; i < LENGTH(escs); i++) {
      if (escs[i] == c)
        len++;
    }
    len++;
  }

  char *buf = malloc(len+1);
  char *dest = buf;

  str = src;
  while ((c = *(str++))) {
    for (int i = 0; i < LENGTH(escs); i++) {
      if (escs[i] == c)
        *(buf++) = '\\';
    }
    *(buf++) = c;
  }
  *buf = '\0';
  return dest;
}

char* strncat_shell(char *dest, const char *src)
{
  char c;
  *(dest++) = '\'';
  while ((c = *(src++))) {
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

int count_lines(char *src)
{
  char *next = NULL;
  int count = 0;
  while ((next = strstr(src, "\n"))) {
    next++;
    if (!next)
      break;
    if (*src != '#')
      count++;
    src = next;
  }
  return count;
}

char* lines2argv(int argc, char **argv)
{
  int len = 0;
  for (int i = 0; i < argc; i++) {
    len += 3;  //quotes + space
    for (int j = 0; argv[i][j]; j++) {
      len++;
      if (argv[i][j] == '\'')
        len+=3;  //escapes
    }
  }

  char *dest = malloc(len+1);
  char *buf = dest;

  for (int i = 0; i < argc; i++) {
    if (i > 0)
      *(buf++) = ' ';
    buf = strncat_shell(buf, argv[i]);
  }
  *buf = '\0';

  return dest;
}

char* lines2yank(int argc, char **argv)
{
  int len = 0;
  for (int i = 0; i < argc; i++)
    len += strlen(argv[i]) + 1;

  char *dest = malloc(len+1);
  *dest = '\0';
  for (int i = 0; i < argc; i++) {
    if (i > 0)
      strcat(dest, "\n");
    strcat(dest, argv[i]);
  }

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

char* strip_quotes(const char *src)
{
  if (*src == '"')
    src++;

  char *dest = strdup(src);
  int i;
  for (i = 0; dest[i] != '\0'; ++i);

  if (i > 0 && dest[i-1] == '"')
    dest[i-1] = '\0';

  return dest;
}

char* add_quotes(char *src)
{
  int len = strlen(src) + 3;
  char *dest = malloc(len);
  snprintf(dest, len, "\"%s\"", src);
  return dest;
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
