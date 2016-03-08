#include <sys/queue.h>
#include "fnav/cmd.h"
#include "fnav/log.h"
#include "fnav/compl.h"
#include "fnav/table.h"
#include "fnav/option.h"

enum CTLCMD { CTL_NOP, CTL_IF, CTL_ELSEIF, CTL_ELSE, CTL_END, CTL_FUNC, };

typedef struct Symb Symb;
struct Symb {
  enum CTLCMD type;
  STAILQ_HEAD(Childs, Symb) childs;
  STAILQ_ENTRY(Symb) ent;
  int st;
  Symb *parent;
  Symb *end;
  char *line;
};

static void* cmd_ifblock();
static void* cmd_elseifblock();
static void* cmd_elseblock();
static void* cmd_endblock();
static void* cmd_funcblock();

static Cmd_T *cmd_table;
static Symb *tape;
static Symb *cur;
static Symb root;
static int pos;
static int lvl;
static int maxpos;
static char *lncont;
static int lvlcont;
static int fndefopen;
static int parse_error;
static fn_func fndef;

static const Cmd_T builtins[] = {
  {NULL,          NULL,               0},
  {"if",          cmd_ifblock,        0},
  {"else",        cmd_elseblock,      0},
  {"elseif",      cmd_elseifblock,    0},
  {"end",         cmd_endblock,       0},
  {"function",    cmd_funcblock,      0},
};

static void stack_push(char *line)
{
  if (fndef.key) {
    log_msg("CMD", "push : %s", line);
    utarray_push_back(fndef.lines, &line);
    return;
  }
  if (pos + 2 > maxpos) {
    maxpos *= 2;
    tape = realloc(tape, maxpos*sizeof(Symb));
    for (int i = pos+1; i < maxpos; i++)
      memset(&tape[i], 0, sizeof(Symb));
  }
  tape[++pos].line = strdup(line);
  tape[pos].parent = cur;
  STAILQ_INIT(&tape[pos].childs);
  STAILQ_INSERT_TAIL(&cur->childs, &tape[pos], ent);
}

void cmd_reset()
{
  lncont = NULL;
  fndef.key = NULL;
  lvlcont = 0;
  fndefopen = 0;
  parse_error = 0;
  lvl = 0;
  pos = -1;
  maxpos = BUFSIZ;
  tape = calloc(maxpos, sizeof(Symb));
  cur = &root;
  cur->parent = &root;
  STAILQ_INIT(&cur->childs);
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
  //TODO: error open
  if (fndefopen) {
    log_err("CMD", "parse error: open definition not closed!");
    utarray_free(fndef.lines);
  }
  if (lvl > 0)
    log_err("CMD", "parse error: open block not closed!");
  if (lvlcont > 0)
    log_err("CMD", "parse error: open '(' not closed!");
  for (int i = 0; tape[i].line; i++)
    free(tape[i].line);

  if (fndef.key)
    free(fndef.key);
  free(tape);
  free(lncont);
  cmd_reset();
}

static void cmd_do(char *line)
{
  if (lvlcont > 0) {
    char *str;
    asprintf(&str, "%s%s", lncont, line);
    SWAP_ALLOC_PTR(lncont, str);
    line = lncont;
  }

  Cmdline cmd;
  cmdline_build(&cmd, line);
  lvlcont = cmd.lvl;

  if (lvlcont == 0)
    cmdline_req_run(&cmd);
  else
    SWAP_ALLOC_PTR(lncont, strdup(line));

  cmdline_cleanup(&cmd);
}

static int cond_do(char *line)
{
  int cond = 0;
  Cmdline cmd;
  cmdline_build(&cmd, line);
  cmdline_req_run(&cmd);
  cond = cmdline_getcmd(&cmd)->ret ? 1 : 0;
  cmdline_cleanup(&cmd);
  return cond;
}

static Symb* cmd_next(Symb *node)
{
  Symb *n = STAILQ_NEXT(node, ent);
  if (!n)
    n = node->parent->end;
  return n;
}

