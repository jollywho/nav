#ifndef NV_EVENT_FS_H
#define NV_EVENT_FS_H

#include "nav/event/event.h"
#include "nav/table.h"

typedef struct fentry fentry;
typedef struct nv_fs nv_fs;

struct nv_fs {
  char *path;

  Handle *hndl;
  uv_fs_t uv_fs;  /* readonly stat */
  char *readkey;
  fentry *ent;

  void *data;
  bool running;
  argv_callback open_cb;
  argv_callback stat_cb;
  UT_hash_handle hh;
};

nv_fs* fs_init(Handle *hndl);
void fs_cleanup(nv_fs *fs);
void fs_open(nv_fs *fs, const char *);
void fs_close(nv_fs *fs);
void fs_read(nv_fs *fs, const char *);
void fs_clr_cache(char *);
void fs_clr_all_cache();
void fs_reload(char *);

void fs_cancel(nv_fs *fs);
void fs_fastreq(nv_fs *fs);
bool fs_blocking(nv_fs *fs);

const char* stat_type(struct stat *);
bool isrecdir(TblRec *);
bool isreclnk(TblRec *rec);
bool isrecreg(TblRec *rec);
time_t rec_ctime(TblRec *rec);
off_t rec_stsize(TblRec *rec);
mode_t rec_stmode(TblRec *rec);
char* fs_expand_path(const char *);
char* valid_full_path(char *, char *);
char* fs_parent_dir(char *);
char* conspath(const char *, const char *);
const char* file_ext(const char *filename);
const char* file_base(char *filename);

char* fs_current_dir();

#endif
