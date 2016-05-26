#include "nav/tui/window.h"
#include "nav/plugins/op/op.h"
#include "nav/expand.h"
#include "nav/log.h"
#include "nav/util.h"
#include "nav/option.h"
#include "nav/table.h"
#include "nav/model.h"
#include "nav/event/fs.h"
#include "nav/tui/select.h"

enum fm_fmt {
  FM_NORM,
  FM_EXT,
  FM_NAME,
  FM_GROUP,
};

enum fs_fmt {
  FS_KIND,
};

static char* bu_type(char ch)
{
  if (ch == 'b') {
    char *pidstr;
    asprintf(&pidstr, "%d", buf_id(window_get_focus()));
    return pidstr;
  }
  if (ch == 'B') {
    Plugin *plug = window_get_plugin();
    return strdup(plug->name);
  }
  return NULL;
}

static char* fs_type(const char *name, enum fs_fmt fmt)
{
  Buffer *buf = window_get_focus();
  Model *m = buf->hndl->model;
  if (!buf || strcmp(buf->plugin->name, "fm"))
    return NULL;

  if (buf_sel_count(buf) < 2) {
    struct stat *sb = model_curs_value(m, "stat");
    return strdup(stat_type(sb));
  }

  int count = 0;
  for (int i = 0; i < model_count(m); ++i) {
    if (select_has_line(buf, i))
      count++;
  }

  int j = 0;
  char **str = malloc((1+count)*sizeof(char*));
  for (int i = 0; i < model_count(m); ++i) {
    if (select_has_line(buf, i)) {
      struct stat *sb = model_fld_line(m, "stat", i);
      str[j++] = strdup(stat_type(sb));
    }
  }
  char *src = lines2argv(count, str);
  del_param_list(str, count);
  return src;
}

static char* fm_type(const char *name, enum fm_fmt fmt)
{
  Buffer *buf = window_get_focus();
  if (!buf || strcmp(buf->plugin->name, "fm"))
    return NULL;

  varg_T args = buf_focus_sel(buf, name);

  for (int i = 0; i < args.argc; i++) {
    if (fmt == FM_EXT || fmt == FM_GROUP)  {
      const char *it = file_ext(args.argv[i]);
      if (fmt == FM_GROUP) {
        fn_syn *syn = get_syn(it);
        if (syn)
          it = syn->group->key;
        else
          it = "";
      }
      SWAP_ALLOC_PTR(args.argv[i], strdup(it));
    }
    else if (fmt == FM_NAME)
      file_base(args.argv[i]);
  }

  char *src = lines2argv(args.argc, args.argv);
  del_param_list(args.argv, args.argc);
  return src;
}

static char* op_type(const char *name)
{
  fn_group *grp = op_active_group();
  if (!name || !grp)
    return NULL;

  int isfld = !strcmp(name, "prev") || !strcmp(name, "next");
  ventry *vent = fnd_val("op_procs", "group", grp->key);

  if (isfld && vent) {
    ventry *head = ent_head(vent);
    char *ret = rec_fld(head->rec, "pid");
    return strdup(ret);
  }

  return NULL;
}

static char* get_type(const char *key, const char *alt)
{
  switch (*key) {
    case 'b':
    case 'B':
      return bu_type(*key);
    case 'f':
      return fm_type("fullpath", FM_NORM);
    case 'n':
      return fm_type("name",     FM_NORM);
    case 'N':
      return fm_type("name",     FM_NAME);
    case 'e':
      return fm_type("name",     FM_EXT);
    case 'd':
      return fm_type("dir",      FM_NORM);
    case 't':
      return fm_type("name",     FM_GROUP);
    case 'k':
      return fs_type("stat",     FS_KIND);
    case 'o':
      return op_type(alt);
    default:
      return NULL;
  }
}

char* expand_symbol(const char *key, const char *alt)
{
  log_msg("EXPAND", "expand symb {%s:%s}", key, alt);

  char *var = get_type(key, alt);
  if (!var)
    var = strdup("''");
  return var;
}
