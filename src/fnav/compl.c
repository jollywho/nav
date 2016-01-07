#include "fnav/compl.h"
#include "fnav/tui/cntlr.h"
#include "fnav/log.h"

#include "fnav/cmd.h"
#include "fnav/tui/cntlr.h"
#include "fnav/table.h"

#define DEFAULT_SIZE ARRAY_SIZE(compl_defaults)
static compl_entry compl_defaults[] = {
  { "cmds",    cmd_list      },
  { "cntlrs",  cntlr_list    },
  { "fields",  field_list    },
};

static compl_entry *compl_table;
static fn_context *cxroot;
static fn_compl *cur_cmpl;
static int compl_param(fn_context **arg, String param);
static int count_subgrps(String str, String fnd);

void compl_init()
{
  cur_cmpl = NULL;
  String param = strdup("cmd:string:cmds");
  compl_param(&cxroot, param);

  for (int i = 0; i < (int)DEFAULT_SIZE; i++) {
    compl_entry *it = malloc(sizeof(compl_entry));
    it = memcpy(it, &compl_defaults[i], sizeof(compl_entry));
    HASH_ADD_STR(compl_table, key, it);
  }
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
  (*arg)->key = strdup(name);
  (*arg)->type = strdup(type);
  (*arg)->comp = strdup(comp);
  (*arg)->sub = NULL;
  (*arg)->cmpl = NULL;
  return 1;
}

void compl_add_context(String fmt_compl)
{
  log_msg("COMPL", "compl_add_context");
  log_msg("COMPL", "%s", fmt_compl);
  int pidx = 0;
  int grpc = count_subgrps(fmt_compl, ";");

  String line = strdup(fmt_compl);
  fn_context *cx = malloc(sizeof(fn_context));
  fn_context **args = malloc(grpc*sizeof(fn_context*));
  fn_context *parent;

  String keyptr;
  if (grpc < 1) {
    log_msg("COMPL", "invalid format");
    return;
  }
  else {
    String saveptr;
    String lhs = strtok_r(line, ";", &saveptr);
    String name = strtok(lhs, ":");

    String wildch = strtok(NULL, ":");
    //FIXME: wildch should attach context to all matches
    if (wildch[0] == '*') {
      name = strtok(NULL, ":");
      keyptr = strdup(strtok(NULL, ":"));
    }
    else
      keyptr = strdup(wildch);

    parent = find_context(cxroot, name);

    String param = strtok_r(NULL, ";", &saveptr);
    for (pidx = 0; pidx < grpc; pidx++) {
      if (compl_param(&args[pidx], param) == -1)
        goto breakout;
      param = strtok_r(NULL, ";", &saveptr);
    }
  }

  fn_context *find;
  cx->key = keyptr;
  cx->argc = grpc;
  cx->params = args;
  cx->sub = NULL;
  cx->cmpl = NULL;

  HASH_FIND_STR((parent)->sub, keyptr, find);
  if (find) {
    log_msg("COMPL", "ERROR: context already defined");
    goto breakout;
  }
  else {
    log_msg("*|*", "%s %s %s", cx->key, args[0]->key, keyptr);
    for (int i = 0; i < grpc; i++) {
      HASH_ADD_STR(cx->sub, key, args[i]);
    }
    log_msg("*|*", "%s %s %s", cx->key, args[0]->key, keyptr);
    HASH_ADD_STR((parent)->sub, key, cx);
  }
  free(line);

  return;
breakout:
  log_msg("**", "CLEANUP");
  for (pidx = pidx - 1; pidx > 0; pidx--) {
    free(args[pidx]);
  }
  free(cx);
  free(args);
  free(line);
  return;
}

fn_context* find_context(fn_context *cx, String name)
{
  log_msg("COMPL", "find_context");
  if (!(cx)) {
    log_msg("COMPL", "not available.");
    return NULL;
  }
  log_msg("COMPL", "find_context %s %s", cx->key, name);
  if (strcmp(name, "cmd") == 0) {
    log_msg("COMPL", "::found root");
    return cx;
  }

  fn_context *find;
  HASH_FIND_STR(cx->sub, name, find);
  if (find) {
    log_msg("COMPL", "::found %s %s", name, find->key);
    return find;
  }
  else {
    fn_context *it;
    for (it = (cx)->sub; it != NULL; it = it->hh.next) {
      log_msg("COMPL", "::");
      fn_context *ret = find_context(it, name);
      if (ret)
        return ret;
    }
    log_msg("COMPL", "not found.");
    return NULL;
  }
}

void compl_build(fn_context *cx, String line)
{
  log_msg("COMPL", "compl_build");
  if (!cx) return;
  String key = cx->comp;
  log_msg("COMPL", "%s", key);
  compl_entry *find;
  HASH_FIND_STR(compl_table, key, find);
  if (!find) {
    cx->cmpl = NULL;
    return;
  }

  find->gen(line);

  if (cur_cmpl) {
    cx->cmpl = cur_cmpl;
  }
  cur_cmpl = NULL;
}

void compl_destroy(fn_context *cx)
{
  log_msg("COMPL", "compl_destroy");
  if (!cx) return;
  compl_delete(cx->cmpl);
  cx->cmpl = NULL;
}

fn_context* context_start()
{
  return cxroot;
}

void compl_update(fn_context *cx, String line)
{
  log_msg("COMPL", "compl_update");
  if (!cx) return;
  if (!cx->cmpl) return;
  log_msg("COMPL", "%s", line);

  fn_compl *cmpl = cx->cmpl;
  cmpl->matchcount = 0;

  for (int i = 0; i < cmpl->rowcount; i++) {
    String key = cmpl->rows[i]->key;
    if (strstr(key, line)) {
      cmpl->matches[cmpl->matchcount] = cmpl->rows[i];
      cmpl->matchcount++;
    }
    else
      cmpl->matches[i] = NULL;
  }
}

void compl_new(int size, int dynamic)
{
  log_msg("COMPL", "compl_new");
  cur_cmpl = malloc(sizeof(fn_compl));
  cur_cmpl->rows    = malloc(size*sizeof(compl_item));
  cur_cmpl->matches = malloc(size*sizeof(compl_item));
  cur_cmpl->rowcount = size;
  cur_cmpl->matchcount = 0;
  cur_cmpl->comp_type = dynamic;
}

void compl_delete(fn_compl *cmpl)
{
  if (!cmpl) return;
  for (int i = 0; i < cmpl->rowcount; i++) {
    free(cmpl->rows[i]);
  }
  free(cmpl->rows);
  free(cmpl->matches);
  free(cmpl);
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
