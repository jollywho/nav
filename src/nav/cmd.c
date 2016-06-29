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
} Cmd_ptr;

typedef struct Cmdblock Cmdblock;
struct Cmdblock {
  Cmdblock *parent;
  int brk;
  nv_block *blk;
  Cmdret ret;
};

typedef struct Script Script;
struct Script {
  nv_func *fndef;
  Cmdblock *callstack;
  Cmdblock scope;
  Symb *tape;
  Symb *cur;
  Symb root;
  int pos;
  int lvl;
  int maxpos;
  char *line;
  bool lncont;
  bool lvlcont;
  bool opendef;
  bool condition;
  bool error;
};

static Cmdret cmd_ifblock();
static Cmdret cmd_elseifblock();
static Cmdret cmd_elseblock();
static Cmdret cmd_endblock();
static Cmdret cmd_funcblock();
static Cmdret cmd_returnblock();

static Cmd_ptr *cmd_tbl;
static Script nvs;

#define IS_READ  (nvs.opendef)
#define IS_SEEK  (nvs.lvl > 0 && !IS_READ)
#define IS_PARSE (IS_READ || IS_SEEK)

static Cmd_T builtins[] = {
  {NULL,             NULL, NULL,                 0, 0},
  {"if","if",        "Conditional expression.",  cmd_ifblock,        0, 1},
  {"else","el",      "Conditional expression.",  cmd_elseblock,      0, 1},
  {"elseif","elif",  "Conditional expression.",  cmd_elseifblock,    0, 1},
  {"end","en",       "End function definition.", cmd_endblock,       0, 1},
  {"function","fu",  "Define a function.",       cmd_funcblock,      0, 2},
  {"return","ret",   "Return from a function.",  cmd_returnblock,    0, 0},
};

static void stack_push(char *line)
{
  if (nvs.pos + 2 > nvs.maxpos) {
    nvs.maxpos *= 2;
    nvs.tape = realloc(nvs.tape, nvs.maxpos*sizeof(Symb));
    for (int i = nvs.pos+1; i < nvs.maxpos; i++)
      memset(&nvs.tape[i], 0, sizeof(Symb));
  }
  if (!line)
    line = "";
  nvs.tape[++nvs.pos].line = strdup(line);
  nvs.tape[nvs.pos].parent = nvs.cur;
  STAILQ_INIT(&nvs.tape[nvs.pos].childs);
  STAILQ_INSERT_TAIL(&nvs.cur->childs, &nvs.tape[nvs.pos], ent);
}

static void read_line(char *line)
{
  utarray_push_back(nvs.fndef->lines, &line);
}

static void cmd_reset()
{
  if (!IS_READ || nvs.error) {
    nvs.fndef = NULL;
    nvs.opendef = 0;
  }
  nvs.line = NULL;
  nvs.error = false;
  nvs.lncont = false;
  nvs.lvlcont = false;
  nvs.lvl = 0;
  nvs.pos = -1;
  nvs.maxpos = BUFSIZ;
  nvs.tape = calloc(nvs.maxpos, sizeof(Symb));
  nvs.cur = &nvs.root;
  nvs.cur->parent = &nvs.root;
  STAILQ_INIT(&nvs.cur->childs);
}

void cmd_init()
{
  cmd_reset();
  nvs.callstack = NULL;
  for (int i = 1; i < LENGTH(builtins); i++)
    cmd_add(&builtins[i]);
}

void cmd_cleanup()
{
  for (int i = 0; nvs.tape[i].line; i++)
    free(nvs.tape[i].line);
  free(nvs.tape);
  cmd_clearall();
}

int name_sort(Cmd_ptr *a, Cmd_ptr *b)
{
  return strcmp(a->name, b->name);
}

void cmd_sort_cmds()
{
  HASH_SORT(cmd_tbl, name_sort);
}

void cmd_flush()
{
  log_msg("CMD", "flush");

  if (nvs.error && nvs.opendef) {
    nv_err("parse error: open definition not closed!");
    del_param_list(nvs.fndef->argv, nvs.fndef->argc);
    utarray_free(nvs.fndef->lines);
    free(nvs.fndef->key);
    free(nvs.fndef);
  }
  if (nvs.lvl > 0)
    nv_err("parse error: open block not closed!");
  if (nvs.lvlcont)
    nv_err("parse error: open '(' not closed!");
  for (int i = 0; nvs.tape[i].line; i++)
    free(nvs.tape[i].line);

  free(nvs.tape);
  free(nvs.line);
  cmd_reset();
}

