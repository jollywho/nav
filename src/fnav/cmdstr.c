#include <ctype.h>
#include "fnav/cmdstr.h"
#include "fnav/log.h"
#include "fnav/lib/utarray.h"

void trim(char *str)
{
  for (size_t i=0, j=0; str[j]; j+=!isspace(str[i++]))
    str[j] = str[i];
}

struct Token {
  char *word;
  int type;
};

#define PUTCH(word,len,ch) \
  word = realloc(word, len + 1); \
  word[len++] = ch;

char* addword(char **str, UT_array *tokens)
{
  char ch;
  char* ptr;
  size_t len;

  char *word = malloc(1);
  len = 0;
  ptr = *str;
  --ptr;

  while (*ptr != '\0') {
    switch(ch = *ptr++) {
      case ' ':
        goto pushtoken;
      case ':':
        goto pushtoken;
      case '}':
        --ptr;
        goto pushtoken;
      case '>':
        PUTCH(word, len, ch);
        goto pushtoken;
      case '<':
        PUTCH(word, len, ch);
        goto pushtoken;
      default:
        PUTCH(word, len, ch);
    }
  }
pushtoken:
  if (len > 0) {
    word[len] = '\0';
//    log_msg("CMDSTR", "word: %s\n", word);
    utarray_push_back(tokens, &word);
  }
  return ptr;
}

char* addstring(char **str, UT_array *tokens)
{
  char ch;
  char* ptr;
  size_t len;

  char *word = malloc(1);
  len = 0;
  ptr = *str;

  while (*ptr != '\0') {
    switch(ch = *ptr++) {
      case '"':
        ++ptr;
        goto pushtoken;
      default:
        PUTCH(word, len, ch);
    }
  }
pushtoken:
  if (len > 0) {
    //  printf("string: %s %d\n", word, len);
    word[len] = '\0';
    utarray_push_back(tokens, &word);
  }
  return ptr;
}

char* addbrace(char **str, UT_array *tokens)
{
  char ch;
  char* ptr;
  ptr = *str;

  while (*ptr != '\0') {
    switch(ch = *ptr++) {
      case '}':
        goto pushbrace;
      case '"':
        ptr = addstring(&ptr, tokens);
      default:
        ptr = addword(&ptr, tokens);
    }
  }
pushbrace:
  return ptr;
}

void tokenize_line(char **str)
{
  log_msg("CMDSTR", "tokenize_line");
  char ch;
  char *ptr;
  UT_array *tokens;
  utarray_new(tokens, &ut_str_icd);

  ptr = *str;
  while (*ptr != '\0') {
    switch(ch = *ptr++) {
      case '"':
        ptr = addstring(&ptr, tokens);
        break;
      case '|':
        break;
      case '{':
        ptr = addbrace(&ptr, tokens);
        break;
      case '[':
        break;
      default:
        ptr = addword(&ptr, tokens);
    }
  }
  char **p = NULL;
  while ( (p=(char**)utarray_next(tokens,p))) {
    log_msg("test", "%s\n", *p);
  }
}
