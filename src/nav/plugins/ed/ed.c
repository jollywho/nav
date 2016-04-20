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

#define ED_PASSVE 0
#define ED_RENAME 1
#define ED_CONFRM 2
#define ED_CLOSED 4

static const char* ED_MSG =
  "# This file will be executed when you close the editor.\n"
  "# Please double-check everything, clear the file to abort.";

static void ed_chown_plugin(Ed *ed)
{
  ed->base->top = ed;
  ed->base->name = "ed";
  buf_set_plugin(ed->buf, ed->base);
}

static void ed_cleanup(Ed *ed)
{
  log_msg("ED", "ed_cleanup");
  //TODO: cleanup based on state
  del_param_list(ed->src.argv,  ed->src.argc);
  del_param_list(ed->dest.argv, ed->dest.argc);
  ed_chown_plugin(ed);
  unlink(ed->tmp_name);
  close(ed->fd);
  window_close_focus();
}

static void ed_dump_contents(Ed *ed, varg_T *args)
{
  log_msg("ED", "ed_dump_contents");

  lseek(ed->fd, 0, SEEK_SET);

  if (ed->state == ED_CONFRM) {
    write(ed->fd, ED_MSG, strlen(ED_MSG));
    write(ed->fd, "\n", 1);
  }

  for (int i = 0; i < args->argc; i++) {
    log_err("ED", "write: [%s]", args->argv[i]);
    write(ed->fd, args->argv[i], strlen(args->argv[i]));
    write(ed->fd, "\n", 1);
  }
}

static bool ed_read_temp(Ed *ed)
{
  log_msg("ED", "ed_read_temp");

  lseek(ed->fd, 0, SEEK_SET);

  char buf[1024];
  ssize_t len, off, max;
  char *dest = malloc(sizeof(buf));
  off = 0;
  max = 1024;

  while ((len = read(ed->fd, buf, sizeof(buf)))) {
    if (off + len > max)
      dest = realloc(dest, max *= 2);
    strncpy(&dest[off], buf, len);
    off += len;
  }
  dest[off] = '\0';
  log_err("ED", "read: [%s]", dest);

  if (ed->state == ED_CONFRM) {
    //  if read 0 lines || # > dest.argc
    //    nv_msg. cleanup
    del_param_list(ed->dest.argv, ed->dest.argc);
  }

  ed->dest.argc = count_lines(dest);
  ed->dest.argv = malloc((1+ed->dest.argc)*sizeof(char*));
  log_err("ED", "%d count", ed->dest.argc);

  char *next = NULL;
  char *prev = dest;
  int i = 0;
  while ((next = strstr(prev, "\n"))) {
    log_err("ED", "newline");
    *next = '\0';
    log_err("ED", "read: [%s]", prev);
    ed->dest.argv[i] = strdup(prev);
    i++;
    prev = next+1;
    if (!prev)
      break;
  }
  free(dest);

  //if (ed->state == ED_RENAME)
  //  remove from dest where src != dest
  //  if no remaining
  //    nv_msg. cleanup
  //
  return true;
}

static void ed_start_term(Ed *ed, varg_T *arg)
{
  log_msg("ED", "ed_start_term");
  ed->state++;
  if (ed->state > ED_CONFRM) {
    //error
    return ed_cleanup(ed);
  }

  ed_dump_contents(ed, arg);

  char *line;
  asprintf(&line, "$EDITOR %s", ed->tmp_name);
  term_new(ed->base, ed->buf, line);
  free(line);

  term_set_editor(ed->base, ed);
  buf_set_status(ed->buf, "ED", NULL, NULL);
}

static void ed_stage_confirm(Ed *ed)
{
  log_msg("ED", "ed_stage_confirm");
  int max = 0;
  for (int i = 0; i < ed->src.argc; i++)
    max = MAX(max, strlen(ed->src.argv[i]));

  char buf[PATH_MAX];
  for (int i = 0; i < ed->dest.argc; i++) {
    char **src  = &ed->src.argv[i];
    char **dest = &ed->dest.argv[i];
    sprintf(buf, "%-*s  %s", max, *src, *dest);
    SWAP_ALLOC_PTR(*dest, strdup(buf));
  }

  ed_start_term(ed, &ed->dest);
}

static void ed_eval_temp(Ed *ed)
{
  log_msg("ED", "ed_eval_temp");
  //TODO:
  //split string after src != dest
  //strip spaces
  //file_move_str(src, split_dest)
  ed_cleanup(ed);
}

void ed_close_cb(Plugin *plugin, Ed *ed)
{
  log_msg("ED", "ed_close_cb");

  term_delete(ed->base);
  window_refresh();

  if (!ed_read_temp(ed))
    return ed_cleanup(ed);

  if (ed->state == ED_RENAME)
    return ed_stage_confirm(ed);
  if (ed->state == ED_CONFRM)
    return ed_eval_temp(ed);
}

static bool ed_prepare(Ed *ed)
{
  Plugin *lis = plugin_from_id(ed->bufno);
  if (!lis || !lis->hndl)
    return false;

  Buffer *buf = lis->hndl->buf;
  ed->src = buf_focus_sel(buf, "name");

  sprintf(ed->tmp_name, "/tmp/navedit-XXXXXX");
  ed->fd = mkstemp(ed->tmp_name);
  log_err("ED", "%d:::::::::::%s", ed->fd, ed->tmp_name);
  return true;
}

//new ED  : idle pipe target
//edit    : open with selection
void ed_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("ED", "init");

  Ed *ed = malloc(sizeof(Ed));
  ed->base = plugin;
  ed->buf = buf;
  ed->state = ED_PASSVE;

  if (arg && str_num(arg, &ed->bufno)) {
    if (ed_prepare(ed))
      ed_start_term(ed, &ed->src);
  }
}

void ed_delete(Plugin *plugin)
{
  log_msg("ED", "delete");
  Ed *ed = plugin->top;
  free(ed);
}
