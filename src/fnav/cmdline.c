#include <malloc.h>
#include <string.h>
#include <ctype.h>

#include "fnav/lib/queue.h"
#include "fnav/plugins/plugin.h"
#include "fnav/event/hook.h"
#include "fnav/event/shell.h"
#include "fnav/cmdline.h"
#include "fnav/model.h"
#include "fnav/cmd.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/macros.h"

typedef struct {
  Token item;
  QUEUE node;
} queue_item;

typedef struct {
  char v_type;
  void *ref;
} ref_item;

typedef struct {
  ref_item item;
  QUEUE node;
} queue_ref_item;

static void ref_pop(QUEUE *refs);

#define NEXT_CMD(cl,c) \
  (c = (Cmdstr*)utarray_next(cl->cmds, c))

static UT_icd dict_icd = { sizeof(Pair),   NULL };
static UT_icd list_icd = { sizeof(Token),  NULL };
static UT_icd cmd_icd  = { sizeof(Cmdstr), NULL };

int str_num(char *str, int *tmp)
{
  return sscanf(str, "%d", tmp);
}

void* token_val(Token *token, char v_type)
{
  if (!token)
    return NULL;

  char t_type = token->var.v_type;
  if (t_type != v_type)
    return NULL;

  switch (t_type) {
    case VAR_LIST:
      return token->var.vval.v_list;
    case VAR_DICT:
      return token->var.vval.v_dict;
    case VAR_PAIR:
      return token->var.vval.v_pair;
    case VAR_STRING:
      return token->var.vval.v_string;
    default:
      return NULL;
  }
}

void* list_arg(List *lst, int argc, char v_type)
{
  if (!lst || utarray_len(lst->items) < argc)
    return NULL;
  Token *word = (Token*)utarray_eltptr(lst->items, argc);
  return token_val(word, v_type);
}

Token* list_tokbtwn(List *lst, int st, int ed)
{
  Token *word = NULL;
  while ((word = (Token*)utarray_next(lst->items, word))) {
    if (MAX(0, MIN(ed, word->end) - MAX(st, word->start)) > 0)
      return word;
  }
  return NULL;
}

void* tok_arg(List *lst, int argc)
{
  if (!lst || utarray_len(lst->items) < argc)
    return NULL;
  Token *word = (Token*)utarray_eltptr(lst->items, argc);
  return word;
}

void cmdline_init_config(Cmdline *cmdline, char *line)
{
  cmdline->line = line;
  cmdline->cmds = NULL;
  cmdline->tokens = NULL;
  QUEUE_INIT(&cmdline->refs);
}

void cmdline_init(Cmdline *cmdline, int size)
{
  cmdline->line = malloc(size * sizeof(char*));
  memset(cmdline->line, '\0', size);
  cmdline->cmds = NULL;
  cmdline->tokens = NULL;
  QUEUE_INIT(&cmdline->refs);
}

void cmdline_cleanup(Cmdline *cmdline)
{
  log_msg("CMDLINE", "cmdline_cleanup");
  if (!cmdline->cmds)
    return;
  Token *word = NULL;
  while ((word = (Token*)utarray_next(cmdline->tokens, word)))
    free(word->var.vval.v_string);
  while (!QUEUE_EMPTY(&cmdline->refs))
    ref_pop(&cmdline->refs);
  utarray_clear(cmdline->cmds);
  utarray_clear(cmdline->tokens);
  utarray_free(cmdline->cmds);
  utarray_free(cmdline->tokens);
}

static void stack_push(QUEUE *queue, Token token)
{
  queue_item *item = malloc(sizeof(queue_item));
  item->item = token;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_HEAD(queue, &item->node);
}

static void ref_push(QUEUE *queue, void *ref, char v_type)
{
  queue_ref_item *item = malloc(sizeof(queue_ref_item));
  item->item.v_type = v_type;
  item->item.ref = ref;
  QUEUE_INIT(&item->node);
  QUEUE_INSERT_HEAD(queue, &item->node);
}

static void ref_pop(QUEUE *refs)
{
  QUEUE *h = QUEUE_HEAD(refs);
  queue_ref_item *item = QUEUE_DATA(h, queue_ref_item, node);
  QUEUE_REMOVE(&item->node);
  void *ref = item->item.ref;
  if (item->item.v_type == VAR_LIST) {
    List *l = ref;
    utarray_free(l->items);
  }
  if (item->item.v_type == VAR_DICT) {
    Dict *d = ref;
    utarray_free(d->items);
  }
  free(item);
  free(ref);
}

