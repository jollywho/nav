#include <sys/queue.h>
#include "nav/cmd.h"
#include "nav/log.h"
#include "nav/compl.h"
#include "nav/table.h"
#include "nav/option.h"
#include "nav/util.h"
#include "nav/model.h"
#include "nav/event/shell.h"
#include "nav/tui/message.h"

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
  void *ret;
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
  {"elseif","elif",  cmd_elseifblock,    0, 1},
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
  for (int i = 1; i < LENGTH(builtins); i++)
    cmd_add(&builtins[i]);
}

void cmd_cleanup()
{
  for (int i = 0; tape[i].line; i++)
    free(tape[i].line);
  free(tape);
  cmd_clearall();
}

int name_sort(Cmd_T *a, Cmd_T *b)
{
  return strcmp(a->name, b->name);
}

void cmd_sort_cmds()
{
  HASH_SORT(cmd_table, name_sort);
}

void cmd_flush()
{
  log_msg("CMD", "flush");

  if (parse_error && fndefopen) {
    nv_err("parse error: open definition not closed!");
    del_param_list(fndef->argv, fndef->argc);
    utarray_free(fndef->lines);
    free(fndef->key);
    free(fndef);
  }
  if (lvl > 0)
    nv_err("parse error: open block not closed!");
  if (lvlcont > 0)
    nv_err("parse error: open '(' not closed!");
  for (int i = 0; tape[i].line; i++)
    free(tape[i].line);

  free(tape);
  free(lncont);
  cmd_reset();
}

