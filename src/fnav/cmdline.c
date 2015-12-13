#include <malloc.h>
#include <string.h>
#include <ctype.h>

#include "fnav/lib/queue.h"
#include "fnav/tui/cntlr.h"
#include "fnav/event/hook.h"
#include "fnav/cmdline.h"
#include "fnav/cmd.h"
#include "fnav/ascii.h"
#include "fnav/log.h"

static void cmdline_reset(Cmdline *cmdline);
static UT_icd dict_icd = { sizeof(Pair),   NULL };
static UT_icd list_icd = { sizeof(Token),  NULL };
static UT_icd cmd_icd  = { sizeof(Cmdstr), NULL };

typedef struct {
  Token item;
  QUEUE node;
} queue_item;

void cmdline_init_config(Cmdline *cmdline, char *line)
{
  cmdline->line = line;
  utarray_new(cmdline->cmds, &list_icd);
  utarray_new(cmdline->tokens, &list_icd);
}

void cmdline_init(Cmdline *cmdline, int size)
{
  cmdline->line = malloc(size * sizeof(char*));
  memset(cmdline->line, '\0', size);
  utarray_new(cmdline->cmds, &list_icd);
  utarray_new(cmdline->tokens, &list_icd);
}

void cmdline_cleanup(Cmdline *cmdline)
{
  //TODO: map $ free [cmds, tokens]
  utarray_free(cmdline->cmds);
  utarray_free(cmdline->tokens);
  free(cmdline->line);
  // other freeing
}

static void cmdline_reset(Cmdline *cmdline)
{
  utarray_free(cmdline->cmds);
  utarray_free(cmdline->tokens);
  utarray_new(cmdline->cmds, &cmd_icd);
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

static Token pair_init()
{
  Token token;
  Pair *p = malloc(sizeof(Pair));
  token.var.v_type = VAR_PAIR;
  token.var.vval.v_pair = p;
  return token;
}

static Token list_init()
{
  Token token;
  List *l = malloc(sizeof(List));
  token.var.v_type = VAR_LIST;
  utarray_new(l->items, &list_icd);
  token.var.vval.v_list = l;
  return token;
}

static Token dict_init()
{
  Token token;
  Dict *d = malloc(sizeof(Dict));
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
  char *vstr = malloc(sizeof(char*)*len);
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
  if (parent.var.v_type == VAR_PAIR) {
    stack_pop(stack);
    Token key = stack_pop(stack);
    parent.var.vval.v_pair->key = &key;
    parent.var.vval.v_pair->value = &token;
  }
}

static void push(Token token, QUEUE *stack)
{
  log_msg("CMDLINE", "push %d", token.var.v_type);
  if (token.var.v_type == VAR_STRING) {
    int temp;
    if (sscanf(token.var.vval.v_string, "%d", &temp)) {
      token.var.v_type = VAR_NUMBER;
      token.var.vval.v_number = temp;
    }
  }
  stack_push(stack, token);
}

static Token* pipe_type(Cmdline *cmdline, Token *word, Cmdstr *cmd)
{
  Token *nword = (Token*)utarray_next(cmdline->tokens, word);
  if (!nword) return word;
  char ch = nword->var.vval.v_string[0];
  if (ch == '>')
    (*cmd).pipet = PIPE_CNTLR;
  else
    (*cmd).pipet = PIPE_CMD;
  return nword;
}

static Token* cmdline_parse(Cmdline *cmdline, Token *word)
{
  log_msg("CMDLINE", "cmdline_parse");
  char ch;
  Cmdstr cmd = {.pipet = 0};

  QUEUE *stack = &cmd.stack;
  QUEUE_INIT(stack);

  cmd.args = list_init();
  stack_push(stack, cmd.args);

  while ((word = (Token*)utarray_next(cmdline->tokens, word))) {
    char *str = word->var.vval.v_string;
    switch(ch = str[0]) {
      case '|':
        word = pipe_type(cmdline, word, &cmd);
        goto breakout;
      case '"':
        break;
      case ',':
      case '}':
      case ']':
        /*FALLTHROUGH*/
        push(*word, stack);
        pop(stack);
        break;
        //
      case ':':
        push(pair_init(), stack);
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
  utarray_push_back(cmdline->cmds, &cmd);
  return NULL;
breakout:
  utarray_push_back(cmdline->cmds, &cmd);
  return word;
}

void cmdline_build(Cmdline *cmdline)
{
  log_msg("CMDSTR", "tokenize_line");

  cmdline_reset(cmdline);
  cmdline_tokenize(cmdline);

  /* parse until empty */
  Token *word = NULL;
  for(;;) {
    word = cmdline_parse(cmdline, word);
    if (!word) break;
  }
}

#define NEXT_CMD(cl,c) \
  (c = (Cmdstr*)utarray_next(cl->cmds, c))

void cmdline_req_run(Cmdline *cmdline)
{
  log_msg("CMDLINE", "cmdline_req_run");
  Cmdstr *cmd;
  cmd = NULL;

  while (NEXT_CMD(cmdline, cmd)) {
    cmd_run(cmd);
    if (cmd->pipet == PIPE_CNTLR) {
      Cntlr *lhs, *rhs;
      lhs = rhs = NULL;

      // search for rhs in next cmd.
      Cmdstr *tmp = cmd;
      NEXT_CMD(cmdline, tmp);

      List *args = TOKEN_LIST(tmp);
      Token *word = (Token*)utarray_front(args->items);
      String arg = TOKEN_STR(word->var);

      if (!cntlr_isloaded(arg))
        log_msg("ERROR", "cntlr %s not valid", arg);

      rhs = cntlr_open(arg, NULL);

      if (cmd->ret_t == CNTLR)
        lhs = cmd->ret;
      if (!lhs)
        lhs = focus_cntlr();

      if (lhs && rhs) {
        send_hook_msg("pipe_attach", rhs, lhs);
        cntlr_pipe(lhs);
        cmd = tmp;
      }
    }
  }
}
