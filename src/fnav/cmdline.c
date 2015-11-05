#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include "fnav/lib/queue.h"

#include "fnav/cmdline.h"
#include "fnav/ascii.h"
#include "fnav/log.h"

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
}

void cmdline_cleanup(Cmdline *cmdline)
{
  // cmdline_reset
  free(cmdline->line);
  // other freeing
}

void cmdline_destroy(Cmdline *cmdline)
{
}

void token_push(QUEUE *queue, Token token)
{
  queue_item *item = malloc(sizeof(queue_item));
  item->item = token;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_HEAD(queue, &item->node);
}

static queue_item *queue_node_data(QUEUE *queue)
{
  return QUEUE_DATA(queue, queue_item, node);
}

static Token token_pop(QUEUE *stack)
{
  QUEUE *h = QUEUE_HEAD(stack);
  queue_item *item = queue_node_data(h);
  QUEUE_REMOVE(&item->node);
  Token token = item->item;
  free(item);
  return token;
}

static void list_init(typ_T *t)
{
  t->v_type = VAR_LIST;
  t->vval.v_list = malloc(sizeof(List*));
  utarray_new(t->vval.v_list->items, &list_icd);
}

static void dict_init(typ_T *t)
{
  t->v_type = VAR_DICT;
  utarray_new(t->vval.v_list->items, &dict_icd);
}
int cmdline_prev_word(Cmdline *cmdline, int pos)
{
  return 0;
}

void cmdline_reset(Cmdline *cmdline)
{
  utarray_free(cmdline->cmds);
}

void cmdline_create_token(Cmdline *cmdline, char *str, int st, int ed)
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

void cmdline_tokenize(Cmdline *cmdline)
{
  char ch;
  int st, ed, pos;
  char *str = cmdline->line;
  utarray_new(cmdline->tokens, &list_icd);

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
  log_msg("CMDLINE", "pop");
  Token parent = token_pop(stack);
  // switch(parent typ)
  //  case: array or dict
  //    pushback_list(parent, token)
  //  case: pair
  //    parent->value = token
  //  default:
  //    error, out of possible options
}

static void push(Token *token, QUEUE *stack)
{
  log_msg("CMDLINE", "push");
  token_push(stack, *token);
  //if string
  //  if this string is numeric
  //    convert to int. adjust type
  //if pair
  //  it = queue_remove_head
  //  token->key = it;
  //queue_push_head token
}

void cmdline_cmdstr_new(Cmdline *cmdline, Cmdstr *cmd, QUEUE *stack)
{
  log_msg("CMDLINE", "cmdline_cmdstr_new");
  /* create base cmdstr to hold all subtypes */
  QUEUE_INIT(stack);
  list_init(&cmd->args.var);
  utarray_push_back(cmdline->cmds, cmd);
}

void cmdline_parse(Cmdline *cmdline)
{
  log_msg("CMDLINE", "cmdline_parse");
  char ch;
  Cmdstr cmd;
  QUEUE *stack = &cmd.stack;

  cmdline_cmdstr_new(cmdline, &cmd, stack);
  token_push(stack, cmd.args);

  Token *word = NULL;
  Token *block = NULL;
  while( (word = (Token*)utarray_next(cmdline->tokens, word))) {
    char *str = word->var.vval.v_string;
    switch(ch = str[0]) {
      case '|':
        if (str[1] == '>')
          cmd.pipet = PIPE_CNTLR;
        else
          cmd.pipet = PIPE_CMD;
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
        dict_init(&block->var);
        push(block, stack);
        break;
      case '[':
        list_init(&block->var);
        push(block, stack);
        break;
      default:
        push(word, stack);
    }
    pop(stack);
    log_msg("_", "%s", str);
  }
breakout:
  return;
}

void cmdline_build(Cmdline *cmdline)
{
  log_msg("CMDSTR", "tokenize_line");


  cmdline_tokenize(cmdline);

  /* init cmdline list of cmds */
  utarray_new(cmdline->cmds, &cmd_icd);

  cmdline_parse(cmdline);

  cmdline_reset(cmdline);
}
