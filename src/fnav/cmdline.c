#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include "fnav/lib/queue.h"

#include "fnav/cmdline.h"
#include "fnav/ascii.h"
#include "fnav/log.h"

typedef struct {
  Token *item;
  QUEUE node;
} queue_item;

QUEUE stack;

void cmdline_init(Cmdline *cmdline, int size)
{
  cmdline->line = malloc(size * sizeof(char*));
  memset(cmdline->line, '\0', size);
  QUEUE_INIT(&stack);
}

void cmdline_cleanup(Cmdline *cmdline)
{
  // cmdline_reset
  free(cmdline->line);
  // other freeing
}

void token_push(Token *token)
{
  queue_item *item = malloc(sizeof(queue_item));
  item->item = token;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_HEAD(&stack, &item->node);
}

static void list_init(typ_T *t)
{
  t->v_type = VAR_LIST;
  utarray_new(t->vval.v_list->items, &list_icd);
}

static void dict_init(typ_T *t)
{
  t->v_type = VAR_DICT;
  utarray_new(t->vval.v_list->items, &dict_icd);
}

static Token* token_pop()
{
  QUEUE *h = QUEUE_HEAD(&stack);
  queue_item *item = QUEUE_DATA(&h, queue_item, node);
  QUEUE_REMOVE(&item->node);
  Token *tok = item->item;
  free(item);
  return tok;
}

static void pop(String string, Token *token, int pos)
{
  Token *parent = token_pop();

  // if token->typ string
  //  copy string start, end
  //  if this string is numeric
  //    convert to int. adjust type
  // return

  if (token->block && string[pos] != token->esc) {
    // error
  }

  // switch(parent typ)
  //  case: array or dict
  //    pushback_list(parent, token)
  //  case: pair
  //    parent->value = token
  //  default:
  //    error, out of possible options
}

static Token* push(String string, int pos, int type)
{
  Token *token;
  //init type
  //if pair
  //  it = queue_remove_head
  //  token->key = it;
  //queue_push_head token
  return token;
}

void cmdline_reset(Cmdline *cmdline)
{
}

void cmdline_tokenize(Cmdline *cmdline)
{
  char ch;
  int st, ed, pos;
  char *str = cmdline->line;
  utarray_new(cmdline->tokens, &list_icd);

  st = ed = pos = 0;
  while ((ch = str[pos++]) != '\0') {
    int dif = ed - st;
    if (ch == ' ' && dif > 0) {
      ed = pos;
      char *vstr = malloc(dif);
      strncpy(vstr, &str[pos], dif);
      Token token = { 
        .start = st,
        .end = ed,
        .var = {
          .v_type = VAR_STRING,
          .vval.v_string = vstr
        }
      };
      utarray_push_back(cmdline->tokens, &token);
      st = pos;
    }
    else {
      st = pos;
    }
}

void cmdline_build(Cmdline *cmdline)
{
  log_msg("CMDSTR", "tokenize_line");
  cmdline_reset(cmdline);
  char ch;

  char *str = cmdline->line;
  int pos = 0;
  int len;

  Cmdstr cmdstr;
  utarray_new(cmdline->cmds, &cmd_icd);
  list_init(&cmdstr.args.var);
  utarray_push_back(cmdline->cmds, &cmdstr);
  Token *token = push(str, 0, VAR_LIST);

  // FIXME: tokenize first to created valid structure to operate on
  //        parse after

  while ((ch = str[pos++]) != '\0') {
    len = token->start - pos;
    switch(ch) {
      //
      //
      // #token made in case. set token esc
      // push(stack, string, token, pos)
      // {
      //  switch ch
      //  case '\0'          -> error
      //  case |>            -> pop  #with cpipe flag
      //  case |             -> pop  #with pipe flag
      //  case colon         -> push(..,.., token->esc) #change word into pair
      //  case ,             -> pop #if len < 1, skip
      //  case _             -> pop #if len < 1, skip
      //  case }             -> pop #if len < 1, skip
      // }
      case '"':
        token = push(str, pos, VAR_STRING);
        char *cur = &str[pos];
        char *ret = strchr(cur, '"');
        if (ret) {
          int end = cur - ret;
          pop(str, token, end);
        }
        break;
      case '\0':
        // error
        break;
      case '{':
        push(str, VAR_DICT, pos);
        break;
      case '}':
        pop(str, token, pos);
        break;
      case '[':
        push(str, VAR_LIST, pos);
        break;
      case ']':
        pop(str, token, pos);
        break;
      default:
        token = push(str, pos, VAR_STRING);
    }
  }
}
