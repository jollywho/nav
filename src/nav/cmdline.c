#include <malloc.h>
#include <string.h>
#include <ctype.h>

#include "nav/lib/queue.h"
#include "nav/plugins/plugin.h"
#include "nav/event/hook.h"
#include "nav/cmdline.h"
#include "nav/cmd.h"
#include "nav/ascii.h"
#include "nav/log.h"
#include "nav/macros.h"
#include "nav/util.h"

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

void cmdstr_copy(void *_dst, const void *_src)
{
  Cmdstr *dst = (Cmdstr*)_dst, *src = (Cmdstr*)_src;
  memcpy(dst, src, sizeof(Cmdstr));
  dst->chlds = src->chlds;
  dst->args = src->args;
  dst->ret = src->ret;
}

void cmdret_dtor(void *_elt)
{
  Cmdstr *elt = (Cmdstr*)_elt;
  if (elt->ret.type == STRING)
    free(elt->ret.val.v_str);
}

void cmdstr_dtor(void *_elt)
{
  Cmdstr *elt = (Cmdstr*)_elt;
  if (elt->ret.type == STRING)
    free(elt->ret.val.v_str);
  utarray_free(elt->chlds);
}

static UT_icd list_icd = { sizeof(Token),  NULL };
static UT_icd cmd_icd  = { sizeof(Cmdstr), NULL, NULL, cmdret_dtor };
static UT_icd chld_icd = { sizeof(Cmdstr), NULL, cmdstr_copy, cmdstr_dtor };

int str_num(const char *str, int *tmp)
{
  if (!str) return 0;
  return sscanf(str, "%d", tmp);
}

int str_tfmt(const char *str, char *fmt, void *tmp)
{
  return sscanf(str, fmt, tmp);
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
    case VAR_PAIR:
      return token->var.vval.v_pair;
    case VAR_STRING:
      return token->var.vval.v_string;
    default:
      return token;
  }
}