static void push_callstack(Cmdblock *blk)
{
  if (!nvs.callstack)
    nvs.callstack = blk;
  blk->parent = nvs.callstack;
  nvs.callstack = blk;
}

static void pop_callstack()
{
  if (!nvs.callstack)
    return;
  if (nvs.callstack != &nvs.scope)
    clear_block(nvs.callstack->blk);
  if (nvs.callstack == nvs.callstack->parent)
    nvs.callstack = NULL;
  else
    nvs.callstack = nvs.callstack->parent;
}

static void ret2caller(Cmdstr *cmdstr, Cmdret ret)
{
  if (ret.type == 0)
    return;

  if (!cmdstr || !cmdstr->caller) {
    if (ret.type == OUTPUT)
      nv_msg("%s", ret.val.v_str);
    return;
  }

  if (ret.type == OUTPUT)
    ret.type = STRING;

  cmdstr->caller->ret = ret;
  if (ret.type == STRING)
    cmdstr->caller->ret.val.v_str = strdup(ret.val.v_str);
  else if (ret.type == RET_INT) {
    asprintf(&cmdstr->caller->ret.val.v_str, "%d", ret.val.v_int);
    cmdstr->caller->ret.type = STRING;
  }
}

static void cmd_do(Cmdstr *caller, char *line)
{
  if (nvs.lncont) {
    char *str;
    asprintf(&str, "%s%s", nvs.line, line);
    SWAP_ALLOC_PTR(nvs.line, str);
    line = nvs.line;
  }

  Cmdline cmd;
  cmdline_build(&cmd, line);
  nvs.lncont = MAX(cmd.lvl, cmd.cont);
  nvs.lvlcont = cmd.lvl;

  if (!nvs.lncont)
    cmdline_req_run(caller, &cmd);
  else
    SWAP_ALLOC_PTR(nvs.line, strdup(cmdline_cont_line(&cmd)));

  cmdline_cleanup(&cmd);
}

static Cmdret cmd_call(Cmdstr *caller, nv_func *fn, char *line)
{
  log_msg("CMD", "cmd_call");
  if (!line)
    line = "";
  Cmdline cmd;
  cmdline_build(&cmd, line);
  List *args = cmdline_lst(&cmd);
  int argc = utarray_len(args->items);

  Cmdblock blk = {.brk = 0, .blk = &(nv_block){}};
  push_callstack(&blk);
  caller->ret = nvs.callstack->ret;

  if (argc != fn->argc) {
    //TODO: send error up nvs.callstack
    nv_err("incorrect arguments to call: expected %d, got %d!",
        fn->argc, argc);
    goto cleanup;
  }

  for (int i = 0; i < argc; i++) {
    char *param = list_arg(args, i, VAR_STRING);
    if (!param)
      continue;
    nv_var var = {
      .key = strdup(fn->argv[i]),
      .var = strdup(param),
    };
    set_var(&var, cmd_callstack());
  }
  for (int i = 0; i < utarray_len(fn->lines); i++) {
    char *line = *(char**)utarray_eltptr(fn->lines, i);
    cmd_eval(NULL, line);
    if (nvs.callstack->brk)
      break;
  }
  caller->ret = nvs.callstack->ret;
cleanup:
  cmdline_cleanup(&cmd);
  pop_callstack();
  return NORET;
}

