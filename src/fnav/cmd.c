#include "fnav/lib/queue.h"
#include "fnav/cmd.h"
#include "fnav/log.h"
#include "fnav/compl.h"
#include "fnav/table.h"

enum CTLCMD {
  CTL_IF,
  CTL_ELSEIF,
  CTL_ELSE,
  CTL_END,
};

typedef struct Expr Expr;
struct Expr {
  Expr *parent;
  QUEUE childs;
  char *line;
};

typedef struct {
  Expr *expr;
  QUEUE node;
} cmd_item;

static void* cmd_ifblock();
static void* cmd_elseblock();
static void* cmd_endblock();

static Cmd_T *cmd_table;
static Expr *block;

static const Cmd_T builtins[] = {
  {"if",      cmd_ifblock,   0},
  {"elseif",  cmd_elseblock, 0},
  {"else",    cmd_elseblock, 0},
  {"end",     cmd_endblock,  0},
};

static void stack_new(char *line)
{
  Expr *expr = malloc(sizeof(Expr));
  log_msg("CMD", "new %s", line);
  expr->line = strdup(line);
  QUEUE_INIT(&expr->childs);
  if (block) {
    cmd_item *item = malloc(sizeof(cmd_item));
    item->expr = expr;
    QUEUE_INSERT_HEAD(&block->childs, &item->node);
  }
  block = expr;
}

static void stack_push(char *line)
{
  cmd_item *item = malloc(sizeof(cmd_item));
  item->expr = malloc(sizeof(Expr));
  log_msg("CMD", "push %s", line);
  item->expr->line = strdup(line);
  QUEUE_INSERT_HEAD(&block->childs, &item->node);
}

static Expr* stack_pop()
{
  QUEUE *h = QUEUE_HEAD(&block->childs);
  cmd_item *item = QUEUE_DATA(h, cmd_item, node);
  QUEUE_REMOVE(&item->node);
  Expr *expr = item->expr;
  free(item);
  return expr;
}

void cmd_init()
{
  for (int i = 0; i < LENGTH(builtins); i++) {
    Cmd_T *cmd = malloc(sizeof(Cmd_T));
    cmd = memmove(cmd, &builtins[i], sizeof(Cmd_T));
    cmd_add(cmd);
  }
}

static void cmd_do(char *line)
{
  log_msg("CMD", "cmd_do");
  Cmdline cmd;
  cmdline_init_config(&cmd, line);
  cmdline_build(&cmd);
  cmdline_req_run(&cmd);
  cmdline_cleanup(&cmd);
}

static void start_do()
{
  log_msg("CMD", "start_do");
  // run item
  // if ret
}

static enum CTLCMD ctl_cmd(const char *line)
{
  for (int i = 0; i < LENGTH(builtins); i++) {
    char *str = builtins[i].name;
    if (!strncmp(str, line, strlen(str)))
      return i;
  }
  return -1;
}

void cmd_eval(char *line)
{
  enum CTLCMD ctl = ctl_cmd(line);
  switch (ctl) {
    case CTL_IF:
      stack_new(line);
      break;
      // elseif
      // add to head->parent->childs
    case CTL_END:
      if (!block->parent)
        start_do();
      else
        block = block->parent;
      break;
    default:
      if (block)
        stack_push(line);
      else
        cmd_do(line);
  }
}

static void* cmd_ifblock(List *args, Cmdarg *ca)
{
  log_msg("CMD", "cmd_ifblock");

  if (ca->flags || ca->cmdstr->ret) {
  }
  return 0;
}

static void* cmd_elseblock()
{
  log_msg("CMD", "cmd_elseblock");
  return 0;
}

static void* cmd_endblock()
{
  log_msg("CMD", "cmd_endblock");
  return 0;
}

void cmd_clearall()
{
  log_msg("CMD", "cmd_clearall");
  Cmd_T *it, *tmp;
  HASH_ITER(hh, cmd_table, it, tmp) {
    HASH_DEL(cmd_table, it);
    free(it);
  }
}

int name_sort(Cmd_T *a, Cmd_T *b)
{
  return strcmp(a->name, b->name);
}

void cmd_add(Cmd_T *cmd)
{
  HASH_ADD_STR(cmd_table, name, cmd);
  HASH_SORT(cmd_table, name_sort);
}

void cmd_remove(const char *name)
{
}

Cmd_T* cmd_find(const char *name)
{
  Cmd_T *cmd;
  if (!name)
    return NULL;

  HASH_FIND_STR(cmd_table, name, cmd);
  return cmd;
}

void cmd_run(Cmdstr *cmdstr, Cmdline *cmdline)
{
  log_msg("CMD", "cmd_run");
  List *args = token_val(&cmdstr->args, VAR_LIST);
  if (utarray_len(args->items) < 1)
    return;

  Cmd_T *fun = cmd_find(list_arg(args, 0, VAR_STRING));
  if (!fun) {
    cmdstr->ret_t = 0;
    cmdstr->ret = NULL;
    return;
  }

  cmdstr->ret_t = PLUGIN;
  Cmdarg flags = {fun->flags, 0, cmdstr, cmdline};
  cmdstr->ret = fun->cmd_func(args, &flags);
}

void cmd_list(List *args)
{
  log_msg("CMD", "compl cmd_list");
  int i = 0;
  Cmd_T *it;
  compl_new(HASH_COUNT(cmd_table), COMPL_STATIC);
  for (it = cmd_table; it != NULL; it = it->hh.next) {
    compl_set_index(i, 0, NULL, "%s", it->name);
    i++;
  }
}