static void push_callstack(Cmdblock *blk, fn_func *fn)
{
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

static void ret2caller(Cmdstr *cmdstr, int ret_t, void *ret)
{
  if (!ret || ret_t == 0)
    return;

  if (!cmdstr || !cmdstr->caller) {
    if (ret_t == STRING)
      nv_msg("%s", ret);
    return;
  }

  cmdstr->caller->ret_t = ret_t;
  cmdstr->caller->ret = strdup(ret);
}

static void cmd_do(Cmdstr *caller, char *line)
{
  char *swap = NULL;
  if (exparg_isset()) {
    swap = line;
    line = do_expansion(line, NULL);
  }

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
  if (swap)
    free(line);
}

static void* cmd_call(Cmdstr *caller, fn_func *fn, char *line)
{
  log_msg("CMD", "cmd_call");
  if (!line)
    line = "";
  Cmdline cmd;
  cmdline_build(&cmd, line);
  List *args = cmdline_lst(&cmd);
  int argc = utarray_len(args->items);
  if (argc != fn->argc) {
    nv_err("incorrect arguments to call: expected %d, got %d!",
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
    cmd_eval(NULL, line);
    if (callstack->brk)
      break;
  }
  caller->ret = callstack->ret;
cleanup:
  cmdline_cleanup(&cmd);
  pop_callstack();
  return 0;
}

static void cmd_sub(Cmdstr *caller, Cmdline *cmdline)
{
  Cmdstr *cmd = NULL;
  int maxlen = strlen(cmdline->line) + 2;
  char *base = malloc(maxlen);
  int pos = 0;
  int prevst = 0;

  while ((cmd = (Cmdstr*)utarray_next(caller->chlds, cmd))) {
    int nlen = cmd->st - prevst;
    if (maxlen < pos + nlen)
      base = realloc(base, maxlen += nlen + 2);
    strncpy(base+pos, &cmdline->line[prevst], cmd->st);
    pos += nlen;
    prevst = cmd->ed + 1;

    size_t len = cmd->ed - cmd->st;
    char subline[len+1];
    strncpy(subline, &cmdline->line[cmd->st+1], len-1);
    subline[len-1] = '\0';

    cmd_do(cmd, subline);

    List *args = cmdline_lst(cmdline);
    char *symb = list_arg(args, cmd->idx, VAR_STRING);
    if (symb) {
      fn_func *fn = opt_func(symb);
      if (fn) {
        Cmdstr rstr;
        cmd_call(&rstr, fn, cmd->ret);
        pos -= strlen(symb);
        if (rstr.ret)
          SWAP_ALLOC_PTR(cmd->ret, rstr.ret);
      }
      //TODO: error here unless symb is '$'
    }

    if (cmd->ret) {
      char *retline = cmd->ret;
      int retlen = strlen(retline);
      if (maxlen < pos + retlen)
        base = realloc(base, maxlen += retlen + 2);
      strcpy(base+pos, retline);
      pos += retlen;
    }
  }
  int nlen = caller->ed;
  if (maxlen < pos + nlen)
    base = realloc(base, maxlen += nlen + 2);
  strcpy(base+pos, &cmdline->line[prevst]);

  cmd_do(caller->caller, base);
  free(base);
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
  cmd_do(caller->caller, base);
}

static char* op_arg(List *args, int argc, int st, char *line)
{
  Token *tok = tok_arg(args, argc);
  if (tok->start - 1 < st || line[tok->start - 1] == '"')
    return NULL;
  char *str = list_arg(args, argc, VAR_STRING);
  if (!str)
    return NULL;
  return strpbrk(str, "~=<>");
}

static char* cmd_int_comp(int lhs, int rhs, char ch, char nch, int *ret)
{
  if (ch == '~' && nch == '=')
    *ret = lhs != rhs;
  else if (ch == '=' && nch == '=')
    *ret = lhs == rhs;
  else if (ch == '>' && nch == '=')
    *ret = lhs >= rhs;
  else if (ch == '<' && nch == '=')
    *ret = lhs <= rhs;
  else if (ch == '<')
    *ret = lhs < rhs;
  else if (ch == '>')
    *ret = lhs > rhs;

  return NULL;
}

static char* cmd_str_comp(char *lhs, char *rhs, char ch, char nch, int *ret)
{
  log_msg("CMD", "strcomp");
  int diff = strcmp(lhs, rhs);
  if (ch == '~' && nch == '=')
    *ret = diff != 0;
  else if (ch == '=' && nch == '=')
    *ret = diff == 0;
  else if (ch == '>' && nch == '=')
    *ret = diff <= 0;
  else if (ch == '<' && nch == '=')
    *ret = diff >= 0;
  else if (ch == '<')
    *ret = diff > 0;
  else if (ch == '>')
    *ret = diff < 0;
  return NULL;
}

static int cond_do(char *line)
{
  log_msg("CMD", "cond");
  Cmdstr nstr;
  cmd_eval(&nstr, line);
  if (!nstr.ret)
    return 0;

  int cond = 0;
  char *err = NULL;

  Cmdline cmd;
  cmdline_build(&cmd, nstr.ret);

  Cmdstr *cmdstr = (Cmdstr*)utarray_front(cmd.cmds);
  List *args = token_val(&cmdstr->args, VAR_LIST);

  for (int i = 0; i < utarray_len(args->items); i++) {
    char *opstr = op_arg(args, i, cmdstr->st, nstr.ret);
    log_err("CMD", "%s", opstr);

    if (!opstr) {
      char *lhs = list_arg(args, i, VAR_STRING);
      int dlhs;
      int tlhs = str_num(lhs, &dlhs);
      if (tlhs)
        cond = dlhs;
      else
        cond = !!lhs;
      continue;
    }

    char ch  = opstr[0];
    char nch = opstr[1];

    char *lhs = list_arg(args, i - 1, VAR_STRING);
    char *rhs = list_arg(args, i + 1, VAR_STRING);
    i++;

    int dlhs, drhs;
    int tlhs = str_num(lhs, &dlhs);
    int trhs = str_num(rhs, &drhs);

    if (ch == '~' && !nch) {
      if (!rhs) {
        err = "syntax error, rhs";
        goto error;
      }
      if (trhs)
        cond = !drhs;
      else
        cond = !rhs;
      continue;
    }

    if (!lhs || !rhs) {
      err = "syntax error, lhs & rhs";
      goto error;
    }

    if (tlhs != trhs) {
      err = "syntax error, type mismatch";
      goto error;
    }

    if (tlhs)
      err = cmd_int_comp(dlhs, drhs, ch, nch, &cond);
    else
      err = cmd_str_comp(lhs, rhs, ch, nch, &cond);

    if (err)
      goto error;

    break;
  }

  goto cleanup;
error:
  parse_error = 1;
  nv_err("%s:%s", err, nstr.ret);
cleanup:
  cmdline_cleanup(&cmd);
  free(nstr.ret);
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
  log_msg("CMD", "start");
  Symb *it = STAILQ_FIRST(&root.childs);
  int cond;
  while (it) {
    switch (it->type) {
      case CTL_IF:
      case CTL_ELSEIF:
        cond = cond_do(it->line);
        if (parse_error)
          return;
        log_err("CMD", "cond ret %d", cond);
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
  log_msg("CMD", ": %s", line);

  if (!IS_PARSE || ctl_cmd(line) != -1)
    return cmd_do(caller, line);
  if (IS_READ)
    return read_line(line);
  if (IS_SEEK)
    stack_push(line);
}

static void* cmd_ifblock(List *args, Cmdarg *ca)
{
  log_msg("CMD", "ifblock");
  if (IS_READ) {
    ++lvl;
    read_line(ca->cmdline->line);
    return 0;
  }
  if (!IS_SEEK)
    cmd_flush();
  ++lvl;
  stack_push(cmdline_line_after(ca->cmdline, 0));
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
  stack_push(cmdline_line_after(ca->cmdline, 0));
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
  stack_push(cmdline_line_after(ca->cmdline, 0));
  tape[pos].type = CTL_ELSE;
  cur = &tape[pos];
  return 0;
}

static void* cmd_endblock(List *args, Cmdarg *ca)
{
  log_msg("CMD", "endblock");
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
  if (!IS_SEEK) {
    cmd_start();
    lvl = 0;
  }
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
  nv_err("parse error: invalid function argument!");
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
  log_msg("CMD", "return %s", out);
  callstack->ret = strdup(out);
  //FIXME: use ret2caller
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
  Cmd_alt *ait, *atmp;
  HASH_ITER(hh, alt_tbl, ait, atmp) {
    HASH_DEL(alt_tbl, ait);
    free(ait);
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

void exec_line(Cmdstr *cmd, char *line)
{
  char *str = strstr(line, "!");
  ++str;
  Exparg exparg = {.expfn = model_str_expansion, .key = NULL};
  str = do_expansion(str, &exparg);
  char *pidstr;
  int pid = shell_exec(str, NULL, focus_dir(), NULL);
  asprintf(&pidstr, "%d", pid);

  //TODO: hook output + block for output
  free(str);
  ret2caller(cmd, WORD, pidstr);
  free(pidstr);
}

void cmd_run(Cmdstr *cmdstr, Cmdline *cmdline)
{
  log_msg("CMD", "cmd_run");
  log_msg("CMD", "%s", cmdline->line);
  List *args = token_val(&cmdstr->args, VAR_LIST);
  char *word = list_arg(args, 0, VAR_STRING);
  Cmd_T *fun = cmd_find(word);

  if (!fun || !fun->bflags) {
    if (utarray_len(cmdstr->chlds) > 0)
      return cmd_sub(cmdstr, cmdline);

    if (utarray_len(cmdline->vars) > 0)
      return cmd_vars(cmdstr, cmdline);
  }

  if (cmdline_can_exec(cmdstr, word))
    return exec_line(cmdstr, cmdline->line);

  if (!fun)
    return ret2caller(cmdstr, WORD, cmdline->line);

  Cmdarg flags = {fun->flags, 0, cmdstr, cmdline};
  return ret2caller(cmdstr, flags.flags, fun->cmd_func(args, &flags));
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
