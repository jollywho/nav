#include <malloc.h>
#include <string.h>
#include <ctype.h>

#include "fnav/lib/queue.h"
#include "fnav/tui/cntlr.h"
#include "fnav/event/hook.h"
#include "fnav/event/shell.h"
#include "fnav/cmdline.h"
#include "fnav/model.h"
#include "fnav/cmd.h"
#include "fnav/ascii.h"
#include "fnav/log.h"

static void cmdline_reset(Cmdline *cmdline);
static UT_icd dict_icd = { sizeof(Pair),   NULL };
static UT_icd list_icd = { sizeof(Token),  NULL };
static UT_icd cmd_icd  = { sizeof(Cmdstr), NULL };

typedef struct {
  Token *item;
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

static void stack_push(QUEUE *queue, Token *token)
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

static Token* stack_prevprev(QUEUE *stack)
{
  QUEUE *p = QUEUE_PREV(stack);
  p = QUEUE_PREV(p);
  queue_item *item = queue_node_data(p);
  return item->item;
}

static Token* stack_pop(QUEUE *stack)
{
  log_msg("CMDLINE", "stack_pop");
  QUEUE *h = QUEUE_HEAD(stack);
  queue_item *item = queue_node_data(h);
  QUEUE_REMOVE(&item->node);
  Token *token = item->item;
  free(item);
  return token;
}

static Token* stack_head(QUEUE *stack)
{
  QUEUE *h = QUEUE_HEAD(stack);
  queue_item *item = queue_node_data(h);
  Token *token = item->item;
  return token;
}

static Token* pair_init()
{
  Token* token = malloc(sizeof(Token));
  Pair *p = malloc(sizeof(Pair));
  token->var.v_type = VAR_PAIR;
  token->var.vval.v_pair = p;
  return token;
}

static Token* list_init()
{
  Token* token = malloc(sizeof(Token));
  List *l = malloc(sizeof(List));
  token->var.v_type = VAR_LIST;
  utarray_new(l->items, &list_icd);
  token->var.vval.v_list = l;
  return token;
}

static Token* dict_init()
{
  Token* token = malloc(sizeof(Token));
  Dict *d = malloc(sizeof(Dict));
  token->var.v_type = VAR_DICT;
  utarray_new(d->items, &dict_icd);
  token->var.vval.v_dict = d;
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
      char *closech = strchr(&str[pos], '"');
      if (closech) {
        int end = (closech - &str[pos]) + pos;
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
  Token *token = stack_pop(stack);
  Token *parent = stack_head(stack);
  log_msg("CMDLINE", "pop %d", token->var.v_type);

  if (parent->var.v_type == VAR_LIST) {
    utarray_push_back(parent->var.vval.v_list->items, token);
  }
  else if (parent->var.v_type == VAR_DICT) {
    utarray_push_back(parent->var.vval.v_dict->items, token);
  }
  else if (stack_prevprev(stack)->var.v_type == VAR_PAIR) {
    Token *key = stack_pop(stack);
    Token *p = stack_prevprev(stack);
    log_msg("CMDLINE", "%s:%s", TOKEN_STR(key->var), TOKEN_STR(token->var));
    p->var.vval.v_pair->key = key;
    p->var.vval.v_pair->value = token;

    stack_pop(stack);
    Token *pt = stack_head(stack);
    utarray_push_back(pt->var.vval.v_list->items, p);
  }
}

static void push(Token *token, QUEUE *stack)
{
  log_msg("CMDLINE", "push %d", token->var.v_type);
  if (token->var.v_type == VAR_STRING) {
    int temp;
    if (sscanf(token->var.vval.v_string, "%d", &temp)) {
      token->var.v_type = VAR_NUMBER;
      token->var.vval.v_number = temp;
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

static bool seek_ahead(Cmdline *cmdline, QUEUE *stack, Token *token)
{
  Token *next = (Token*)utarray_next(cmdline->tokens, token);
  if (next) {
    if (TOKEN_STR(next->var)[0] == ':') {
      push(pair_init(), stack);
      return true;
    }
  }
  return false;
}

static Token* cmdline_parse(Cmdline *cmdline, Token *word)
{
  log_msg("CMDLINE", "cmdline_parse");
  char ch;
  bool seek;
  Cmdstr cmd = {.pipet = 0};

  QUEUE *stack = &cmd.stack;
  QUEUE_INIT(stack);

  cmd.args = list_init();
  stack_push(stack, cmd.args);

  while ((word = (Token*)utarray_next(cmdline->tokens, word))) {
    char *str = TOKEN_STR(word->var);
    if (str[0] == '!')
      cmd.exec = 1;

    switch(ch = str[0]) {
      case '|':
        word = pipe_type(cmdline, word, &cmd);
        goto breakout;
      case '"':
        break;
      case ':':
        break;
      case ',':
      case '}':
      case ']':
        /*FALLTHROUGH*/
        push(word, stack);
        pop(stack);
        break;
        //
      case '{':
        push(dict_init(), stack);
        break;
      case '[':
        push(list_init(), stack);
        break;
      default:
        seek = seek_ahead(cmdline, stack, word);
        push(word, stack);
        if (!seek)
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
  log_msg("CMDSTR", "cmdline_build");

  cmdline_reset(cmdline);

  cmdline_tokenize(cmdline);

  /* parse until empty */
  Token *word = NULL;
  for(;;) {
    word = cmdline_parse(cmdline, word);
    if (!word) break;
  }
}

static String do_expansion(String line)
{
  log_msg("CMDLINE", "do_expansion");
  String head = strtok(line, "%:");
  String name = strtok(NULL, "%:");

  char *quote = "\"";
  char *delim = strchr(name, '"');
  if (!delim) {
    delim = " ";
    quote = "";
  }

  name = strtok(name, delim);
  String tail = strtok(NULL, delim);

  String body = model_str_expansion(name);
  if (!body) { return NULL; }

  if (!tail) { tail = "";   }

  String out;
  asprintf(&out, "%s%s%s%s", head, body, quote, tail);
  return out;
}

#define NEXT_CMD(cl,c) \
  (c = (Cmdstr*)utarray_next(cl->cmds, c))

void cmdline_req_run(Cmdline *cmdline)
{
  log_msg("CMDLINE", "cmdline_req_run");
  Cmdstr *cmd;
  cmd = NULL;

  while (NEXT_CMD(cmdline, cmd)) {

    //TODO cleanup this area
    if (cmd->exec) {
      String ret = strstr(cmdline->line, "%:");
      if (ret) {
        ret = do_expansion(cmdline->line);
        if (ret) {
          shell_exec(ret);
          free(ret);
        }
        else
          shell_exec(cmdline->line);
      }
      else
        shell_exec(cmdline->line);
      continue;
    }

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
