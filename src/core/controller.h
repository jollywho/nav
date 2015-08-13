#ifndef _CONTROLLER_H
#define _CONTROLLER_H

typedef struct {
  char *name;
  void (*func)();
} FN_cntlr_func;

typedef struct _cntlr {
  char *name;
  struct _cntlr *self;

  FN_data *dat;

  int fcount;
  FN_cntlr_func **callbacks;
  void (*call)();
  void (*add)();
} FN_cntlr;

FN_cntlr *cntlr_create(const char *name);
void add(struct _cntlr *self, FN_cntlr_func *f);
void call(struct _cntlr *self, const char *name);

void showdat();
#endif
