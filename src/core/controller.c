/*
 ** controller.c - data controller interface
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fnav.h"
#include "parser.h"
#include "controller.h"

FN_cntlr *cntlr_create(const char *name) {
  FN_cntlr *cntlr = malloc(sizeof(FN_cntlr));
  cntlr->name = malloc(sizeof(char)*strlen(name));
  cntlr->self = cntlr;
  cntlr->call = call;
  cntlr->add = add;
  cntlr->fcount = 0;
  strcpy(cntlr->name, name);

  //data
  FN_data *d = malloc(sizeof(FN_data));
  parse_file("dummy", d);
  return cntlr;
}

void add(struct _cntlr *self, FN_cntlr_func *f) {
  int c = self->fcount * sizeof(FN_cntlr_func);
  self->callbacks = realloc(self->callbacks, c);
  self->callbacks[c] = malloc(sizeof(FN_cntlr_func));
  self->callbacks[c]->name = f->name;
  self->callbacks[c]->func = f->func;
  self->fcount++;
}

void call(struct _cntlr *self, const char *name) {
  for(int c = 0; c < self->fcount; c++) {
    if (self->callbacks[c]->name == name) {
      self->callbacks[0]->func();
    }
  }
}

void showdat() {
  printf("dat shown\n");
}