static void cmd_sub(Cmdstr *caller, Cmdline *cmdline)
{
  log_msg("CMD", "sub");
  nvs.condition = false;

  Cmdstr *cmd = NULL;
  int maxlen = strlen(cmdline->line) + 2;
  char *base = malloc(maxlen);
  int pos = 0;
  int prevst = 0;

  while ((cmd = (Cmdstr*)utarray_next(caller->chlds, cmd))) {
    int nlen = cmd->st - prevst;
    if (maxlen < pos + nlen)
      base = realloc(base, maxlen += nlen + 2);
    strncpy(base+pos, &cmdline->line[prevst], cmd->st - prevst);
    pos += nlen;
    prevst = cmd->ed + 1;

    size_t len = cmd->ed - cmd->st;
    char subline[len+1];
    strncpy(subline, &cmdline->line[cmd->st+1], len-1);
    subline[len-1] = '\0';

    cmd_do(cmd, subline);
    if (cmd->ret.type != STRING)
      continue;

    char *scope = "";
    char *symb = list_arg(cmdline_lst(cmdline), cmd->idx, VAR_STRING);
    Pair *p = list_arg(cmdline_lst(cmdline), cmd->idx, VAR_PAIR);
    int slen = 0;
    if (p) {
      scope = token_val(&p->key, VAR_STRING);
      symb = token_val(&p->value, VAR_STRING);
      load_module(scope);
      slen = strlen(scope) + 2;
    }

    if (symb) {
      nv_func *fn = opt_func(symb, cmd_callstack());
      if (fn) {
        Cmdstr rstr;
        cmd_call(&rstr, fn, cmd->ret.val.v_str);
        pos -= strlen(symb) + slen;
        free(cmd->ret.val.v_str);
        cmd->ret = rstr.ret;
        if (cmd->ret.type != STRING)
          cmd->ret.val.v_str = "''";
      }
    }

    if (p)
      cmd_unload();

    char *retline = cmd->ret.val.v_str;
    int retlen = strlen(retline);
    if (maxlen < pos + retlen)
      base = realloc(base, maxlen += retlen + 2);
    strcpy(base+pos, retline);
    pos += retlen;
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
  int size = strlen(cmdline->line);

  for (int i = 0; i < count; i++) {
    Token *word = (Token*)utarray_eltptr(cmdline->vars, i);
    char *var = opt_var(word, cmd_callstack());
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
    free(var_lst[i]);
  }
  strcpy(base+pos, &cmdline->line[prevst]);
  cmd_do(caller->caller, base);
}

static void cmd_arrys(Cmdstr *caller, Cmdline *cmdline, List *args)
{
  log_msg("CMD", "cmd_arrys");
  int count = utarray_len(cmdline->arys);
  char *var_lst[count];
  int len_lst[count];
  int st_lst[count], ed_lst[count];
  int size = strlen(cmdline->line);
  int num = 0;

  for (int i = 0; i < count; i++) {
    int fst = *(int*)utarray_eltptr(cmdline->arys, i);
    Token *ary = tok_arg(args, fst);

    int lst = ++fst;
    Token *get = ary;
    Token *tmp;
    while ((tmp = tok_arg(args, lst++))) {
      if (get->end != tmp->start)
        break;
      if (get != ary)
        i++;
      get = tmp;
    }

    if (!ary)
      return nv_err("parse error: invalid expression");

    Token *elem = container_elem(ary, args, fst, lst - 1);
    if (!elem)
      return nv_err("parse error: index not found");

    char *var = container_str(cmdline->line, elem);
    log_msg("CMD", "var %s", var);

    len_lst[num] = strlen(var);
    var_lst[num] = var;
    st_lst[num] = ary->start;
    ed_lst[num] = get->end;
    size += len_lst[num];
    num++;
  }

  char base[size];
  int pos = 0, prevst = 0;
  for (int i = 0; i < num; i++) {
    strncpy(base+pos, &cmdline->line[prevst], st_lst[i] - prevst);
    pos += st_lst[i] - prevst;
    prevst = ed_lst[i];
    strcpy(base+pos, var_lst[i]);
    free(var_lst[i]);
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
  return strpbrk(str, "!=<>");
}

static char* cmd_int_comp(int lhs, int rhs, char ch, char nch, int *ret)
{
  if (ch == '!' && nch == '=')
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
  if (ch == '!' && nch == '=')
    *ret = diff != 0;
  else if (ch == '=' && nch == '=')
    *ret = diff == 0;
  else if (ch == '>' && nch == '=')
    *ret = diff >= 0;
  else if (ch == '<' && nch == '=')
    *ret = diff <= 0;
  else if (ch == '<')
    *ret = diff < 0;
  else if (ch == '>')
    *ret = diff > 0;
  return NULL;
}

static int cond_do(char *line)
{
  nvs.condition = true;
  Cmdstr nstr;
  cmd_eval(&nstr, line);
  if (nstr.ret.type == 0)
    return 0;

  int cond = 0;
  char *err = NULL;
  char *condline = nstr.ret.val.v_str;

  Cmdline cmd;
  cmdline_build(&cmd, condline);

  Cmdstr *cmdstr = (Cmdstr*)utarray_front(cmd.cmds);
  List *args = token_val(&cmdstr->args, VAR_LIST);

  for (int i = 0; i < utarray_len(args->items); i++) {
    char *opstr = op_arg(args, i, cmdstr->st, condline);

    if (!opstr) {
      char *lhs = list_arg(args, i, VAR_STRING);
      int dlhs;
      int tlhs = str_num(lhs, &dlhs);
      if (tlhs)
        cond = dlhs;
      else
        cond = lhs != NULL;
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

    if (ch == '!' && !nch) {
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
  nvs.error = 1;
  nv_err("%s:%s", err, condline);
cleanup:
  cmdline_cleanup(&cmd);
  free(condline);
  nvs.condition = false;
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
  Symb *it = STAILQ_FIRST(&nvs.root.childs);
  int cond;
  while (it) {
    switch (it->type) {
      case CTL_IF:
      case CTL_ELSEIF:
        cond = cond_do(it->line);
        if (nvs.error)
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
        if (it->parent == &nvs.root)
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
  while (*line == ' ')
    line++;

  for (int i = 1; i < LENGTH(builtins) - 1; i++) {
    char *str = builtins[i].name;
    if (!strncmp(str, line, strlen(str)))
      return i;
    char *alt = builtins[i].alt;
    if (!strncmp(alt, line, strlen(str)))
      return i;
  }
  return -1;
}

void cmd_eval(Cmdstr *caller, char *line)
{
  if (nvs.error)
    return;
  log_msg("CMD", "eval: [%s]", line);

  if (!IS_PARSE || ctl_cmd(line) != -1)
    return cmd_do(caller, line);
  if (IS_READ)
    return read_line(line);
  if (IS_SEEK)
    stack_push(line);
}

static Cmdret cmd_ifblock(List *args, Cmdarg *ca)
{
  log_msg("CMD", "ifblock");
  if (IS_READ) {
    ++nvs.lvl;
    read_line(ca->cmdline->line);
    return NORET;
  }
  if (!IS_SEEK)
    cmd_flush();
  ++nvs.lvl;
  stack_push(cmdline_line_after(ca->cmdline, 0));
  nvs.cur = &nvs.tape[nvs.pos];
  nvs.cur->st = nvs.pos;
  nvs.tape[nvs.pos].type = CTL_IF;
  return NORET;
}

static Cmdret cmd_elseifblock(List *args, Cmdarg *ca)
{
  if (IS_READ) {
    read_line(ca->cmdline->line);
    return NORET;
  }
  int st = nvs.cur->st;
  nvs.cur = nvs.cur->parent;
  nvs.cur->st = st;
  stack_push(cmdline_line_after(ca->cmdline, 0));
  nvs.tape[nvs.pos].type = CTL_ELSEIF;
  nvs.cur = &nvs.tape[nvs.pos];
  return NORET;
}

static Cmdret cmd_elseblock(List *args, Cmdarg *ca)
{
  if (IS_READ) {
    read_line(ca->cmdline->line);
    return NORET;
  }
  int st = nvs.cur->st;
  nvs.cur = nvs.cur->parent;
  nvs.cur->st = st;
  stack_push(cmdline_line_after(ca->cmdline, 0));
  nvs.tape[nvs.pos].type = CTL_ELSE;
  nvs.cur = &nvs.tape[nvs.pos];
  return NORET;
}

static Cmdret cmd_endblock(List *args, Cmdarg *ca)
{
  log_msg("CMD", "endblock");
  --nvs.lvl;
  if (IS_READ && nvs.lvl == 0) {
    set_func(nvs.fndef, cmd_callstack());
    nvs.opendef = 0;
    cmd_flush();
    return NORET;
  }
  if (IS_READ) {
    read_line(ca->cmdline->line);
    return NORET;
  }
  nvs.cur = nvs.cur->parent;
  stack_push("");
  nvs.tape[nvs.cur->st].end = &nvs.tape[nvs.pos];
  nvs.tape[nvs.pos].type = CTL_END;
  if (!IS_SEEK) {
    cmd_start();
    nvs.lvl = 0;
  }
  return NORET;
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

static Cmdret cmd_funcblock(List *args, Cmdarg *ca)
{
  const char *name = list_arg(args, 1, VAR_STRING);
  if (!name || IS_PARSE) {
    nvs.error = 1;
    return NORET;
  }

  if (ca->cmdstr->rev) {
    ++nvs.lvl;
    nvs.fndef = malloc(sizeof(nv_func));
    nvs.fndef->argc = mk_param_list(ca, &nvs.fndef->argv);
    utarray_new(nvs.fndef->lines, &ut_str_icd);
    nvs.fndef->key = strdup(name);
    nvs.opendef = 1;
  }
  else { /* print */
    nv_func *fn = opt_func(name, cmd_callstack());
    for (int i = 0; i < utarray_len(fn->lines); i++)
      log_msg("CMD", "%s", *(char**)utarray_eltptr(fn->lines, i));
  }
  return NORET;
}

static Cmdret cmd_returnblock(List *args, Cmdarg *ca)
{
  log_msg("CMD", "cmd_return");
  if (!nvs.callstack) {
    nvs.error = 1;
    return NORET;
  }

  nvs.callstack->brk = 1;
  char *line = cmdline_line_from(ca->cmdline, 1);

  if (line) {
    Cmdstr nstr;
    cmd_eval(&nstr, line);
    nvs.callstack->ret = nstr.ret;
  }
  return NORET;
}

void cmd_clearall()
{
  log_msg("CMD", "cmd_clearall");
  Cmd_ptr *it, *tmp;
  HASH_ITER(hh, cmd_tbl, it, tmp) {
    HASH_DEL(cmd_tbl, it);
    free(it);
  }
}

void cmd_add(Cmd_T *src)
{
  Cmd_ptr *cmd = malloc(sizeof(Cmd_ptr));
  cmd->cmd = src;
  cmd->name = src->name;
  HASH_ADD_STR(cmd_tbl, name, cmd);

  if (!src->alt || !strcmp(src->alt, src->name))
    return;

  Cmd_ptr *alt = malloc(sizeof(Cmd_ptr));
  alt->cmd = src;
  alt->name = src->alt;
  HASH_ADD_STR(cmd_tbl, name, alt);
}

void cmd_remove(const char *name)
{
}

Cmd_T* cmd_find(const char *name)
{
  if (!name)
    return NULL;

  Cmd_ptr *cmd;
  HASH_FIND_STR(cmd_tbl, name, cmd);

  if (cmd)
    return cmd->cmd;
  return NULL;
}

void exec_line(Cmdstr *cmd, char *line)
{
  log_msg("CMD", "exec_line");
  char *str = strstr(line, "!");
  ++str;
  char *pidstr;
  int pid = shell_exec(str, focus_dir());

  asprintf(&pidstr, "%d", pid);
  ret2caller(cmd, (Cmdret){STRING, .val.v_str = pidstr});
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

    if (utarray_len(cmdline->arys) > 0)
      return cmd_arrys(cmdstr, cmdline, args);
  }

  if (cmdline_can_exec(cmdstr, word))
    return exec_line(cmdstr, cmdline->line);

  if (!fun)
    return ret2caller(cmdstr, (Cmdret){STRING, .val.v_str = cmdline->line});

  Cmdarg flags = {fun->flags, 0, cmdstr, cmdline};
  return ret2caller(cmdstr, fun->cmd_func(args, &flags));
}

nv_block* cmd_callstack()
{
  if (nvs.callstack)
    return nvs.callstack->blk;
  return NULL;
}

void cmd_load(nv_block *blk)
{
  nvs.scope = (Cmdblock){.brk = 0, .blk = blk};
  push_callstack(&nvs.scope);
}

void cmd_unload()
{
  /* stack should be empty except scope */
  pop_callstack();
}

bool cmd_conditional()
{
  return nvs.condition;
}

void cmd_list(List *args)
{
  log_msg("CMD", "compl cmd_list");
  int i = 0;
  Cmd_ptr *it;
  for (it = cmd_tbl; it != NULL; it = it->hh.next) {
    compl_list_add("%s", it->name);
    compl_set_col(i, "%s", it->cmd->msg);
    i++;
  }
}
