#include <stdint.h>
#include <ctype.h>
#include "fnav/cmdstr.h"
#include "fnav/log.h"
#include "fnav/lib/utarray.h"

void trim(char *str)
{
  for (size_t i=0, j=0; str[j]; j+=!isspace(str[i++]))
    str[j] = str[i];
}

typedef struct Token Token;

#define WORD_T     0x01
#define KEY_T      0x02
#define BRACE_T    0x04
#define ARRAY_T    0x08

struct Token {
  uint32_t argt;
  char *word;
};

void token_init(void *elt) {
  Token *dst = (Token*)elt;
  dst->word = malloc(sizeof(1));
  dst->argt = WORD_T;
}
void token_copy(void *_dst, const void *_src) {
  Token *dst = (Token*)_dst, *src = (Token*)_src;
  dst->argt = src->argt;
  dst->word = src->word ? strdup(src->word) : NULL;
}

void token_dtor(void *_elt) {
  Token *elt = (Token*)_elt;
  if (elt->word) free(elt->word);
}

UT_icd icd = {sizeof(Token), token_init, token_copy, token_dtor};

#define PUTCH(word,len,ch) \
  word = realloc(word, len + 1); \
  word[len++] = ch;

char* addword(char **str, UT_array *tokens, uint32_t type)
{
  char ch;
  char* ptr;
  size_t len;

  Token tok;
  token_init(&tok);
  len = 0;
  ptr = *str;
  --ptr;

  while (*ptr != '\0') {
    switch(ch = *ptr++) {
      case ' ':
        goto pushtoken;
      case ':':
        tok.argt |= KEY_T;
        goto pushtoken;
      case ']':
        --ptr;
        goto pushtoken;
      case '}':
        --ptr;
        goto pushtoken;
      case ',':
        goto pushtoken;
      case '>':
        PUTCH(tok.word, len, ch);
        goto pushtoken;
      case '<':
        PUTCH(tok.word, len, ch);
        goto pushtoken;
      default:
        PUTCH(tok.word, len, ch);
    }
  }
pushtoken:
  if (len > 0) {
    tok.word[len] = '\0';
    tok.argt |= type;
    //    log_msg("CMDSTR", "word: %s\n", word);
    utarray_push_back(tokens, &tok);
  }
  return ptr;
}

char* addstring(char **str, UT_array *tokens, uint32_t type)
{
  char ch;
  char* ptr;
  size_t len;

  Token tok;
  token_init(&tok);
  len = 0;
  ptr = *str;

  while (*ptr != '\0') {
    switch(ch = *ptr++) {
      case '"':
        ++ptr;
        goto pushtoken;
      default:
        PUTCH(tok.word, len, ch);
    }
  }
pushtoken:
  if (len > 0) {
    //  printf("string: %s %d\n", word, len);
    tok.word[len] = '\0';
    tok.argt |= type;
    utarray_push_back(tokens, &tok);
  }
  return ptr;
}

char* addarray(char **str, UT_array *tokens, uint32_t type)
{
  char ch;
  char* ptr;
  ptr = *str;

  while (*ptr != '\0') {
    switch(ch = *ptr++) {
      case ']':
        goto pusharray;
      case '"':
        ptr = addstring(&ptr, tokens, ARRAY_T | type);
      default:
        ptr = addword(&ptr, tokens, ARRAY_T | type);
    }
  }
pusharray:
  return ptr;
}

char* addbrace(char **str, UT_array *tokens, uint32_t type)
{
  char ch;
  char* ptr;
  ptr = *str;

  while (*ptr != '\0') {
    switch(ch = *ptr++) {
      case '}':
        goto pushbrace;
      case '[':
        ptr = addarray(&ptr, tokens, BRACE_T | type);
        break;
      case '"':
        ptr = addstring(&ptr, tokens, BRACE_T | type);
        break;
      default:
        ptr = addword(&ptr, tokens, BRACE_T | type);
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
  utarray_new(tokens, &icd);

  ptr = *str;
  while (*ptr != '\0') {
    switch(ch = *ptr++) {
      case '"':
        ptr = addstring(&ptr, tokens, WORD_T);
        break;
      case '|':
        break;
      case '{':
        ptr = addbrace(&ptr, tokens, BRACE_T);
        break;
      case '[':
        ptr = addarray(&ptr, tokens, ARRAY_T);
        break;
      default:
        ptr = addword(&ptr, tokens, WORD_T);
    }
  }
  Token *p = NULL;
  while ( (p=(Token*)utarray_next(tokens,p))) {
    log_msg("test", "%s %d", p->word, p->argt);
  }
}
