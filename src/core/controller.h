#ifndef _CONTROLLER_H
#define _CONTROLLER_H

typedef struct {
  char *name;
  void (*func)();
} FN_cntlr_func;

typedef struct {
  char *name;
  FN_cntlr_func **callbacks;
  FN_cntlr_func *(*call)(char *name);
  FN_data *dat;
} FN_cntlr;

int cntlr_create(FN_cntlr *cntlr, char *name);
void showdat();
#endif
