#include "fnav/compl.h"
#include "fnav/tui/cntlr.h"
#include "fnav/log.h"

#include "fnav/cmd.h"
#include "fnav/tui/cntlr.h"

#define HASH_INS(t,obj) \
  HASH_ADD_KEYPTR(hh, t, obj.key, strlen(obj.key), &obj);

#define DEFAULT_SIZE ARRAY_SIZE(compl_defaults)
static compl_entry compl_defaults[] = {
  { "cmds",    cmd_list      },
  { "cntlrs",  cntlr_list    },
};

static compl_entry *compl_table;
static fn_context *context_root;
static fn_compl *cur_cmpl;

void compl_init()
{
  cur_cmpl = NULL;
  compl_add_context("_;cmd:string:cmds");
  fn_context *find;
  HASH_FIND_STR(context_root, "_", find);
  context_root = find;

  for (int i = 0; i < (int)DEFAULT_SIZE; i++) {
    HASH_INS(compl_table, compl_defaults[i]);
  }
}

void compl_build(fn_context *cx, String line)
{
  String key = cx->params[0]->comp;
  compl_entry *find;
  HASH_FIND_STR(compl_table, key, find);
  if (!find) return;
  find->gen(line);
  if (cur_cmpl) {
    cx->cmpl = cur_cmpl;
    cur_cmpl = NULL;
  }
}

fn_context* context_start()
{
  return context_root;
}

void compl_new(int size)
{
  cur_cmpl = malloc(sizeof(fn_compl));
  cur_cmpl->rows = malloc(size*sizeof(compl_item));
  cur_cmpl->rowcount = size;
}

void compl_set_index(int idx, String key, int colcount, String *cols)
{
  cur_cmpl->rows[idx] = malloc(sizeof(compl_item));
  cur_cmpl->rows[idx]->key = key;
  cur_cmpl->rows[idx]->colcount = colcount;
  cur_cmpl->rows[idx]->columns = cols;
}

static int count_subgrps(String str, String fnd)
{
  int count = 0;
  const char *tmp = str;
  while((tmp = strstr(tmp, fnd))) {
    count++; tmp++;
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
