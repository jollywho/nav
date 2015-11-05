#include "fnav/cmdline.h"
#include "fnav/cmd.h"

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

void cmd_run(Cmd_T *cmd)
{
}
