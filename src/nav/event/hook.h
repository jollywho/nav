#ifndef NV_EVENT_HOOK_H
#define NV_EVENT_HOOK_H

#include "nav/plugins/plugin.h"

typedef struct {
  Keyarg *ka;
  void *arg;
} HookArg;

typedef void (*hook_cb)(Plugin *host, Plugin *caller, HookArg *hka);

void send_hook_msg(char *msg, Plugin *host, Plugin *caller, HookArg *hka);
void hook_init();
void hook_cleanup();
void hook_clear_host(int);

void augroup_add(char *key);
void augroup_remove(char *key);
void hook_add(char *group, char *event, char *pattern, char *expr, int id);
void hook_remove(char *group, char *event, char *pattern);
char* hook_group_name(char *key);
char* hook_event_name(char *key);

void hook_add_intl(int id, Plugin *host, Plugin *caller, hook_cb fn, char *msg);
void hook_rm_intl(Plugin *host, Plugin *caller, hook_cb fn, char *msg);
void hook_set_tmp(char *msg);
void hook_clear_msg(Plugin *host, char *msg);

void event_list();
void augs_list();

#endif
