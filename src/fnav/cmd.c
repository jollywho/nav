#include "fnav/cmdline.h"
#include "fnav/cmd.h"
#include "fnav/log.h"

Cmd_T *cmd_table;
static UT_icd cmd_icd = { sizeof(Cmd_T),   NULL };

void cmd_add(Cmd_T *cmd)
{
  HASH_ADD_KEYPTR(hh, cmd_table, cmd->name, strlen(cmd->name), cmd);
}

void cmd_remove(String name)
{
}

Cmd_T* cmd_find(String name)
{
  Cmd_T *cmd;
  HASH_FIND_STR(cmd_table, name, cmd);
  return cmd;
}

#define TOKEN_LIST(token) \
  token->args.var.vval.v_list

#define TOKEN_STR(token) \
  token->var.vval.v_string

void cmd_run(Cmdstr *cmdstr)
{
  log_msg("CMD", "cmd_run");
  Token *word, *func = NULL;
  List *args = TOKEN_LIST(cmdstr);
  if (utarray_len(args->items) < 1) return;

  word = func = (Token*)utarray_front(args->items);
  Cmd_T *fun = cmd_find(TOKEN_STR(word));
  if (!fun) return; // :'(

  while((word = (Token*)utarray_next(args->items, word))) {
    //TODO: validate params vs tokens
  }
  word = (Token*)utarray_next(args->items, func);
  fun->cmd_func(word, fun->cmd_flags);
}
