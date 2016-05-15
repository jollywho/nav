#ifndef FN_EVENT_HOOK_H
#define FN_EVENT_HOOK_H

#include "nav/plugins/plugin.h"

typedef struct {
  Keyarg *ka;
  void *arg;
} HookArg;

typedef void (*hook_cb)(Plugin *host, Plugin *caller, HookArg *hka);

void send_hook_msg(char *msg, Plugin *host, Plugin *caller, HookArg *hka);
void hook_init();
void hook_cleanup();
void hook_init_host(Plugin *host);
void hook_cleanup_host(Plugin *host);

void hook_group_add(char *key);
void hook_group_remove(char *key);
void hook_add(char *event, char *pattern, char *cmd);
void hook_remove(char *event, char *pattern);
char* isgroup(char *key);
char* isevent(char *key);

void hook_add_intl(Plugin *host, Plugin *caller, hook_cb fn, char *msg);
void hook_rm_intl(Plugin *host, Plugin *caller, hook_cb fn, char *msg);
void hook_set_tmp(char *msg);
void hook_clear_msg(Plugin *host, char *msg);
void hook_clear_host(Plugin *host);

void event_list(List *args);
void augs_list(List *args);

#endif
