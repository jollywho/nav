#ifndef FN_TUI_plugin_H
#define FN_TUI_plugin_H

#include "fnav/fnav.h"
#include "fnav/config.h"

typedef void (*argv_callback)(void **argv);
typedef struct Window Window;
typedef struct Plugin Plugin;
typedef struct fn_tbl fn_tbl;
typedef struct Buffer Buffer;
typedef struct fn_handle fn_handle;
typedef struct Model Model;
typedef struct HookHandler HookHandler;
typedef struct Overlay Overlay;
typedef struct fn_reg fn_reg;
typedef struct Cmdarg Cmdarg;

typedef Plugin* (*plugin_open_cb)(Buffer *b);
typedef void (*plugin_close_cb)(Plugin *plugin);

struct fn_handle {
  Buffer *buf;
  Model *model;
  String tn;      // listening table name
  String key;     // listening value
  String key_fld; // listening field
  String fname;   // filter field
};

struct Plugin {
  int id;
  String name;
  String fmt_name;
  String compl_key;
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

void plugin_load(String name, plugin_open_cb open_cb, plugin_close_cb close_cb);
int plugin_isloaded(String name);

Plugin* plugin_open(String name, Buffer *buf);
void plugin_close(Plugin *plugin);

Plugin* focus_plugin();
Plugin* plugin_from_id(int id);
void plugin_pipe(Plugin *plugin);

void plugin_list(List *args);

void fm_mark_dir(Plugin *plugin, String label);
void mark_list(List *args);
void marklbl_list(List *args);
String* mark_info_save();

#endif
