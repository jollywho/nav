#ifndef FN_PLUGINS_PLUGIN_H
#define FN_PLUGINS_PLUGIN_H

#include "nav/nav.h"
#include "nav/config.h"

typedef struct Window Window;
typedef struct Plugin Plugin;
typedef struct fn_tbl fn_tbl;
typedef struct Buffer Buffer;
typedef struct fn_handle fn_handle;
typedef struct Model Model;
typedef struct HookHandler HookHandler;
typedef struct Overlay Overlay;
typedef struct fn_reg fn_reg;
typedef struct Keyarg Keyarg;

typedef void (*plugin_init_cb)(void);
typedef void (*plugin_open_cb)(Plugin *base, Buffer *b, char *arg);
typedef void (*plugin_close_cb)(Plugin *plugin);

struct fn_handle {
  Buffer *buf;
  Model *model;
  char *tn;      // listening table name
  char *key;     // listening value
  char *key_fld; // listening field
  char *fname;   // filter field
  char *kname;   // filter field
};

struct Plugin {
  int id;
  char *name;
  char *fmt_name;
  char *compl_key;
  fn_handle *hndl;
  void *top;
  void (*_cancel)(Plugin *plugin);
  void (*_focus)(Plugin *plugin);
  HookHandler *event_hooks;
};

typedef struct {
  int lnum;    /* line number */
  int col;     /* column number */
} pos_T;

typedef struct {
  int argc;
  char **argv;
} varg_T;

void plugin_init();
void plugin_cleanup();
int plugin_isloaded(const char *);

int plugin_open(const char *name, Buffer *buf, char *line);
void plugin_close(Plugin *plugin);

Plugin* focus_plugin();
Plugin* plugin_from_id(int id);
int plugin_requires_buf(const char *);
char *focus_dir();

void plugin_list(List *args);
void win_list(List *args);

#endif
