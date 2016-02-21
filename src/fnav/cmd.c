#include "fnav/cmd.h"
#include "fnav/log.h"
#include "fnav/compl.h"
#include "fnav/table.h"

enum CTLCMD {
  CTL_NOP,
  CTL_IF,
  CTL_ELSEIF,
  CTL_ELSE,
  CTL_END,
};

typedef struct Expr Expr;
struct Expr {
  int size;
  Expr *parent;
  char *line;
};

static void* cmd_ifblock();
static void* cmd_elseblock();
static void* cmd_endblock();

static Cmd_T *cmd_table;
static Expr *root;
static Expr *block;
static int pos;

static const Cmd_T builtins[] = {
  {NULL,      NULL,          0},
  {"if",      cmd_ifblock,   0},
  {"elseif",  cmd_elseblock, 0},
  {"else",    cmd_elseblock, 0},
  {"end",     cmd_endblock,  0},
};

static void stack_push(char *line)
{
  root[++pos].line = strdup(line);
  block->size++;
}

static void stack_new(char *line)
{
  log_msg("CMD", "new| %s", line);
  Expr *prev = block;
  if (prev != root)
    prev->size++;
  block = &root[pos+1];
  block->parent = prev;
  root[++pos].line = strdup(line);
}

void cmd_init()
{
  for (int i = 1; i < LENGTH(builtins); i++) {
    Cmd_T *cmd = malloc(sizeof(Cmd_T));
    cmd = memmove(cmd, &builtins[i], sizeof(Cmd_T));
    cmd_add(cmd);
  }
}

void cmd_start()
{
  pos = -1;
  root = calloc(50, sizeof(Expr));
  block = &root[0];
  block->size++;
}

void cmd_end()
{
  //TODO: force parse errors
  for (int i = 0; i < pos + 1; i++) {
    free(root[i].line);
  }
  free(root);
  block = NULL;
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

static enum CTLCMD ctl_cmd(const char *line)
{
  for (int i = 1; i < LENGTH(builtins); i++) {
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
      if (block == root) {
        cmd_do(block->line);
      }
      else
        block = block->parent;
      break;
    default:
      if (pos > 0)
        stack_push(line);
      else
        cmd_do(line);
  }
}

static void* cmd_ifblock(List *args, Cmdarg *ca)
{
  log_msg("CMD", "cmd_ifblock");
  //TODO: make ret a arg struct
  Expr *node = block;
  char *line = ca->cmdline->line + 3;
  Cmdline cmd;
  cmdline_init_config(&cmd, line);
  cmdline_build(&cmd);
  cmdline_req_run(&cmd);
  void *ret = cmdline_getcmd(&cmd)->ret;
  log_msg("CMD", ":-:| %s %d", node->line, node->size);

  if (ret) {
    int ofs = 1;
    for (int i = 1; i < node->size + 1; i++) {
      Expr *expr = &node[ofs];
      ofs += expr->size + 1;
      log_msg("CMD", ":| %s ", expr->line);
      block = expr;
      cmd_do(expr->line);
    }
  }
  log_msg("CMD", "cmd_endofblock");
  cmdline_cleanup(&cmd);
  block = node->parent;
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

  char *word = list_arg(args, 0, VAR_STRING);
  Cmd_T *fun = cmd_find(word);
  if (!fun) {
    cmdstr->ret_t = WORD;
    cmdstr->ret = word;
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
