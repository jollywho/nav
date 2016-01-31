#include <unistd.h>
#include "fnav/lib/uthash.h"

#include "fnav/info.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/compl.h"
#include "fnav/tui/window.h"

typedef struct fn_mark fn_mark;
struct fn_mark {
  String key;
  String path;
  UT_hash_handle hh;
};

static fn_mark *lbl_marks;
static fn_mark *chr_marks;

void info_parse(String line)
{
  String label;
  String dir;
  switch (line[0]) {
    case '@':
      label = strtok(line, " ");
      dir = strtok(NULL, " ");
      mark_label_dir(label, dir);
      break;
    case '\'':
      label = strtok(line, " ");
      dir = strtok(NULL, " ");
      mark_strchr_str(label, dir);
      break;
  }
}

static void write_mark_info(FILE *f, fn_mark *mrk)
{
  if (!mrk)
    return;

  for (; mrk != NULL; mrk = mrk->hh.next)
    fprintf(f, "%s %s\n", mrk->key, mrk->path);
}

void info_write_file(FILE *file)
{
  log_msg("INFO", "config_write_info");
  write_mark_info(file, lbl_marks);
  write_mark_info(file, chr_marks);
}

void mark_list(List *args)
{
}

void marklbl_list(List *args)
{
  log_msg("INFO", "marklbl_list");
  unsigned int count = HASH_COUNT(lbl_marks);
  compl_new(count, COMPL_STATIC);
  fn_mark *it;
  int i = 0;
  for (it = lbl_marks; it != NULL; it = it->hh.next) {
    compl_set_index(i, it->key, 1, it->path);
    i++;
  }
}

static void mark_del(fn_mark **mrk, fn_mark **tbl)
{
  log_msg("INFO", "MARKDEL");
  HASH_DEL(*tbl, *mrk);
  free((*mrk)->key);
  free((*mrk)->path);
  free((*mrk));
}

String mark_path(String key)
{
  fn_mark *mrk;
  HASH_FIND_STR(lbl_marks, key, mrk);
  if (mrk)
    return mrk->path;

  return NULL;
}

String mark_str(int chr)
{
  fn_mark *mrk;
  String key;
  asprintf(&key, "'%c", chr);

  HASH_FIND_STR(chr_marks, key, mrk);
  free(key);
  if (mrk)
    return mrk->path;

  return NULL;
}

void mark_label_dir(String label, String dir)
{
  log_msg("INFO", "mark_label_dir");
  String key;
  if (label[0] == '@')
    label = &label[1];
  asprintf(&key, "@%s", label);

  String tmp = strdup(dir);
  fn_mark *mrk;
  HASH_FIND_STR(lbl_marks, key, mrk);
  if (mrk)
    mark_del(&mrk, &lbl_marks);

  mrk = malloc(sizeof(fn_mark));
  mrk->key = key;
  mrk->path = tmp;
  HASH_ADD_STR(lbl_marks, key, mrk);
}

void mark_strchr_str(String str, String dir)
{
  if (strlen(str) == 2)
    mark_chr_str(str[1], dir);
}

void mark_chr_str(int chr, String dir)
{
  log_msg("FM", "mark_key_str");
  String key;
  asprintf(&key, "'%c", chr);

  String tmp = strdup(dir);
  fn_mark *mrk;
  HASH_FIND_STR(chr_marks, key, mrk);
  if (mrk)
    mark_del(&mrk, &chr_marks);

  mrk = malloc(sizeof(fn_mark));
  mrk->key = key;
  mrk->path = tmp;
  HASH_ADD_STR(chr_marks, key, mrk);
}
