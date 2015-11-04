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
  Token *item;
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

void token_push(QUEUE *queue, Token *token)
{
  queue_item *item = malloc(sizeof(queue_item));
  item->item = token;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_HEAD(queue, &item->node);
}

static Token* token_pop(QUEUE *stack)
{
  QUEUE *h = QUEUE_HEAD(stack);
  queue_item *item = QUEUE_DATA(&h, queue_item, node);
  QUEUE_REMOVE(&item->node);
  Token *tok = item->item;
  free(item);
  return tok;
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
    if (ch == ' ') {
      cmdline_create_token(cmdline, str, st, ed);
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

static void pop(String string, Token *token, int end, QUEUE *stack)
{
  Token *parent = token_pop(stack);

  // if token->typ string
  //  copy string start, end
  //  if this string is numeric
  //    convert to int. adjust type
  // return

  // switch(parent typ)
  //  case: array or dict
  //    pushback_list(parent, token)
  //  case: pair
  //    parent->value = token
  //  default:
  //    error, out of possible options
}

static Token* push(String string, int start, QUEUE *stack, int type)
{
  Token *token;
  //init type
  //if pair
  //  it = queue_remove_head
  //  token->key = it;
  //queue_push_head token
  return token;
}

void cmdline_cmdstr_new(Cmdline *cmdline, Cmdstr *cmd, QUEUE *stack)
{
  log_msg("CMDLINE", "cmdline_cmdstr_new");
  /* create base cmdstr to hold all subtypes */
  cmd->args.start = 0;
  QUEUE_INIT(stack);
  list_init(&cmd->args.var);
  utarray_push_back(cmdline->cmds, cmd);
}

void cmdline_parse(Cmdline *cmdline)
{
  log_msg("CMDLINE", "cmdline_parse");
  char ch;
  int pos = 0;
  QUEUE stack;
  Cmdstr cmd;

  cmdline_cmdstr_new(cmdline, &cmd, &stack);
  log_msg("CMDLINE", "cmdline_parse");
  Token *word, *token;
  word = (Token*)utarray_front(cmdline->tokens);
  token_push(&stack, &cmd.args);
  word = NULL;

  while( (word = (Token*)utarray_next(cmdline->tokens, word))) {
    char *str = word->var.vval.v_string;
    //while ((ch = str[pos++]) != '\0') {
    //  switch(ch) {
    //    //  case colon         -> push(..,.., token->esc) #change word into pair
    //    case '"':
    //      t = push(str, pos, stack, VAR_STRING);
    //      char *cur = &cmdline->line[word->start+pos];
    //      char *ret = strchr(cur, '"');
    //      if (ret) {
    //        int end = cur - ret;
    //        pop(str, token, end);
    //      }
    //      break;
    //    case '|':
    //      if (str[pos++] == '>')
    //        cmd.pipet = PIPE_CNTLR;
    //      else
    //        cmd.pipet = PIPE_CMD;;
    //      goto breakout;
    //    case ',':
    //      pop(str, token, pos);
    //      break;
    //    case '{':
    //      push(str, VAR_DICT, pos);
    //      break;
    //    case '[':
    //      push(str, VAR_LIST, stack, pos);
    //      break;
    //    case '}':
    //      pop(str, token, pos, &stack);
    //      break;
    //    case ']':
    //      pop(str, token, pos, &stack);
    //      break;
    //    default:
    //      token = push(str, pos, stack, VAR_STRING);
    //  }
    //}
    //pop(str, t, pos, &stack);
    log_msg("_", "%s", word->var.vval.v_string);
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

  cmdline_parse(cmdline);
}