static queue_item *queue_node_data(QUEUE *queue)
{
  return QUEUE_DATA(queue, queue_item, node);
}

static Token stack_prevprev(QUEUE *stack)
{
  QUEUE *p = QUEUE_PREV(stack);
  p = QUEUE_PREV(p);
  queue_item *item = queue_node_data(p);
  return item->item;
}

static Token stack_pop(QUEUE *stack)
{
  QUEUE *h = QUEUE_HEAD(stack);
  queue_item *item = queue_node_data(h);
  QUEUE_REMOVE(&item->node);
  Token token = item->item;
  free(item);
  return token;
}

static Token stack_head(QUEUE *stack)
{
  QUEUE *h = QUEUE_HEAD(stack);
  queue_item *item = queue_node_data(h);
  Token token = item->item;
  return token;
}

static Token pair_new(Cmdline *cmdline)
{
  Token token;
  memset(&token, 0, sizeof(Token));
  Pair *p = malloc(sizeof(Pair));
  ref_push(&cmdline->refs, p, VAR_PAIR);
  token.var.v_type = VAR_PAIR;
  token.var.vval.v_pair = p;
  return token;
}

static Token list_new(Cmdline *cmdline)
{
  Token token;
  memset(&token, 0, sizeof(Token));
  List *l = malloc(sizeof(List));
  ref_push(&cmdline->refs, l, VAR_LIST);
  token.var.v_type = VAR_LIST;
  utarray_new(l->items, &list_icd);
  token.var.vval.v_list = l;
  return token;
}

static Token dict_new(Cmdline *cmdline)
{
  Token token;
  memset(&token, 0, sizeof(Token));
  Dict *d = malloc(sizeof(Dict));
  ref_push(&cmdline->refs, d, VAR_DICT);
  token.var.v_type = VAR_DICT;
  utarray_new(d->items, &dict_icd);
  token.var.vval.v_dict = d;
  return token;
}

static void cmdline_create_token(UT_array *ar, char *str, int st, int ed, int b)
{
  int len = ed - st;
  if (len < 1)
    return;
  char *vstr = malloc(sizeof(char*)*len);
  strncpy(vstr, &str[st], len);
  vstr[len] = '\0';
  Token token = {
    .start = st,
    .end = ed,
    .block = b,
    .var = {
      .v_type = VAR_STRING,
      .vval.v_string = vstr
    }
  };
  utarray_push_back(ar, &token);
}

static void cmdline_tokenize(Cmdline *cmdline)
{
  char ch[] = {0,'\0'};
  int st, ed, pos;
  char *str = cmdline->line;
  int block = 0;

  st = ed = pos = 0;
  for(;;) {
    ch[0] = str[pos++];
    if (ch[0] == '\0') {
      cmdline_create_token(cmdline->tokens, str, st, ed, block);
      break;
    }
    else if (ch[0] == '"') {
      char *closech = strchr(&str[pos], '"');
      if (closech) {
        int end = (closech - &str[pos]) + pos;
        cmdline_create_token(cmdline->tokens, str, pos, end, block);
        end++;
        pos = end;
        st = end;
      }
    }
    else if (strpbrk(ch, "!/:|<>,[]{} ")) {
      cmdline_create_token(cmdline->tokens, str, st, ed, block);
      if (*ch == ' ')
        block++;
      else {
        cmdline_create_token(cmdline->tokens, str, pos-1, pos, block);
        ed = pos;
      }
      st = pos;
    }
    else
      ed = pos;
  }
}

static Token* pipe_type(Cmdline *cmdline, Token *word, Cmdstr *cmd)
{
  Token *nword = (Token*)utarray_next(cmdline->tokens, word);
  Token *bword = (Token*)utarray_prev(cmdline->tokens, word);
  if (nword) {
    char n_ch = nword->var.vval.v_string[0];
    if (n_ch == '>') {
      (*cmd).flag = PIPE_RIGHT;
      return nword;
    }
  }
  if (bword) {
    char b_ch = bword->var.vval.v_string[0];
    if (b_ch == '<') {
      Token parent = stack_head(&cmd->stack);
      List *l = token_val(&parent, VAR_LIST);
      utarray_pop_back(l->items);
      (*cmd).flag = PIPE_LEFT;
      return word;
    }
  }
  (*cmd).flag = PIPE;
  return word;
}

