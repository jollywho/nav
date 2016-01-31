#include "fnav/compl.h"
#include "fnav/plugins/plugin.h"
#include "fnav/log.h"
#include "fnav/cmd.h"
#include "fnav/tui/menu.h"
#include "fnav/table.h"

typedef struct {
  String key;
  fn_context *cx;
  UT_hash_handle hh;
} fn_context_arg;

static compl_entry compl_defaults[] = {
  { "cmds",    cmd_list      },
  { "plugins", plugin_list   },
  { "fields",  field_list    },
  { "paths",   path_list     },
  { "marks",   mark_list     },
  { "marklbls",marklbl_list  },
};

static compl_entry *compl_table;
static fn_context *cxroot;
static fn_context *cxtbl;
static fn_context_arg *cxargs;
static fn_context *cur_cx;
static int compl_param(fn_context **arg, String param);
static int count_subgrps(String str, String fnd);

void compl_init()
{
  cur_cx = NULL;
  String param;
  param = strdup("cmd:string:cmds");
  compl_param(&cxroot, param);
  free(param);

  for (int i = 0; i < LENGTH(compl_defaults); i++) {
    compl_entry *it = malloc(sizeof(compl_entry));
    it = memmove(it, &compl_defaults[i], sizeof(compl_entry));
    HASH_ADD_STR(compl_table, key, it);
  }
}

void compl_cleanup()
{
  // each context in cxroot should be also linked to a flat array
  // iterate array
  // free it strings
  // compl_destroy(it->cmpl)
  // clear it->sub
  // free it->params
  // free it
  //
  // clear entries in compl_table
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

  (*arg) = calloc(1, sizeof(fn_context));
  (*arg)->key = strdup(name);
  (*arg)->type = strdup(type);
  (*arg)->comp = strdup(comp);
  (*arg)->cmpl = NULL;
  return 1;
}

static void compl_add(String fmt_compl, fn_context **parent)
{
  log_msg("COMPL", "compl_add");
  log_msg("COMPL", "%s", fmt_compl);
  int pidx = 0;
  int grpc = count_subgrps(fmt_compl, ";");
  if (grpc < 1) {
    log_msg("COMPL", "invalid format");
    return;
  }

  String line = strdup(fmt_compl);
  fn_context *cx = malloc(sizeof(fn_context));
  fn_context **args = malloc(grpc*sizeof(fn_context*));

  String keyptr;
  String saveptr;
  String lhs = strtok_r(line, ";", &saveptr);
  fn_context *find;

  keyptr = strdup(lhs);

  String param = strtok_r(NULL, ";", &saveptr);
  for (pidx = 0; pidx < grpc; pidx++) {
    if (compl_param(&args[pidx], param) == -1)
      goto breakout;
    param = strtok_r(NULL, ";", &saveptr);
  }

  find = NULL;
  cx->key = keyptr;
  cx->argc = grpc;
  cx->params = args;
  cx->cmpl = NULL;

  HASH_FIND_STR(*parent, keyptr, find);
  if (find) {
    log_msg("COMPL", "ERROR: context already defined");
    goto breakout;
  }
  else {
    log_msg("*|*", "%s %s %s", cx->key, args[0]->key, keyptr);
    HASH_ADD_STR(*parent, key, cx);
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

void compl_add_context(String fmt_compl)
{
  log_msg("COMPL", "compl_add_context");
  compl_add(fmt_compl, &cxtbl);
}

void compl_add_arg(String name, String fmt_compl)
{
  log_msg("COMPL", "compl_add_arg");
  fn_context_arg *find;
  HASH_FIND_STR(cxargs, name, find);
  if (!find) {
    fn_context_arg *cxa = calloc(1, sizeof(fn_context_arg));
    cxa->key = strdup(name);
    HASH_ADD_STR(cxargs, key, cxa);
    find = cxa;
  }

  compl_add(fmt_compl, &find->cx);
}

fn_context* find_context(fn_context *cx, String name)
{
  log_msg("COMPL", "find_context");
  if (!cx || !name) {
    log_msg("COMPL", "not available.");
    return NULL;
  }
  if (cx == cxroot)
    cx = cxtbl;

  if (cx->argc > 0) {
    fn_context *find;
    HASH_FIND_STR(cx, name, find);
    if (find) {
      log_msg("COMPL", "::found %s %s", name, find->key);
      return find;
    }
  }

  fn_context_arg *find_arg;
  HASH_FIND_STR(cxargs, cx->key, find_arg);
  if (!find_arg)
    return NULL;

  fn_context *find;
  HASH_FIND_STR(find_arg->cx, name, find);
  if (!find)
    return NULL;
  return find;

  //TODO: else find next param
}

void compl_build(fn_context *cx, List *args)
{
  log_msg("COMPL", "compl_build");
  if (!cx)
    return;
  String key = cx->comp;
  log_msg("COMPL", "%s", key);
  compl_entry *find;
  HASH_FIND_STR(compl_table, key, find);
  if (!find) {
    cx->cmpl = NULL;
    return;
  }

  cur_cx = cx;
  find->gen(args);
}

fn_context* context_start()
{
  return cxroot;
}

void compl_update(fn_context *cx, String line)
{
  log_msg("COMPL", "compl_update");
  if (!cx || !line || !cx->cmpl)
    return;

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
  compl_destroy(cur_cx);
  cur_cx->cmpl = malloc(sizeof(fn_compl));
  cur_cx->cmpl->rows    = malloc(size*sizeof(compl_item));
  cur_cx->cmpl->matches = malloc(size*sizeof(compl_item));
  cur_cx->cmpl->rowcount = size;
  cur_cx->cmpl->matchcount = 0;
  cur_cx->cmpl->comp_type = dynamic;
}

void compl_delete(fn_compl *cmpl)
{
  log_msg("COMPL", "compl_delete");
  if (!cmpl)
    return;
  for (int i = 0; i < cmpl->rowcount; i++)
    free(cmpl->rows[i]);

  free(cmpl->rows);
  free(cmpl->matches);
  free(cmpl);
}

void compl_destroy(fn_context *cx)
{
  log_msg("COMPL", "compl_destroy");
  if (!cx)
    return;
  compl_delete(cx->cmpl);
  cx->cmpl = NULL;
}

void compl_set_index(int idx, String key, int colcount, String cols)
{
  fn_compl *cmpl = cur_cx->cmpl;
  cmpl->rows[idx] = malloc(sizeof(compl_item));
  cmpl->rows[idx]->key = key;
  cmpl->rows[idx]->colcount = colcount;
  cmpl->rows[idx]->columns = cols;
}

static int count_subgrps(String str, String fnd)
{
  int count = 0;
  const char *tmp = str;
  while((tmp = strstr(tmp, fnd))) {
    count++;
    tmp++;
  }

  return count;
}

bool compl_isdynamic(fn_context *cx)
{
  if (!cx || !cx->cmpl)
    return false;
  return cx->cmpl->comp_type == COMPL_DYNAMIC;
}
