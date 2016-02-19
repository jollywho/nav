#include <unistd.h>
#include "fnav/lib/uthash.h"

#include "fnav/info.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/compl.h"
#include "fnav/tui/window.h"

typedef struct fn_mark fn_mark;
struct fn_mark {
  char *key;
  char *path;
  UT_hash_handle hh;
};

static fn_mark *lbl_marks;
static fn_mark *chr_marks;

void info_parse(char *line)
{
  char *label;
  char *dir;
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
    compl_set_index(i, 1, it->path, "%s", it->key);
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

char * mark_path(const char *key)
{
  fn_mark *mrk;
  HASH_FIND_STR(lbl_marks, key, mrk);
  if (mrk)
    return mrk->path;

  return NULL;
}

char * mark_str(int chr)
{
  fn_mark *mrk;
  char * key;
  asprintf(&key, "'%c", chr);

  HASH_FIND_STR(chr_marks, key, mrk);
  free(key);
  if (mrk)
    return mrk->path;

  return NULL;
}

void mark_label_dir(char *label, const char *dir)
{
  log_msg("INFO", "mark_label_dir");
  char * key;
  if (label[0] == '@')
    label = &label[1];
  asprintf(&key, "@%s", label);

  char * tmp = strdup(dir);
  fn_mark *mrk;
  HASH_FIND_STR(lbl_marks, key, mrk);
  if (mrk)
    mark_del(&mrk, &lbl_marks);

  mrk = malloc(sizeof(fn_mark));
  mrk->key = key;
  mrk->path = tmp;
  HASH_ADD_STR(lbl_marks, key, mrk);
}

void mark_strchr_str(const char *str, const char *dir)
{
  if (strlen(str) == 2)
    mark_chr_str(str[1], dir);
}

void mark_chr_str(int chr, const char *dir)
{
  log_msg("FM", "mark_key_str");
  char * key;
  asprintf(&key, "'%c", chr);

  char * tmp = strdup(dir);
  fn_mark *mrk;
  HASH_FIND_STR(chr_marks, key, mrk);
  if (mrk)
    mark_del(&mrk, &chr_marks);

  mrk = malloc(sizeof(fn_mark));
  mrk->key = key;
  mrk->path = tmp;
  HASH_ADD_STR(chr_marks, key, mrk);
}