void check_if_exec(Cmdline *cmdline, Cmdstr *cmd, Token *word)
{
  if (word)
    word = (Token*)utarray_next(cmdline->tokens, word);
  else
    word = (Token*)utarray_front(cmdline->tokens);
  if (!word)
    return;

  char *str = token_val(word, VAR_STRING);
  if (str[0] == '!')
    cmd->exec = 1;
}

Token* cmdline_tokbtwn(Cmdline *cmdline, int st, int ed)
{
  Token *word = NULL;
  while ((word = (Token*)utarray_next(cmdline->tokens, word))) {
    if (MAX(0, MIN(ed, word->end) - MAX(st, word->start)) > 0)
      return word;
  }
  return NULL;
}

Cmdstr* cmdline_cmdbtwn(Cmdline *cmdline, int st, int ed)
{
  Cmdstr *cmd = NULL;
  for (int i = 0; i < utarray_len(cmdline->cmds); i++) {
    NEXT_CMD(cmdline, cmd);
    List *list = token_val(&cmd->args, VAR_LIST);
    Token *word = (Token*)utarray_back(list->items);
    if (!word)
      continue;
    if (MAX(0, MIN(ed, word->end) - MAX(st, word->start)) > 0)
      break;
  }
  return cmd;
}

Token* cmdline_tokindex(Cmdline *cmdline, int idx)
{
  if (!cmdline->cmds)
    return NULL;
  Cmdstr *cmd = NULL;
  NEXT_CMD(cmdline, cmd);

  List *list = cmd->args.var.vval.v_list;
  UT_array *arr = list->items;
  Token *word = NULL;

  word = (Token*)utarray_eltptr(arr, idx);
  return word;
}

Token* cmdline_last(Cmdline *cmdline)
{
  if (!cmdline->tokens || utarray_len(cmdline->tokens) < 1)
    return NULL;
  Token *word = (Token*)utarray_back(cmdline->tokens);
  return word;
}

char * cmdline_line_from(Cmdline *cmdline, int idx)
{
  Token *word = cmdline_tokindex(cmdline, idx);
  if (!word)
    return NULL;
  return &cmdline->line[word->start];
}

static void pop(QUEUE *stack)
{
  Token token = stack_pop(stack);
  Token parent = stack_head(stack);

  if (parent.var.v_type == VAR_LIST) {
    utarray_push_back(parent.var.vval.v_list->items, &token);
    parent.end = token.end;
  }
  else if (parent.var.v_type == VAR_DICT) {
    utarray_push_back(parent.var.vval.v_dict->items, &token);
    parent.end = token.end;
  }
  else if (stack_prevprev(stack).var.v_type == VAR_PAIR) {
    Token key = stack_pop(stack);
    Token p = stack_prevprev(stack);
    p.var.vval.v_pair->key = &key;
    p.var.vval.v_pair->value = &token;

    stack_pop(stack);
    Token pt = stack_head(stack);
    utarray_push_back(pt.var.vval.v_list->items, &p);
  }
}

static void push(Token token, QUEUE *stack)
{
  stack_push(stack, token);
}

static bool seek_ahead(Cmdline *cmdline, QUEUE *stack, Token *token)
{
  Token *next = (Token*)utarray_next(cmdline->tokens, token);
  if (!next)
    return false;

  char * str = token_val(next, VAR_STRING);
  if (str && str[0] == ':') {
    push(pair_new(cmdline), stack);
    return true;
  }

  return false;
}

static Token* cmdline_parse(Cmdline *cmdline, Token *word)
{
  log_msg("CMDLINE", "cmdline_parse");
  char ch;
  bool seek;
  Cmdstr cmd = {.flag = 0};

  QUEUE *stack = &cmd.stack;
  QUEUE_INIT(stack);

  cmd.args = list_new(cmdline);
  stack_push(stack, cmd.args);
  Token head = stack_head(stack);

  check_if_exec(cmdline, &cmd, word);

  while ((word = (Token*)utarray_next(cmdline->tokens, word))) {
    char *str = token_val(word, VAR_STRING);

    switch(ch = str[0]) {
      case '|':
        word = pipe_type(cmdline, word, &cmd);
        goto breakout;
      case '!':
        cmd.rev = 1;
        break;
      case '"':
        break;
      case ':':
        break;
      case ',':
        push(*word, stack);
      case '}':
      case ']':
        /*FALLTHROUGH*/
        pop(stack);
        break;
        //
      case '{':
        push(dict_new(cmdline), stack);
        head.start = word->start;
        break;
      case '[':
        push(list_new(cmdline), stack);
        head.start = word->start;
        break;
      default:
        seek = seek_ahead(cmdline, stack, word);
        push(*word, stack);
        if (!seek)
          pop(stack);
    }
  }
breakout:
  while (!QUEUE_EMPTY(stack))
    stack_pop(stack);

  utarray_push_back(cmdline->cmds, &cmd);
  return word;
}

