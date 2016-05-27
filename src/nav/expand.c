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
  FM_KIND,
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

static char* fm_ext(void *arg)
{
  return strdup(file_ext(arg));
}

static char* fm_name(void *arg)
{
  return (char*)file_base(strdup(arg));
}

static char* fm_group(void *arg)
{
  fn_syn *syn = get_syn(file_ext(arg));
  if (syn)
    return strdup(syn->group->key);
  else
    return strdup("");
}

static char* fm_kind(void *arg)
{
  struct stat *sb = arg;
  return strdup(stat_type(sb));
}

static char* fm_type(const char *name, enum fm_fmt fmt)
{
  Buffer *buf = window_get_focus();
  if (!buf || strcmp(buf->plugin->name, "fm"))
    return NULL;

  select_cb cb = NULL;
  switch (fmt) {
    case FM_EXT:   cb = fm_ext;   break;
    case FM_NAME:  cb = fm_name;  break;
    case FM_GROUP: cb = fm_group; break;
    case FM_KIND:  cb = fm_kind;  break;
    default:
      break;
  }
  varg_T args = buf_select(buf, name, cb);

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
      return fm_type("stat",     FM_KIND);
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
