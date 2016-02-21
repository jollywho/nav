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
  enum CTLCMD type;
  char *line;
};

#define DEFAULT_SIZE 20

static void* cmd_ifhead();
static void* cmd_elsehead();
static void* cmd_endhead();

static Cmd_T *cmd_table;
static Expr *root;
static int pos;
static int lvl;

static const Cmd_T builtins[] = {
  {NULL,      NULL,         0},
  {"if",      cmd_ifhead,   0},
  {"elseif",  cmd_elsehead, 0},
  {"else",    cmd_elsehead, 0},
  {"end",     cmd_endhead,  0},
};

static void stack_push(char *line)
{
  root[++pos].line = strdup(line);
}

void cmd_reset()
{
  pos = -1;
  root = calloc(DEFAULT_SIZE, sizeof(Expr));
}

void cmd_init()
{
  cmd_reset();
  for (int i = 1; i < LENGTH(builtins); i++) {
    Cmd_T *cmd = malloc(sizeof(Cmd_T));
    cmd = memmove(cmd, &builtins[i], sizeof(Cmd_T));
    cmd_add(cmd);
  }
}

void cmd_cleanup()
{
  for (int i = 0; root[i].line; i++)
    free(root[i].line);
  free(root);
}

void cmd_flush()
{
  log_msg("CMD", "flush");
  if (lvl > 0)
    log_msg("CMD", "parse error: open block not closed!");
  //TODO: force parse errors
  for (int i = 0; root[i].line; i++)
    free(root[i].line);
  free(root);
  cmd_reset();
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

static bool cond_do(char *line)
{
  bool cond = false;
  Cmdline cmd;
  cmdline_init_config(&cmd, line);
  cmdline_build(&cmd);
  cmdline_req_run(&cmd);
  cond = cmdline_getcmd(&cmd)->ret ? true : false;
  cmdline_cleanup(&cmd);
  return cond;
}

//TODO: make ret a arg struct
//TODO: handle bracketed expr
static void cmd_start()
{
  bool skip = 0;
  for (int i = 0; i < pos; i++) {
    Expr *cur = &root[i];
    log_msg("CMD", "cur %d %d", cur->type, skip);
    switch (cur->type) {
      case CTL_IF:
        skip = !cond_do(cur->line);
        break;
      case CTL_ELSEIF:
        if (!skip) {
          skip = true;
          continue;
        }
        skip = !cond_do(cur->line);
        break;
      case CTL_ELSE:
        if (!skip) {
          skip = true;
          continue;
        }
        break;
      case CTL_END:
        skip = false;
        break;
      default:
        if (!skip)
          cmd_do(cur->line);
    }
  }
}

static int ctl_cmd(const char *line)
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
  log_msg("CMD", "%d %d", lvl, ctl_cmd(line));
  if (lvl < 1 || ctl_cmd(line) != -1)
    cmd_do(line);
  else
    stack_push(line);
}

static void* cmd_ifhead(List *args, Cmdarg *ca)
{
  log_msg("CMD", "cmd_ifhead");
  if (lvl == 0)
    cmd_flush();
  stack_push(ca->cmdline->line + strlen("if"));
  ++lvl;
  root[pos].type = CTL_IF;
  return 0;
}

static void* cmd_elsehead(List *args, Cmdarg *ca)
{
  log_msg("CMD", "cmd_elsehead");
  stack_push(ca->cmdline->line + strlen("else"));
  root[pos].type = CTL_ELSE;
  return 0;
}

static void* cmd_endhead(List *args, Cmdarg *ca)
{
  log_msg("CMD", "cmd_endhead");
  stack_push("end");
  --lvl;
  root[pos].type = CTL_END;
  if (lvl < 1)
    cmd_start();
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
