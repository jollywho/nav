#include "fnav/cmdline.h"
#include "fnav/cmd.h"
#include "fnav/log.h"
#include "fnav/compl.h"

Cmd_T *cmd_table;

int name_sort(Cmd_T *a, Cmd_T *b)
{
  return strcmp(a->name, b->name);
}

void cmd_add(Cmd_T *cmd)
{
  HASH_ADD_KEYPTR(hh, cmd_table, cmd->name, strlen(cmd->name), cmd);
  HASH_SORT(cmd_table, name_sort);
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

void cmd_list(String line)
{
  log_msg("CMD", "compl cmd_list");
  unsigned int count = HASH_COUNT(cmd_table);
  compl_new(count, COMPL_STATIC);
  Cmd_T *it;
  int i = 0;
  for (it = cmd_table; it != NULL; it = it->hh.next) {
    compl_set_index(i, it->name, 0, NULL);
    i++;
  }
}
