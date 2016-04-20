#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "nav/plugins/ed/ed.h"
#include "nav/plugins/term/term.h"
#include "nav/event/event.h"
#include "nav/tui/buffer.h"
#include "nav/tui/window.h"
#include "nav/event/hook.h"
#include "nav/log.h"
#include "nav/table.h"
#include "nav/util.h"

void ed_close_cb(Plugin *plugin, Ed *ed)
{
  log_msg("ED", "ed_close_cb");
}

//create term with $EDITOR temp
static void ed_start_term(Ed *ed, Buffer *buf)
{
  log_msg("ED", "ed_start_term");
  char *line;
  asprintf(&line, "$EDITOR %s", ed->tmp_name);
  term_new(ed->base, buf, line);
  free(line);
  term_set_editor(ed->base, ed);
}

//create temp file
//write to file contents of selection (arg)
static bool ed_dump_select(Ed *ed)
{
  log_msg("ED", "ed_start_term");
  Plugin *lis = plugin_from_id(ed->bufno);
  if (!lis || !lis->hndl)
    return false;

  Buffer *buf = lis->hndl->buf;
  varg_T args = buf_focus_sel(buf, "name");

  sprintf(ed->tmp_name, "/tmp/navedit-XXXXXX");
  ed->fd = mkstemp(ed->tmp_name);
  log_err("ED", "%d:::::::::::%s", ed->fd, ed->tmp_name);

  for (int i = 0; i < args.argc; i++) {
    write(ed->fd, args.argv[i], strlen(args.argv[i]));
    write(ed->fd, "\r\n", 2);
  }

  del_param_list(args.argv, args.argc);

  return true;
}

//new ED  : idle pipe target
//edit    : open with selection
void ed_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("ED", "init");

  Ed *ed = malloc(sizeof(Ed));
  ed->base = plugin;

  if (arg && str_num(arg, &ed->bufno)) {
    if (ed_dump_select(ed))
      ed_start_term(ed, buf);
  }

  //override VT's ownership of buffer

  buf_set_status(buf, "ED", NULL, NULL);

  //FIXME: term closes ed and buffer.
  //can easily make new term. need ED callback and prevent buffer del

  //check if buf or term was closed with term->closed
  //window closed -> interpret as a cancel -> cleanup
  //term closed   -> next stage
  //
  //stage 1
  //edit $tmp with confirm contents (mv src dest)
  //reopen $EDITOR $tmp in plugin
  //
  //stage 2
  //send line to file_move
  //cleanup
}

void ed_delete(Plugin *plugin)
{
  log_msg("ED", "delete");
  Ed *ed = plugin->top;

  unlink(ed->tmp_name);
  close(ed->fd);
  free(ed);
}
