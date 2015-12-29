#include "fnav/compl.h"
#include "fnav/tui/cntlr.h"
#include "fnav/log.h"

#define HASH_INS(t,obj) \
  HASH_ADD_KEYPTR(hh, t, obj.key, strlen(obj.key), &obj);

static fn_compl* compl_cmds();
static fn_compl* compl_win();
static fn_compl* compl_cntlr();

#define DEFAULT_SIZE ARRAY_SIZE(compl_defaults)
static compl_entry compl_defaults[] = {
  { "cmds",    compl_cmds      },
  { "win",     compl_win       },
  { "cntlrs",  compl_cntlr     },
};

static compl_entry *compl_table;
static fn_context *context_root;

void compl_init()
{
  compl_add_context("_;cmd:string:cmds");
  fn_context *find;
  HASH_FIND_STR(context_root, "_", find);
  context_root = find;

  for (int i = 0; i < (int)DEFAULT_SIZE; i++) {
    HASH_INS(compl_table, compl_defaults[i]);
  }
}

fn_compl* compl_create(fn_context *cx, String line)
{
  String key = cx->params[0]->comp;
  compl_entry *find;
  HASH_FIND_STR(compl_table, key, find);
  if (!find) return NULL;
  return find->gen(line);
}

fn_context* context_start()
{
  return context_root;
}

static int count_subgrps(String str, String fnd)
{
  int count = 0;
  const char *tmp = str;
  while((tmp = strstr(tmp, fnd)))
  {
    count++;
    tmp++;
  }
  return count;
}

static int compl_param(fn_context **arg, String param)
{
  log_msg("COMPL", "compl_param %s", param);

  int grpc = count_subgrps(param, ":");
  if (grpc > 2) {
    log_msg("COMPL", "invalid format.");
    return -1;
  }

  String name = strtok(param, ":");
  String type = strtok(NULL, ":");
  String comp = NULL;

  if (!name || !type) {
    log_msg("COMPL", "invalid format.");
    return -1;
  }
  if (grpc == 2)
    comp = strtok(NULL, ":");

  (*arg) = malloc(sizeof(fn_context));
  (*arg)->key = name;
  //arg->type = type;
  (*arg)->comp = comp;
  return 1;
}

void compl_add_context(String fmt_compl)
{
  log_msg("COMPL", "compl_add_context");
  log_msg("COMPL", "%s", fmt_compl);
  int pidx = 0;
  int grpc = count_subgrps(fmt_compl, ";");

  String line = strdup(fmt_compl);
  fn_context **args = malloc(grpc*sizeof(fn_context));

  String key;
  if (grpc > 0) {
    String saveptr;
    key = strtok_r(line, ";", &saveptr);
    String param = strtok_r(NULL, ";", &saveptr);
    for (pidx = 0; pidx < grpc; pidx++) {
      if (compl_param(&args[pidx], param) == -1)
        goto breakout;
      param = strtok_r(NULL, ";", &saveptr);
    }
  }
  else
    key = line;

  fn_context *cx = malloc(sizeof(fn_context));
  fn_context *find;
  cx->key = key;
  cx->argc = grpc;
  cx->params = args;
  HASH_FIND_STR(context_root, cx->key, find);
  if (find) {
    log_msg("COMPL", "ERROR: context already defined");
    free(cx);
    goto breakout;
  }
  else
    HASH_ADD_KEYPTR(hh, context_root, cx->key, strlen(cx->key), cx);

  return;
breakout:
  for (; pidx > 0; pidx--) {
    free(args[pidx]);
  }
  free(args);
  free(line);
  return;
}

static fn_compl* compl_cmds(String line)
{
  log_msg("COMPL", "compl_cntlr");
  fn_compl *cmplst = malloc(sizeof(fn_compl));
  cmplst->name = "cmd";
  String *lst = cmd_list(line);

  int count;
  for (count = 0; lst[count] != NULL; count++) {}
  cmplst->rowcount = count;

  cmplst->rows = malloc(count*sizeof(compl_item));

  String it = lst[0];
  for (int i = 0; i < count; i++) {
    cmplst->rows[i] = malloc(sizeof(compl_item));
    cmplst->rows[i]->key = lst[i];
    cmplst->rows[i]->colcount = 0;
    it++;
  }
  return cmplst;
}

static fn_compl* compl_win()
{
  return 0;
}

static fn_compl* compl_cntlr(String line)
{
  log_msg("COMPL", "compl_cntlr");
  fn_compl *cmplst = malloc(sizeof(fn_compl));
  String *lst = cntlr_list(line);

  int count;
  for (count = 0; lst[count] != NULL; count++) {}
  cmplst->rowcount = count;

  cmplst->rows = malloc(count*sizeof(compl_item));

  String it = lst[0];
  for (int i = 0; i < count; i++) {
    cmplst->rows[i] = malloc(sizeof(compl_item));
    cmplst->rows[i]->key = lst[i];
    cmplst->rows[i]->colcount = 0;
    it++;
  }
  return cmplst;
}
