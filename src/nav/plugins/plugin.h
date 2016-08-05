#ifndef NV_PLUGINS_PLUGIN_H
#define NV_PLUGINS_PLUGIN_H

#include "nav/nav.h"
#include "nav/config.h"

typedef struct nv_option nv_option;
enum opt_type { OPTION_STRING, OPTION_INT, OPTION_UINT, OPTION_BOOLEAN };

typedef struct Window Window;
typedef struct Plugin Plugin;
typedef struct Table Table;
typedef struct Buffer Buffer;
typedef struct Handle Handle;
typedef struct Model Model;
typedef struct Overlay Overlay;
typedef struct nv_reg nv_reg;
typedef struct Keyarg Keyarg;

typedef void (*plugin_init_cb)(void);
typedef void (*plugin_open_cb)(Plugin *base, Buffer *b, char *arg);
typedef void (*plugin_close_cb)(Plugin *plugin);

struct Handle {
  Buffer *buf;
  Model *model;
  char *tn;      // listening table name
  char *key;     // listening value
  char *key_fld; // listening field
  char *fname;   // filter field
  char *kname;   // filter field
};

struct Plugin {
  char *name;
  char *fmt_name;
  char *compl_key;
  Handle *hndl;
  nv_option *opts;
  void *top;
  void (*_cancel)(Plugin *plugin);
  void (*_focus)(Plugin *plugin);
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

void obtain_id(Buffer *buf);
void forefit_id(Buffer *buf);

int plugin_open(const char *name, Buffer *buf, char *line);
void plugin_close(Plugin *plugin);

nv_option* local_opts();
Plugin* focus_plugin();
Buffer* buf_from_id(int id);
Plugin* plugin_from_id(int id);
int id_from_plugin(Plugin *plug);
int plugin_requires_buf(const char *);

void plugin_list();
void win_list();
void type_list();

#endif
