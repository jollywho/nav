#include "nav/plugins/out/out.h"
#include "nav/tui/buffer.h"
#include "nav/event/event.h"
#include "nav/event/hook.h"
#include "nav/log.h"
#include "nav/table.h"
#include "nav/model.h"

static Out out;

void out_init()
{
  log_msg("OUT", "init");
}

void out_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("OUT", "new");

  out.base = plugin;
  plugin->top = &out;
  plugin->name = "out";
  plugin->fmt_name = "OUT";

  buf_set_plugin(buf, plugin);
  buf_set_pass(buf);

  hook_init_host(plugin);
}

void out_delete(Plugin *plugin)
{
  hook_clear_host(plugin);
  hook_cleanup_host(plugin);
}
