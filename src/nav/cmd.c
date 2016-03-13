#include <sys/queue.h>
#include "nav/cmd.h"
#include "nav/log.h"
#include "nav/compl.h"
#include "nav/table.h"
#include "nav/option.h"
#include "nav/util.h"

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

typedef struct {
  char *name;
  Cmd_T *cmd;
  UT_hash_handle hh;
} Cmd_alt;

typedef struct Cmdblock Cmdblock;
struct Cmdblock {
  Cmdblock *parent;
  fn_func *func;
  int brk;
};

static void* cmd_ifblock();
static void* cmd_elseifblock();
static void* cmd_elseblock();
static void* cmd_endblock();
static void* cmd_funcblock();
static void* cmd_returnblock();

//TODO: wrap these into cmdblock
static Cmd_T *cmd_table;
static Cmd_alt *alt_tbl;
static Symb *tape;
static Symb *cur;
static Symb root;
static int pos;
static int lvl;
static int maxpos;
static char *lncont;
static void *gret;
static int lvlcont;
static int fndefopen;
static int parse_error;

#define IS_READ  (fndefopen)
#define IS_SEEK  (lvl > 0 && !IS_READ)
#define IS_PARSE (IS_READ || IS_SEEK)

static fn_func *fndef;
static Cmdblock *callstack;

static const Cmd_T builtins[] = {
  {NULL,             NULL,               0, 0},
  {"if",0,           cmd_ifblock,        0, 1},
  {"else","el",      cmd_elseblock,      0, 1},
  {"elseif","elsei", cmd_elseifblock,    0, 1},
  {"end","en",       cmd_endblock,       0, 1},
  {"function","fu",  cmd_funcblock,      0, 2},
  {"return","ret",   cmd_returnblock,    0, 0},
};

static void stack_push(char *line)
{
  if (pos + 2 > maxpos) {
    maxpos *= 2;
    tape = realloc(tape, maxpos*sizeof(Symb));
    for (int i = pos+1; i < maxpos; i++)
      memset(&tape[i], 0, sizeof(Symb));
  }
  if (!line)
    line = "";
  tape[++pos].line = strdup(line);
  tape[pos].parent = cur;
  STAILQ_INIT(&tape[pos].childs);
  STAILQ_INSERT_TAIL(&cur->childs, &tape[pos], ent);
}

static void read_line(char *line)
{
  utarray_push_back(fndef->lines, &line);
}

static void cmd_reset()
{
  if (!IS_READ || parse_error) {
    fndef = NULL;
    fndefopen = 0;
  }
  lncont = NULL;
  lvlcont = 0;
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
  callstack = NULL;
  gret = 0;
  for (int i = 1; i < LENGTH(builtins); i++)
    cmd_add(&builtins[i]);
}

int name_sort(Cmd_T *a, Cmd_T *b)
{
  return strcmp(a->name, b->name);
}

void cmd_sort_cmds()
{
  HASH_SORT(cmd_table, name_sort);
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

  if (parse_error && fndefopen) {
    log_err("CMD", "parse error: open definition not closed!");
    del_param_list(fndef->argv, fndef->argc);
    utarray_free(fndef->lines);
    free(fndef->key);
    free(fndef);
  }
  if (lvl > 0)
    log_err("CMD", "parse error: open block not closed!");
  if (lvlcont > 0)
    log_err("CMD", "parse error: open '(' not closed!");
  for (int i = 0; tape[i].line; i++)
    free(tape[i].line);

  free(tape);
  free(lncont);
  cmd_reset();
}

static void push_callstack(Cmdblock *blk, fn_func *fn)
{
  if (gret) {
    free(gret);
    gret = 0;
  }
  if (!callstack)
    callstack = blk;
  blk->parent = callstack;
  blk->func = fn;
  callstack = blk;
}

static void pop_callstack()
{
  clear_locals(callstack->func);
  if (callstack == callstack->parent)
    callstack = NULL;
  else
    callstack = callstack->parent;
}

void ret2caller(Cmdstr *cmdstr, int ret_t, void *ret)
{
  if (!cmdstr || !cmdstr->caller)
    return;
  cmdstr->caller->ret_t = ret_t;
  if (ret)
    cmdstr->caller->ret = strdup(ret);
}

static void cmd_do(Cmdstr *caller, char *line)
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
    cmdline_req_run(caller, &cmd);
  else
    SWAP_ALLOC_PTR(lncont, strdup(line));

  cmdline_cleanup(&cmd);
}

