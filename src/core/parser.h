#ifndef _PARSER_H
#define _PARSER_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

typedef struct {
  int ival;
  char *cval;
} FN_fval;

typedef struct {
  FN_fval val;
  char *name;
} FN_field;

typedef struct {
  int id;
  char *parent;
  FN_field **fields;
} FN_rec;

typedef struct {
  FN_rec **list;
} FN_data;

void parse_file(const char *path, FN_data *rec);
#endif
