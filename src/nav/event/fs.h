#ifndef FN_EVENT_FS_H
#define FN_EVENT_FS_H

#include "nav/event/event.h"
#include "nav/table.h"

typedef struct fentry fentry;
typedef struct fn_fs fn_fs;

struct fn_fs {
  char *path;

  fn_handle *hndl;
  uv_fs_t uv_fs;  /* readonly stat */
  char *readkey;
  fentry *ent;

  void *data;
  bool running;
  argv_callback open_cb;
  argv_callback stat_cb;
  UT_hash_handle hh;
};

fn_fs* fs_init(fn_handle *hndl);
void fs_cleanup(fn_fs *fs);
void fs_open(fn_fs *fs, const char *);
void fs_close(fn_fs *fs);
void fs_read(fn_fs *fs, const char *);
void fs_clr_cache(char *);
void fs_clr_all_cache();
void fs_reload(char *);

void fs_cancel(fn_fs *fs);
void fs_fastreq(fn_fs *fs);
bool fs_blocking(fn_fs *fs);

bool isrecdir(fn_rec *);
bool isreclnk(fn_rec *rec);
bool isrecreg(fn_rec *rec);
time_t rec_ctime(fn_rec *rec);
off_t rec_stsize(fn_rec *rec);
mode_t rec_stmode(fn_rec *rec);
char* fs_expand_path(const char *);
char* valid_full_path(char *, char *);
char* fs_parent_dir(char *);
char* conspath(const char *, const char *);
const char* file_ext(const char *filename);

char* fs_current_dir();

#endif