List* cmdline_lst(Cmdline *cmd)
{
  Cmdstr *root = (Cmdstr*)utarray_front(cmd->cmds);
  return token_val(&root->args, VAR_LIST);
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

void cmdline_cleanup(Cmdline *cmdline)
{
  if (!cmdline->cmds)
    return;
  Token *word = NULL;
  while ((word = (Token*)utarray_next(cmdline->tokens, word)))
    free(word->var.vval.v_string);
  while (!QUEUE_EMPTY(&cmdline->refs))
    ref_pop(&cmdline->refs);
  Cmdstr *root = (Cmdstr*)utarray_front(cmdline->cmds);
  utarray_free(root->chlds);
  utarray_free(cmdline->cmds);
  utarray_free(cmdline->tokens);
  utarray_free(cmdline->vars);
  free(cmdline->line);
}

Cmdstr* cmdline_getcmd(Cmdline *cmdline)
{
  return (Cmdstr*)utarray_front(cmdline->cmds);
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
  for (;;) {
    ch[0] = str[pos++];
    if (ch[0] == '\0') {
      cmdline_create_token(cmdline->tokens, str, st, ed, block);
      break;
    }
    else if (ch[0] == '\"' || ch [0] == '\'') {
      char *closech = strchr(&str[pos], ch[0]);
      if (closech) {
        int end = (closech - &str[pos]) + pos;
        cmdline_create_token(cmdline->tokens, str, pos, end, block);
        end++;
        pos = end;
        st = end;
      }
    }
    else if (strpbrk(ch, "~!/:|<>,[]{}() ")) {
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

#ifdef PIPES_SUPPORTED
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
#endif

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
  List *args = cmdline_lst(cmdline);
  return (Token*)utarray_eltptr(args->items, idx);
}

Token* cmdline_last(Cmdline *cmdline)
{
  if (!cmdline->tokens || utarray_len(cmdline->tokens) < 1)
    return NULL;
  Token *word = (Token*)utarray_back(cmdline->tokens);
  return word;
}

char* cmdline_line_from(Cmdline *cmdline, int idx)
{
  Token *word = cmdline_tokindex(cmdline, idx);
  if (!word)
    return NULL;
  if (word->start >= strlen(cmdline->line))
    return NULL;
  return &cmdline->line[word->start];
}

char* cmdline_line_after(Cmdline *cmdline, int idx)
{
  Token *word = cmdline_tokindex(cmdline, idx);
  if (!word || idx >= utarray_len(cmdline->tokens))
    return NULL;
  if (word->end >= strlen(cmdline->line))
    return NULL;
  return &cmdline->line[word->end];
}

static void pop(QUEUE *stack)
{
  Token token = stack_pop(stack);
  Token parent = stack_head(stack);

  if (parent.var.v_type == VAR_LIST) {
    utarray_push_back(parent.var.vval.v_list->items, &token);
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

  char *str = token_val(next, VAR_STRING);
  if (str && str[0] == ':') {
    push(pair_new(cmdline), stack);
    return true;
  }

  return false;
}

static Token* cmdline_parse(Cmdline *cmdline, Token *word, UT_array *parent)
{
  char ch;
  bool seek;
  Cmdstr cmd = {.flag = 0};
  if (word)
    cmd.st = word->start;

  QUEUE *stack = &cmd.stack;
  QUEUE_INIT(stack);

  cmd.args = list_new(cmdline);
  stack_push(stack, cmd.args);
  Token head = stack_head(stack);
  utarray_new(cmd.chlds, &chld_icd);

  check_if_exec(cmdline, &cmd, word);

  int idx = -1;
  while ((word = (Token*)utarray_next(cmdline->tokens, word))) {
    idx++;
    char *str = token_val(word, VAR_STRING);

    switch(ch = str[0]) {
#ifdef PIPES_SUPPORTED
      case '|':
        word = pipe_type(cmdline, word, &cmd);
        goto breakout;
#endif
      case '!':
        cmd.rev = 1;
        break;
      case ')':
        if (cmdline->lvl < 1)
          break;
        cmdline->lvl--;
        cmd.ed = word->start;
        goto breakout;
      case '(':
        cmdline->lvl++;
        word = cmdline_parse(cmdline, word, cmd.chlds);
        Cmdstr *pcmd = (Cmdstr*)utarray_back(cmd.chlds);
        pcmd->idx = --idx;
        if (!word)
          goto breakout;
        break;
      case '"':
        break;
      case ':':
        break;
      case ',':
        break;
      case ']':
        pop(stack);
        break;
      case '[':
        push(list_new(cmdline), stack);
        head.start = word->start;
        break;
      case '$':
        if (str[1]) {
          utarray_push_back(cmdline->vars, word);
          break;
        }
      default:
        /*FALLTHROUGH*/
        seek = seek_ahead(cmdline, stack, word);
        push(*word, stack);
        if (!seek)
          pop(stack);
    }
  }
breakout:
  while (!QUEUE_EMPTY(stack))
    stack_pop(stack);

  utarray_push_back(parent, &cmd);
  return word;
}

void cmdline_build(Cmdline *cmdline, char *line)
{
  log_msg("CMDSTR", "cmdline_build");
  cmdline->lvl = 0;
  cmdline->line = strdup(line);
  QUEUE_INIT(&cmdline->refs);
  utarray_new(cmdline->cmds,   &cmd_icd);
  utarray_new(cmdline->tokens, &list_icd);
  utarray_new(cmdline->vars,   &list_icd);

  cmdline_tokenize(cmdline);

  /* parse until empty */
  Token *word = NULL;
  for(;;) {
    word = cmdline_parse(cmdline, word, cmdline->cmds);
    if (!word)
      break;
  }
}

int cmdline_can_exec(Cmdstr *cmd, char *line)
{
  return !(!cmd->exec || !line || strlen(line) < 2);
}

#ifdef PIPES_SUPPORTED
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
  char *arg = list_arg(args, 0, VAR_STRING);

  if (!prev->ret) {
    prev->ret = focus_plugin();
    prev->ret_t = PLUGIN;
  }

  if (prev->ret_t == PLUGIN && cmd->ret_t == PLUGIN)
    return do_pipe(prev, cmd);

  cmd->ret = plugin_open(arg, NULL, cmdline->line);
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
#endif

void cmdline_req_run(Cmdstr *caller, Cmdline *cmdline)
{
  Cmdstr *cmd = NULL;
#ifdef PIPES_SUPPORTED
  Cmdstr *prev = NULL;
#endif

  if (!cmdline->cmds)
    return;

  while (NEXT_CMD(cmdline, cmd)) {
    if (cmd->flag & (PIPE_LEFT|PIPE_RIGHT))
      continue;

    cmd->caller = caller;
    cmd_run(cmd, cmdline);

#ifdef PIPES_SUPPORTED
    prev = (Cmdstr*)utarray_prev(cmdline->cmds, cmd);
    if (prev && (prev->flag & (PIPE_LEFT|PIPE_RIGHT)))
      exec_pipe(cmdline, cmd, prev);
#endif
  }
}
