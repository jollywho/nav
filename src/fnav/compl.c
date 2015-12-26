#include "fnav/compl.h"
#include "fnav/log.h"

#define HASH_INS(t,obj) \
  HASH_ADD_KEYPTR(hh, t, obj.key, strlen(obj.key), &obj);

typedef struct {
  String name;
  char v_type;
  String comp;
  int flag;
} fn_context_arg;

typedef struct {
  String key;
  int argc;
  fn_context_arg **argv;
  UT_hash_handle hh;
} fn_context;

static compl_list* compl_win();

#define DEFAULT_SIZE ARRAY_SIZE(compl_defaults)
static compl_entry compl_defaults[] = {
  { "win",     compl_win     },
};

compl_entry *compl_table;
fn_context *context_root;

void compl_init()
{
  for (int i = 0; i < (int)DEFAULT_SIZE; i++) {
    HASH_INS(compl_table, compl_defaults[i]);
  }
}

compl_list* compl_of(String name)
{
  compl_entry *find;
  HASH_FIND_STR(compl_table, name, find);
  if (!find) return NULL;
  return find->gen();
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

static int compl_param(fn_context_arg **arg, String param)
{
  log_msg("COMPL", "compl_param %s", param);

  if (param[0] == '*') {
    (*arg) = malloc(sizeof(fn_context_arg));
    (*arg)->flag = 1;
    return 1;
  }

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

  (*arg) = malloc(sizeof(fn_context_arg));
  (*arg)->name = name;
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
  fn_context_arg **args = malloc(grpc*sizeof(fn_context_arg));

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
  cx->argv = args;
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

static compl_list* compl_win()
{
  return 0;
}
