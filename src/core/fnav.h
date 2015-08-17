#ifndef FN_CORE_FNAV_H
#define FN_CORE_FNAV_H

typedef struct {
  char *val;
  char *name;
}FN_field;

typedef struct  {
  int id;
  char *parent;
  FN_field **fields;
  int field_num;
}FN_rec;

typedef struct {
  FN_rec **rec;
  int rec_num;
}FN_data;

typedef struct {
  char *name;
  void (*func)();
}FN_cntlr_func;

typedef struct FN_cntlr {
  struct FN_cntlr *self;
  char *name;

  FN_data *dat;

  int fcount;
  FN_cntlr_func **callbacks;
  void (*call)();
  void (*add)();
}FN_cntlr;

#endif