static void* cmd_call(Cmdstr *rcmd, fn_func *fn, char *line)
{
  log_msg("CMD", "cmd_call");
  log_msg("CMD", "%s", line);
  Cmdline cmd;
  cmdline_build(&cmd, line);
  List *args = cmdline_lst(&cmd);
  int argc = utarray_len(args->items);
  if (argc != fn->argc) {
    log_err("CMD", "incorrect arguments to call: expected %d, got %d!",
        fn->argc, argc);
    goto cleanup;
  }
  Cmdblock blk = {.brk = 0};
  push_callstack(&blk, fn);

  for (int i = 0; i < argc; i++) {
    char *param = list_arg(args, i, VAR_STRING);
    if (!param)
      continue;
    fn_var var = {
      .key = strdup(fn->argv[i]),
      .var = strdup(param),
    };
    set_var(&var, cmd_callstack());
  }
  for (int i = 0; i < utarray_len(fn->lines); i++) {
    char *line = *(char**)utarray_eltptr(fn->lines, i);
    cmd_eval(rcmd, line);
    if (callstack->brk)
      break;
  }
  log_msg("CMD", "call fin--");
cleanup:
  cmdline_cleanup(&cmd);
  pop_callstack();
  return 0;
}

static void* cmd_do_sub(Cmdstr *caller, Cmdline *cmdline, char *line)
{
  cmdline_build(cmdline, line);
  cmdline_req_run(caller, cmdline);
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

    cmd_do(cmd, subline);
    void *retp = cmd->ret;

    List *args = cmdline_lst(cmdline);
    char *symb = list_arg(args, cmd->idx - 1, VAR_STRING);
    log_msg("CMD", "--- %s", symb);
    if (symb) {
      fn_func *fn = opt_func(symb);
      //TODO: error here unless symb is '$'
      if (fn) {
        cmd_call(cmd, fn, retp);
        Token *word = tok_arg(args, cmd->idx - 1);
        pos = word->start;
        retp = gret;
      }
    }

    if (retp) {
      char *retline = retp;
      strcpy(base+pos, retline);
      pos += strlen(retline);
    }
  }
  Cmdstr *last = (Cmdstr*)utarray_back(cmdstr->chlds);
  strcpy(base+pos, &cmdline->line[last->ed+1]);
  Cmdline newcmd;
  void *retp = cmd_do_sub(cmdstr, &newcmd, base);
  ret2caller(cmdstr, STRING, retp);
  cmdline_cleanup(&newcmd);
}

static void cmd_vars(Cmdstr *caller, Cmdline *cmdline)
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
    char *var = opt_var(name+1, cmd_callstack());
    len_lst[i] = strlen(var);
    var_lst[i] = var;
    tok_lst[i] = word;
    size += len_lst[i];
  }

  char base[size];
  int pos = 0, prevst = 0;
  for (int i = 0; i < count; i++) {
    strncpy(base+pos, &cmdline->line[prevst], tok_lst[i]->start - prevst);
    pos += tok_lst[i]->start - prevst;
    prevst = tok_lst[i]->end;
    strcpy(base+pos, var_lst[i]);
    pos += len_lst[i];
  }
  strcpy(base+pos, &cmdline->line[prevst]);
  log_msg("CMD", "cmd_vars %s", base);
  cmd_do(caller, base);
}

