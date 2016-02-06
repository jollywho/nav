#include "fnav/cmdline.h"
#include "fnav/cmd.h"
#include "fnav/log.h"
#include "fnav/compl.h"
#include "fnav/table.h"

Cmd_T *cmd_table;

void cmd_clearall()
{
  log_msg("CMD", "cmd_clearall");
  Cmd_T *it, *tmp;
  HASH_ITER(hh, cmd_table, it, tmp) {
    HASH_DEL(cmd_table, it);
    free(it);
  }
}

int name_sort(Cmd_T *a, Cmd_T *b)
{
  return strcmp(a->name, b->name);
}

void cmd_add(Cmd_T *cmd)
{
  HASH_ADD_STR(cmd_table, name, cmd);
  HASH_SORT(cmd_table, name_sort);
}

void cmd_remove(String name)
{
}

Cmd_T* cmd_find(String name)
{
  Cmd_T *cmd;
  if (!name)
    return NULL;

  HASH_FIND_STR(cmd_table, name, cmd);
  return cmd;
}

void cmd_run(Cmdstr *cmdstr)
{
  log_msg("CMD", "cmd_run");
  List *args = token_val(&cmdstr->args, VAR_LIST);
  if (utarray_len(args->items) < 1)
    return;

  Cmd_T *fun = cmd_find(list_arg(args, 0, VAR_STRING));
  if (!fun)
    return;

  cmdstr->ret_t = PLUGIN;
  cmdstr->ret = fun->cmd_func(args, fun->flags);
}

void cmd_list(List *args)
{
  log_msg("CMD", "compl cmd_list");
  int i = 0;
  Cmd_T *it;
  compl_new(HASH_COUNT(cmd_table), COMPL_STATIC);
  for (it = cmd_table; it != NULL; it = it->hh.next) {
    compl_set_index(i, 0, NULL, "%s", it->name);
    i++;
  }
}
