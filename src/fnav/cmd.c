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

typedef struct Symb Symb;
struct Symb {
  enum CTLCMD type;
  char *line;
};

static void* cmd_ifblock();
static void* cmd_elseblock();
static void* cmd_endblock();

static Cmd_T *cmd_table;
static Symb *tape;
static int pos;
static int lvl;
static int maxpos;

static const Cmd_T builtins[] = {
  {NULL,      NULL,           0},
  {"if",      cmd_ifblock,    0},
  {"elseif",  cmd_elseblock,  0},
  {"else",    cmd_elseblock,  0},
  {"end",     cmd_endblock,   0},
};

static void stack_push(char *line)
{
  if (pos + 2 > maxpos) {
    maxpos *= 2;
    tape = realloc(tape, maxpos*sizeof(Symb));
    for (int i = pos+1; i < maxpos; i++)
      memset(&tape[i], 0, sizeof(Symb));
  }
  tape[++pos].line = strdup(line);
}

void cmd_reset()
{
  pos = -1;
  maxpos = BUFSIZ;
  tape = calloc(maxpos, sizeof(Symb));
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
  for (int i = 0; tape[i].line; i++)
    free(tape[i].line);
  free(tape);
}

void cmd_flush()
{
  log_msg("CMD", "flush");

  //TODO: force parse errors
  if (lvl > 0)
    log_msg("CMD", "parse error: open block not closed!");
  for (int i = 0; tape[i].line; i++)
    free(tape[i].line);
  free(tape);
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
  bool cond = true;
  bool stack[lvl];
  int curlvl = 0;
  for (int i = 0; i < pos; i++) {
    Symb *cur = &tape[i];
    log_msg("CMD", "cur %d %d", cur->type, cond);
    switch (cur->type) {
      case CTL_IF:
        curlvl++;
        cond = cond_do(cur->line);
        break;
      case CTL_ELSEIF:
        if (!cond)
          cond = cond_do(cur->line);
        break;
      case CTL_ELSE:
        cond = !cond;
        break;
      case CTL_END:
        curlvl--;
        cond = stack[curlvl];
        break;
      default:
        if (cond)
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
  if (lvl < 1 || ctl_cmd(line) != -1)
    cmd_do(line);
  else
    stack_push(line);
}

static void* cmd_ifblock(List *args, Cmdarg *ca)
{
  if (lvl == 0)
    cmd_flush();
  stack_push(ca->cmdline->line + strlen("if"));
  ++lvl;
  tape[pos].type = CTL_IF;
  return 0;
}

static void* cmd_elseblock(List *args, Cmdarg *ca)
{
  stack_push(ca->cmdline->line + strlen("else"));
  tape[pos].type = CTL_ELSE;
  return 0;
}

static void* cmd_endblock(List *args, Cmdarg *ca)
{
  stack_push("end");
  --lvl;
  tape[pos].type = CTL_END;
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
