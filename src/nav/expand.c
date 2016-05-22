#include "nav/tui/window.h"
#include "nav/plugins/op/op.h"
#include "nav/expand.h"
#include "nav/log.h"
#include "nav/util.h"
#include "nav/option.h"
#include "nav/table.h"

enum fm_fmt {
  FM_NORM,
  FM_EXT,
  FM_NAME,
  FM_GROUP,
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
  return strdup("");
}

static char* fm_type(const char *name, enum fm_fmt fmt)
{
  log_msg("EXPAND", "fm_type");
  Buffer *buf = window_get_focus();
  if (!buf)
    return strdup("");

  varg_T args = buf_focus_sel(buf, name);

  //TODO: format args by type

  char *src = lines2argv(args.argc, args.argv);
  log_msg("FM", "%s", src);
  del_param_list(args.argv, args.argc);

  if (src)
    return src;

  return strdup("");
}

static char* op_type(const char *name)
{
  log_msg("EXPAND", "op_type");
  fn_group *grp = op_active_group();
  if (!name || !grp)
    return strdup("");

  int isfld = !strcmp(name, "prev") || !strcmp(name, "next");
  ventry *vent = fnd_val("op_procs", "group", grp->key);

  if (isfld && vent) {
    ventry *head = ent_head(vent);
    char *ret = rec_fld(head->rec, "pid");
    log_msg("OP", "%s", ret);
    return strdup(ret);
  }

  return strdup("");
}

char* expand_symbol(const char *key, const char *alt)
{
  log_msg("EXPAND", "expand symb {%s:%s}", key, alt);

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
    case 'o':
      return op_type(alt);
  }

  return "";
}