//TODO: make ret a arg struct
//TODO: handle bracketed expr
static void cmd_start()
{
  Symb *it = STAILQ_FIRST(&root.childs);
  int cond;
  while (it) {
    switch (it->type) {
      case CTL_IF:
      case CTL_ELSEIF:
        cond = cond_do(it->line);
        if (cond)
          it = STAILQ_FIRST(&it->childs);
        else
          it = cmd_next(it);
        break;
      case CTL_ELSE:
        it = STAILQ_FIRST(&it->childs);
        break;
      case CTL_END:
        if (it->parent == &root)
          it = NULL;
        else
          it++;
        break;
      default:
        cmd_do(it->line);
        it = cmd_next(it);
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
  if (parse_error)
    return;

  if ((lvl < 1 && !fndef.key) || ctl_cmd(line) != -1)
    cmd_do(line);
  else
    stack_push(line);
}

static void* cmd_ifblock(List *args, Cmdarg *ca)
{
  log_msg("CMD", "%d", lvl);
  if (lvl == 0)
    cmd_flush();
  stack_push(ca->cmdline->line + strlen("if"));
  cur = &tape[pos];
  cur->st = pos;
  tape[pos].type = CTL_IF;
  ++lvl;
  return 0;
}

static void* cmd_elseifblock(List *args, Cmdarg *ca)
{
  int st = cur->st;
  cur = cur->parent;
  cur->st = st;
  stack_push(ca->cmdline->line + strlen("elseif"));
  tape[pos].type = CTL_ELSEIF;
  cur = &tape[pos];
  return 0;
}

static void* cmd_elseblock(List *args, Cmdarg *ca)
{
  int st = cur->st;
  cur = cur->parent;
  cur->st = st;
  stack_push(ca->cmdline->line + strlen("else"));
  tape[pos].type = CTL_ELSE;
  cur = &tape[pos];
  return 0;
}

static void* cmd_endblock(List *args, Cmdarg *ca)
{
  if (fndefopen /* TODO: && fndef.lvl */) {
    set_func(&fndef);
    fndefopen = 0;
    cmd_flush();
  }
  else {
    cur = cur->parent;
    stack_push(ca->cmdline->line + strlen("end"));
    tape[cur->st].end = &tape[pos];
    tape[pos].type = CTL_END;
    --lvl;
    if (lvl < 1)
      cmd_start();
  }
  return 0;
}

static void* cmd_funcblock(List *args, Cmdarg *ca)
{
  const char *name = list_arg(args, 1, VAR_STRING);
  if (!name || fndefopen) {
    parse_error = 1;
    return 0;
  }
  if (ca->cmdstr->rev) {
    utarray_new(fndef.lines, &ut_str_icd);
    fndef.key = strdup(name);
    fndefopen = 1;
  }
  else { /* print */
    fn_func *fn = opt_func(name);
    for (int i = 0; i < utarray_len(fn->lines); i++)
      log_msg("CMD", "%s", *(char**)utarray_eltptr(fn->lines, i));
  }
  return 0;
}

void* cmd_call(List *args, Cmdarg *ca)
{
  log_msg("CMD", "cmd_call");
  fn_func *fn = opt_func(list_arg(args, 1, VAR_STRING));
  if (!fn)
    return 0;
  for (int i = 0; i < utarray_len(fn->lines); i++) {
    char *line = *(char**)utarray_eltptr(fn->lines, i);
    cmd_do(line);
    cmd_flush();
  }
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

static void* cmd_do_sub(Cmdline *cmdline, char *line)
{
  cmdline_build(cmdline, line);
  cmdline_req_run(cmdline);
  return cmdline_getcmd(cmdline)->ret;
}

static void cmd_sub(Cmdstr *cmdstr, Cmdline *cmdline)
{
  Cmdstr *cmd = NULL;
  char base[strlen(cmdline->line)];
  int pos = 0;
  int prevst = 0;

  while ((cmd = (Cmdstr*)utarray_next(cmdstr->chlds, cmd))) {
    strncpy(base+pos, &cmdline->line[prevst], cmd->st);
    pos += cmd->st - prevst;
    prevst = cmd->ed + 1;

    size_t len = cmd->ed - cmd->st;
    char subline[len+1];
    strncpy(subline, &cmdline->line[cmd->st+1], len-1);
    subline[len-1] = '\0';

    Cmdline newcmd;
    void *retp = cmd_do_sub(&newcmd, subline);
    if (retp) {
      char *retline = retp;
      strcpy(base+pos, retline);
      pos += strlen(retline);
    }
    cmdline_cleanup(&newcmd);
  }
  Cmdstr *last = (Cmdstr*)utarray_back(cmdstr->chlds);
  strcpy(base+pos, &cmdline->line[last->ed+1]);
  Cmdline newcmd;
  void *retp = cmd_do_sub(&newcmd, base);
  if (retp) {
    cmdstr->ret = strdup((char*)retp);
    cmdstr->ret_t = STRING;
  }
  cmdline_cleanup(&newcmd);
}

static void cmd_vars(Cmdline *cmdline)
{
  log_msg("CMD", "cmd_vars");
  int count = utarray_len(cmdline->vars);
  char *var_lst[count];
  Token *tok_lst[count];
  int len_lst[count];
  int size = 0;

  for (int i = 0; i < count; i++) {
    Token *word = (Token*)utarray_eltptr(cmdline->vars, i);
    char *name = token_val(word, VAR_STRING);
    char *var = opt_var(name+1);
    len_lst[i] = strlen(var);
    var_lst[i] = var;
    tok_lst[i] = word;
    size += len_lst[i];
  }

  char base[size];
  int pos = 0, prevst = 0;
  for (int i = 0; i < count; i++) {
    strncpy(base+pos, &cmdline->line[prevst], tok_lst[i]->start);
    pos += tok_lst[i]->start - prevst;
    prevst = tok_lst[i]->end;
    strcpy(base+pos, var_lst[i]);
    pos += len_lst[i];
  }
  cmd_do(base);
}

void cmd_run(Cmdstr *cmdstr, Cmdline *cmdline)
{
  log_msg("CMD", "cmd_run");
  List *args = token_val(&cmdstr->args, VAR_LIST);

  if (utarray_len(cmdstr->chlds) > 0)
    return cmd_sub(cmdstr, cmdline);

  if (utarray_len(cmdline->vars) > 0)
    return cmd_vars(cmdline);

  char *word = list_arg(args, 0, VAR_STRING);
  Cmd_T *fun = cmd_find(word);
  if (!fun) {
    cmdstr->ret_t = WORD;
    cmdstr->ret = cmdline->line;
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
  compl_new(HASH_COUNT(cmd_table) - (LENGTH(builtins) - 1), COMPL_STATIC);
  for (it = cmd_table; it != NULL; it = it->hh.next) {
    if (ctl_cmd(it->name) == -1) {
      compl_set_key(i, "%s", it->name);
      i++;
    }
  }
}
