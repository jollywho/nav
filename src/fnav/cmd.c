#include "fnav/cmdline.h"
#include "fnav/cmd.h"
#include "fnav/log.h"

Cmd_T *cmd_table;

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

void cmd_run(Cmdstr *cmdstr)
{
  log_msg("CMD", "cmd_run");
  Token *word = NULL;

  List *args = TOKEN_LIST(cmdstr);
  if (utarray_len(args->items) < 1) return;

  word = (Token*)utarray_front(args->items);

  Cmd_T *fun = cmd_find(TOKEN_STR(word->var));
  if (!fun) return; // :'(

  cmdstr->ret_t = CNTLR;
  cmdstr->ret = fun->cmd_func(args, fun->flags);
}
