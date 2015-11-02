#include <stdint.h>

#include <ctype.h>
#include "fnav/cmdstr.h"
#include "fnav/log.h"

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
  dst->word = NULL;
  dst->argt = WORD_T;
}

void token_dtor(void *_elt) {
  Token *elt = (Token*)_elt;
  if (elt->word) free(elt->word);
}

UT_icd icd = {sizeof(Token), NULL, NULL, token_dtor};

void cmdstr_init(Cmdstr *cmdstr)
{
  utarray_new(cmdstr->tokens, &icd);
}

void cmdstr_cleanup(Cmdstr *cmdstr)
{
  utarray_free(cmdstr->tokens);
}

int cmdstr_prev_word(Cmdstr *cmdstr, int pos)
{
  return 0;
}

static void PUTCH(char **str, size_t *len, char ch){
  size_t *l = len;
  char* tmp = realloc(*str, *l+2);
  (*str)=tmp;
  (*str)[*l]= ch;
  (*str)[*l+1]='\0';
  ++(*l);
}

static char* addword(char **str, UT_array *tokens, uint32_t type)
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
        PUTCH(&tok.word, &len, ch);
        goto pushtoken;
      case '<':
        PUTCH(&tok.word, &len, ch);
        goto pushtoken;
      default:
        PUTCH(&tok.word, &len, ch);
    }
  }
pushtoken:
  if (len > 0) {
    tok.word[len] = '\0';
    tok.argt |= type;
    utarray_push_back(tokens, &tok);
  }
  else
    token_dtor(&tok);
  return ptr;
}

static char* addstring(char **str, UT_array *tokens, uint32_t type)
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
        PUTCH(&tok.word, &len, ch);
    }
  }
pushtoken:
  if (len > 0) {
    tok.word[len] = '\0';
    tok.argt |= type;
    utarray_push_back(tokens, &tok);
  }
  else
    token_dtor(&tok);
  return ptr;
}

static char* addarray(char **str, UT_array *tokens, uint32_t type)
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

static char* addbrace(char **str, UT_array *tokens, uint32_t type)
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

void tokenize_line(Cmdstr *cmdstr, char * const *str)
{
  log_msg("CMDSTR", "tokenize_line");
  char ch;
  char *ptr;
  UT_array *tokens = cmdstr->tokens;

  ptr = (char*)*str;
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
