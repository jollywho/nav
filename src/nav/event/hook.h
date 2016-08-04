#ifndef NV_EVENT_HOOK_H
#define NV_EVENT_HOOK_H

#include "nav/plugins/plugin.h"

typedef struct {
  Keyarg *ka;
  void *arg;
} HookArg;

typedef void (*hook_cb)(Plugin *host, Plugin *caller, HookArg *hka);

enum nv_event {
  EVENT_CURSOR_CHANGE,
  EVENT_DIROPEN,
  EVENT_EXEC_CLOSE,
  EVENT_EXEC_OPEN,
  EVENT_FILEOPEN,
  EVENT_JUMP,
  EVENT_LEFT,
  EVENT_OPEN,
  EVENT_OPTION_SET,
  EVENT_PASTE,
  EVENT_PIPE,
  EVENT_PIPE_REMOVE,
  EVENT_REMOVE,
  EVENT_RIGHT,
  EVENT_WINDOW_RESIZE,
};

void send_hook_msg(int event, Plugin *host, Plugin *caller, HookArg *hka);
void hook_init();
void hook_cleanup();
void hook_clear_host(int);

int event_idx(char *key);
void augroup_add(char *key);
void augroup_remove(char *key);
void hook_add(char *group, char *event, char *pattern, char *expr, int id);
void hook_remove(char *group, char *event, char *pattern);
char* hook_group_name(char *key);
char* hook_event_name(char *key);

void hook_add_intl(int id, Plugin *, Plugin *, hook_cb, int event);
void hook_rm_intl(Plugin *, Plugin *, hook_cb, int event);
void hook_set_tmp(int id);
void hook_clear_msg(Plugin *, char *msg);

void event_list();
void augs_list();

#endif
