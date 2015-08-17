/*
*  rpc.c - rpc method example
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct cmd_ret {
  int ret_num;
  char **ret;
} cmd_ret_t;

typedef struct command {
  char *command;
  int required_parameters;
  void (*execute)(cmd_ret_t *, char **args);
  int flags;
} command_t;

void ipc_cmd_clist(cmd_ret_t *data, char **cmd)
{
  printf("called clist\n");
}

void ipc_cmd_attr(cmd_ret_t *data, char **cmd)
{
  printf("/*get attribute for each id*/\n");
  printf("%s(id_list, attr)\n", cmd[0]);
  printf("%s\n", cmd[1]);
  printf("%s\n", cmd[2]);

  /* net would ret with data set */
  int ret_num = 2;
  cmd_ret_t *d = (cmd_ret_t*)malloc(sizeof(cmd_ret_t));
  d->ret = calloc(ret_num, sizeof(char*));
  d->ret[0] = "2015-08-17 01:37:22.771456653";
  d->ret[1] = "2015-08-17 03:55:21.699846242";

  *data = *d;
}

static const command_t ipc_commands[] = {
  { "clist",  0,  ipc_cmd_clist,  0 },
  { "attr",   2,  ipc_cmd_attr,   0 }
};

static void ipc_command_exec(cmd_ret_t *data, char **cmd, const command_t *commands)
{
  int i, j;

  if (!cmd[0]) {
    return;
  }
  for (i = 0; commands[i].command; i++) {
    if (strcasecmp(commands[i].command, cmd[0]) != 0) {
      continue;
    }
    for (j = 1; cmd[j]; j++) {} /*count*/
    j--;
    if (j >= commands[i].required_parameters) {
      commands[i].execute(data, cmd);
      break;
    }
  }
}

int main()
{
  char **cmd_name;
  cmd_name = malloc(sizeof(char*));
  cmd_name[0] = "clist";
  cmd_ret_t *data;
  ipc_command_exec(data, cmd_name, ipc_commands);

  cmd_name[0] = "attr";
  cmd_name[1] = "cmd[1]=\"2345 2583\"";
  cmd_name[2] = "cmd[2]=ModifyDate";
  ipc_command_exec(data, cmd_name, ipc_commands);
  printf("ret1: %s\n", data->ret[0]);
  printf("ret2: %s\n", data->ret[1]);
}