static int cond_do(Cmdstr *caller, char *line)
{
  int cond = 0;
  Cmdline cmd;
  cmdline_build(&cmd, line);
  cmdline_req_run(caller, &cmd);
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

static void cmd_start()
{
  Symb *it = STAILQ_FIRST(&root.childs);
  int cond;
  while (it) {
    switch (it->type) {
      case CTL_IF:
      case CTL_ELSEIF:
        cond = cond_do(NULL, it->line);
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
        cmd_do(NULL, it->line);
        it = cmd_next(it);
    }
  }
}

static int ctl_cmd(const char *line)
{
  for (int i = 1; i < LENGTH(builtins) - 1; i++) {
    char *str = builtins[i].name;
    if (!strncmp(str, line, strlen(str)))
      return i;
  }
  return -1;
}

void cmd_eval(Cmdstr *caller, char *line)
{
  if (parse_error)
    return;

  if (!IS_PARSE || ctl_cmd(line) != -1)
    return cmd_do(caller, line);
  if (IS_READ)
    return read_line(line);
  if (IS_SEEK)
    stack_push(line);
}

static void* cmd_ifblock(List *args, Cmdarg *ca)
{
  if (IS_READ) {
    ++lvl;
    read_line(ca->cmdline->line);
    return 0;
  }
  if (lvl == 1)
    cmd_flush();
  ++lvl;
  stack_push(cmdline_line_from(ca->cmdline, 1));
  cur = &tape[pos];
  cur->st = pos;
  tape[pos].type = CTL_IF;
  return 0;
}

static void* cmd_elseifblock(List *args, Cmdarg *ca)
{
  if (IS_READ) {
    read_line(ca->cmdline->line);
    return 0;
  }
  int st = cur->st;
  cur = cur->parent;
  cur->st = st;
  stack_push(cmdline_line_from(ca->cmdline, 1));
  tape[pos].type = CTL_ELSEIF;
  cur = &tape[pos];
  return 0;
}

static void* cmd_elseblock(List *args, Cmdarg *ca)
{
  if (IS_READ) {
    read_line(ca->cmdline->line);
    return 0;
  }
  int st = cur->st;
  cur = cur->parent;
  cur->st = st;
  stack_push(cmdline_line_from(ca->cmdline, 1));
  tape[pos].type = CTL_ELSE;
  cur = &tape[pos];
  return 0;
}

static void* cmd_endblock(List *args, Cmdarg *ca)
{
  --lvl;
  if (IS_READ && lvl == 0) {
    set_func(fndef);
    fndefopen = 0;
    cmd_flush();
    return 0;
  }
  if (IS_READ) {
    read_line(ca->cmdline->line);
    return 0;
  }
  cur = cur->parent;
  stack_push("");
  tape[cur->st].end = &tape[pos];
  tape[pos].type = CTL_END;
  if (!IS_SEEK)
    cmd_start();
  return 0;
}

static int mk_param_list(Cmdarg *ca, char ***dest)
{
  Cmdstr *substr = (Cmdstr*)utarray_front(ca->cmdstr->chlds);
  if (!substr)
    return 0;

  char *line = ca->cmdline->line;
  char pline[strlen(line)];
  int len = (substr->ed - substr->st) - 1;
  strncpy(pline, line+substr->st + 1, len);
  pline[len] = '\0';

  Cmdline cmd;
  cmdline_build(&cmd, pline);
  Cmdstr *cmdstr = (Cmdstr*)utarray_front(cmd.cmds);
  List *args = token_val(&cmdstr->args, VAR_LIST);
  int argc = utarray_len(args->items);
  if (argc < 1)
    goto cleanup;

  (*dest) = malloc(argc * sizeof(char*));

  for (int i = 0; i < argc; i++) {
    char *name = list_arg(args, i, VAR_STRING);
    if (!name)
      goto type_error;
    (*dest)[i] = strdup(name);
  }

  goto cleanup;
type_error:
  log_err("CMD", "parse error: invalid function argument!");
  del_param_list(*dest, argc);
cleanup:
  cmdline_cleanup(&cmd);
  return argc;
}

static void* cmd_funcblock(List *args, Cmdarg *ca)
{
  const char *name = list_arg(args, 1, VAR_STRING);
  if (!name || IS_PARSE) {
    parse_error = 1;
    return 0;
  }

  if (ca->cmdstr->rev) {
    ++lvl;
    fndef = malloc(sizeof(fn_func));
    fndef->argc = mk_param_list(ca, &fndef->argv);
    utarray_new(fndef->lines, &ut_str_icd);
    fndef->key = strdup(name);
    fndefopen = 1;
  }
  else { /* print */
    fn_func *fn = opt_func(name);
    for (int i = 0; i < utarray_len(fn->lines); i++)
      log_msg("CMD", "%s", *(char**)utarray_eltptr(fn->lines, i));
  }
  return 0;
}

static void* cmd_returnblock(List *args, Cmdarg *ca)
{
  log_msg("CMD", "cmd_return");
  callstack->brk = 1;
  char *out = cmdline_line_from(ca->cmdline, 1);
  if (out)
    gret = strdup(out);
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

void cmd_add(const Cmd_T *cmd_src)
{
  Cmd_T *cmd = malloc(sizeof(Cmd_T));
  memmove(cmd, cmd_src, sizeof(Cmd_T));

  HASH_ADD_STR(cmd_table, name, cmd);
  if (cmd->alt) {
    Cmd_alt *alt = malloc(sizeof(Cmd_alt));
    alt->name = cmd->alt;
    alt->cmd = cmd;
    HASH_ADD_STR(alt_tbl, name, alt);
  }
}

void cmd_remove(const char *name)
{
}

Cmd_T* cmd_find(const char *name)
{
  if (!name)
    return NULL;
  Cmd_T *cmd;
  HASH_FIND_STR(cmd_table, name, cmd);
  if (cmd)
    return cmd;

  Cmd_alt *alt;
  HASH_FIND_STR(alt_tbl, name, alt);
  if (alt)
    cmd = alt->cmd;
  return cmd;
}

void cmd_run(Cmdstr *cmdstr, Cmdline *cmdline)
{
  log_msg("CMD", "cmd_run");
  List *args = token_val(&cmdstr->args, VAR_LIST);
  char *word = list_arg(args, 0, VAR_STRING);
  Cmd_T *fun = cmd_find(word);

  if (!fun || !fun->bflags) {
    if (utarray_len(cmdstr->chlds) > 0)
      return cmd_sub(cmdstr, cmdline);

    if (utarray_len(cmdline->vars) > 0)
      return cmd_vars(cmdstr, cmdline);
  }

  if (!fun)
    return ret2caller(cmdstr, WORD, cmdline->line);

  Cmdarg flags = {fun->flags, 0, cmdstr, cmdline};
  return ret2caller(cmdstr, PLUGIN, fun->cmd_func(args, &flags));
}

fn_func* cmd_callstack()
{
  if (callstack)
    return callstack->func;
  return NULL;
}

void cmd_list(List *args)
{
  log_msg("CMD", "compl cmd_list");
  int i = 0;
  Cmd_T *it;
  compl_new(HASH_COUNT(cmd_table), COMPL_STATIC);
  for (it = cmd_table; it != NULL; it = it->hh.next) {
    compl_set_key(i, "%s", it->name);
    i++;
  }
}
