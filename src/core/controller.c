/*
 ** controller.c - data controller interface
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fnav.h"
#include "parser.h"
#include "controller.h"

int cntlr_create(FN_cntlr *cntlr, char *name) {
  cntlr->name = malloc(sizeof(char)*strlen(name));
  strcpy(cntlr->name, name);
  cntlr->callbacks = malloc(2*sizeof(FN_cntlr_func));
  cntlr->callbacks[0] = malloc(sizeof(FN_cntlr_func));
  cntlr->callbacks[0]->name = "showdat";
  cntlr->callbacks[0]->func = showdat;

  FN_data *d = malloc(sizeof(FN_data));
  parse_file("dummy", d);
  return 1;
}

void showdat() {
  printf("dat shown\n");
}
