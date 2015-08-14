#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include "fnavd.h"

FN_cntlr *cntlr_create(const char *name);
void add(FN_cntlr *self, FN_cntlr_func *f);
void call(FN_cntlr *self, const char *name);

void showdat();
#endif