void cmdline_build(Cmdline *cmdline)
{
  log_msg("CMDSTR", "cmdline_build");

  cmdline_cleanup(cmdline);
  utarray_new(cmdline->cmds, &cmd_icd);
  utarray_new(cmdline->tokens, &list_icd);

  cmdline_tokenize(cmdline);

  /* parse until empty */
  Token *word = NULL;
  for(;;) {
    word = cmdline_parse(cmdline, word);
    if (!word) break;
  }
}

static char * do_expansion(char * line)
{
  log_msg("CMDLINE", "do_expansion");
  if (!strstr(line, "%:"))
    return strdup(line);

  char * head = strtok(line, "%:");
  char * name = strtok(NULL, "%:");

  char *quote = "\"";
  char *delim = strchr(name, '"');
  if (!delim) {
    delim = " ";
    quote = "";
  }

  name = strtok(name, delim);
  char * tail = strtok(NULL, delim);

  char * body = model_str_expansion(name);
  if (!body)
    return strdup(line);

  if (!tail)
    tail = "";

  char * out;
  asprintf(&out, "%s%s%s%s", head, body, quote, tail);
  return out;
}

static int exec_line(char * line, Cmdstr *cmd)
{
  if (!cmd->exec || strlen(line) < 2)
    return 0;
  char * str = strstr(line, "!");
  ++str;
  str = do_expansion(str);
  shell_exec(str, NULL, focus_dir(), NULL);
  free(str);
  return 1;
}

static void do_pipe(Cmdstr *lhs, Cmdstr *rhs)
{
  log_msg("CMDLINE", "do_pipe");
  if ((lhs)->flag == PIPE_LEFT)
    send_hook_msg("pipe_left", lhs->ret, rhs->ret, NULL);
  if ((lhs)->flag == PIPE_RIGHT)
    send_hook_msg("pipe_right", rhs->ret, lhs->ret, NULL);
}

static void exec_pipe(Cmdline *cmdline, Cmdstr *cmd, Cmdstr *prev)
{
  log_msg("CMDLINE", "exec_pipe");
  List *args = token_val(&cmd->args, VAR_LIST);
  char * arg = list_arg(args, 0, VAR_STRING);

  if (!prev->ret) {
    prev->ret = focus_plugin();
    prev->ret_t = PLUGIN;
  }

  if (prev->ret_t == PLUGIN && cmd->ret_t == PLUGIN)
    return do_pipe(prev, cmd);

  cmd->ret = plugin_open(arg, NULL, args);
  log_msg("CMDLINE", "%p %d %p %d",
      prev->ret,
      prev->ret_t,
      cmd->ret,
      cmd->ret_t);

  if (cmd->ret) {
    cmd->ret_t = PLUGIN;
    return do_pipe(prev, cmd);
  }

  int wnum;
  if (!str_num(arg, &wnum))
    return;

  cmd->ret = plugin_from_id(wnum);
  if (cmd->ret) {
    cmd->ret_t = PLUGIN;
    return do_pipe(prev, cmd);
  }
}

void cmdline_req_run(Cmdline *cmdline)
{
  log_msg("CMDLINE", "cmdline_req_run");
  Cmdstr *cmd = NULL;
  Cmdstr *prev = NULL;
  if (!cmdline->cmds)
    return;

  while (NEXT_CMD(cmdline, cmd)) {
    List *list = token_val(&cmd->args, VAR_LIST);
    if (utarray_len(list->items) < 1)
      continue;
    if (exec_line(cmdline->line, cmd))
      continue;

    cmd_run(cmd, cmdline);

    if (prev && (prev->flag & (PIPE_LEFT|PIPE_RIGHT)))
      exec_pipe(cmdline, cmd, prev);

    prev = cmd;
  }
}
