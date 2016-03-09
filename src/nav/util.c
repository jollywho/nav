#include <ncurses.h>
#include <wchar.h>
#include <string.h>
#include <malloc.h>
#include "nav.h"

#define init_mb(state) memset(&state, 0, sizeof(state))
#define reset_mbytes(state) init_mb(state)
#define count_mbytes(buffer,length,state) mbrlen(buffer,length,&state)
#define check_mbytes(wch,buffer,length,state) \
	(int) mbrtowc(&wch, buffer, length, &state)

static wchar_t wstr[CCHARW_MAX + 1];

void trans_str_wide(char *src, cchar_t dst[], int max)
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
    if ((width > 0 && l > 0) || l == CCHARW_MAX) {
      wstr[l] = L'\0';
      l = 0;
      if (setcchar(dst + k, wstr, 0, 0, NULL) != OK)
        break;
      ++k;
      cnt += width;
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
    if (setcchar(dst + k, wstr, 0, 0, NULL) == OK)
      ++k;
  }
  wstr[0] = L'\0';
  setcchar(dst + k, wstr, 0, 0, NULL);
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
