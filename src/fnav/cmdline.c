#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include "fnav/lib/queue.h"

#include "fnav/cmdline.h"
#include "fnav/ascii.h"
#include "fnav/log.h"

static void cmdline_reset(Cmdline *cmdline);
UT_icd dict_icd = { sizeof(Pair),   NULL };
UT_icd list_icd = { sizeof(Token),  NULL };
UT_icd cmd_icd  = { sizeof(Cmdstr), NULL };

typedef struct {
  Token item;
  QUEUE node;
} queue_item;

void cmdline_init(Cmdline *cmdline, int size)
{
  cmdline->line = malloc(size * sizeof(char*));
  memset(cmdline->line, '\0', size);
  utarray_new(cmdline->cmds, &list_icd);
  utarray_new(cmdline->tokens, &list_icd);
}

void cmdline_cleanup(Cmdline *cmdline)
{
  cmdline_reset(cmdline);
  // other freeing
}

static void cmdline_reset(Cmdline *cmdline)
{
  utarray_free(cmdline->cmds);
  utarray_free(cmdline->tokens);
  utarray_new(cmdline->cmds, &list_icd);
  utarray_new(cmdline->tokens, &list_icd);
}

static void stack_push(QUEUE *queue, Token token)
{
  log_msg("CMDLINE", "stack_push");
  queue_item *item = malloc(sizeof(queue_item));
  item->item = token;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_HEAD(queue, &item->node);
}

static queue_item *queue_node_data(QUEUE *queue)
{
  return QUEUE_DATA(queue, queue_item, node);
}

static Token stack_pop(QUEUE *stack)
{
  log_msg("CMDLINE", "stack_pop");
  QUEUE *h = QUEUE_HEAD(stack);
  queue_item *item = queue_node_data(h);
  QUEUE_REMOVE(&item->node);
  Token token = item->item;
  free(item);
  return token;
}

static Token stack_head(QUEUE *stack)
{
  log_msg("CMDLINE", "stack_prev");
  QUEUE *h = QUEUE_HEAD(stack);
  queue_item *item = queue_node_data(h);
  Token token = item->item;
  return token;
}

static Token list_init()
{
  Token token;
  List *l = malloc(sizeof(List*));
  token.var.v_type = VAR_LIST;
  utarray_new(l->items, &list_icd);
  token.var.vval.v_list = l;
  return token;
}

static Token dict_init()
{
  Token token;
  Dict *d = malloc(sizeof(Dict*));
  token.var.v_type = VAR_DICT;
  utarray_new(d->items, &dict_icd);
  token.var.vval.v_dict = d;
  return token;
}

int cmdline_prev_word(Cmdline *cmdline, int pos)
{
  return 0;
}

static void cmdline_create_token(Cmdline *cmdline, char *str, int st, int ed)
{
  int len = ed - st;
  if (len < 1) return;
  char *vstr = malloc(len);
  strncpy(vstr, &str[st], len);
  vstr[len] = '\0';
  Token token = {
    .start = st,
    .end = ed,
    .var = {
      .v_type = VAR_STRING,
      .vval.v_string = vstr
    }
  };
  utarray_push_back(cmdline->tokens, &token);
}

static void cmdline_tokenize(Cmdline *cmdline)
{
  char ch;
  int st, ed, pos;
  char *str = cmdline->line;

  st = ed = pos = 0;
  for(;;) {
    ch = str[pos++];
    if (ch == '"') {
      char *ret = strchr(&str[pos], '"');
      if (ret) {
        int end = (ret - &str[pos]) + pos;
        cmdline_create_token(cmdline, str, pos, end);
        end++;
        pos = end;
        st = end;
      }
    }
    else if (strpbrk(&ch, ":|<>,[]{} ")) {
      cmdline_create_token(cmdline, str, st, ed);
      if (ch != ' ') {
        cmdline_create_token(cmdline, str, pos-1, pos);
        ed = pos;
      }
      st = pos;
    }
    else if (ch == '\0') {
      cmdline_create_token(cmdline, str, st, ed);
      break;
    }
    else
      ed = pos;
  }
}

static void pop(QUEUE *stack)
{
  Token token = stack_pop(stack);
  Token parent = stack_head(stack);
  log_msg("CMDLINE", "pop %d", token.var.v_type);
  if (parent.var.v_type == VAR_LIST) {
    log_msg("CMDLINE", "add");
    utarray_push_back(parent.var.vval.v_list->items, &token);
  }
  if (parent.var.v_type == VAR_DICT) {
    utarray_push_back(parent.var.vval.v_dict->items, &token);
  }
  //  case: pair
  //    parent->value = token
  //  default:
  //    error, out of possible options
}

static void push(Token token, QUEUE *stack)
{
  log_msg("CMDLINE", "push %d", token.var.v_type);
  stack_push(stack, token);
  //if string
  //  if this string is numeric
  //    convert to int. adjust type
  //if pair
  //  it = queue_remove_head
  //  token->key = it;
}

static void cmdline_parse(Cmdline *cmdline, Cmdstr *cmd)
{
  log_msg("CMDLINE", "cmdline_parse");
  char ch;

  QUEUE *stack = &cmd->stack;
  QUEUE_INIT(stack);

  cmd->args = list_init();
  utarray_push_back(cmdline->cmds, cmd);
  stack_push(stack, cmd->args);

  Token *word = NULL;
  while( (word = (Token*)utarray_next(cmdline->tokens, word))) {
    char *str = word->var.vval.v_string;
    switch(ch = str[0]) {
      case '|':
        if (str[1] == '>')
          cmd->pipet = PIPE_CNTLR;
        else
          cmd->pipet = PIPE_CMD;
        goto breakout;
      case '"':
        break;
      case ',':
      case '}':
      case ']':
        /* fallthrough */
        pop(stack);
        break;
        //
      case ':':
        //pair_init(&block->var);
        //push(block, &stack);
        break;
      case '{':
        push(dict_init(), stack);
        break;
      case '[':
        push(list_init(), stack);
        break;
      default:
        push(*word, stack);
        pop(stack);
    }
  }
breakout:
  return;
}

void cmdline_build(Cmdline *cmdline)
{
  log_msg("CMDSTR", "tokenize_line");

  cmdline_reset(cmdline);
  cmdline_tokenize(cmdline);

  /* init cmdline list of cmds */
  utarray_new(cmdline->cmds, &cmd_icd);

  Cmdstr cmd;
  cmdline_parse(cmdline, &cmd);
  Token *word = NULL;
  while( (word = (Token*)utarray_next(cmd.args.var.vval.v_list->items, word))) {
    log_msg("_", "%d", word->var.v_type);
  }
}
