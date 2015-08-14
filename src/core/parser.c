/*
 ** parser.c - a primitive file parser
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include "parser.h"

void
new_record(FN_data *data, int *rcount, int *fcount) {
  (*rcount)++;
  (*fcount) = 0;
  data->rec[*rcount] = realloc(data->rec[*rcount], sizeof(FN_rec));
  data->rec_num++;
}

void
new_attr(char *line, char **ptr) {
  char *lbl = strtok(line, ":");
  (*ptr) = malloc(sizeof(char)*strlen(lbl));
  strncpy(*ptr, lbl, strlen(lbl));
}

char*
trim(char *c) {
  char *e = c + strlen(c) - 1;
  while(*c && isspace(*c)) c++;
  while(e > c && isspace(*e)) *e-- = '\0';
  return c;
}

void
parse_file(const char *path, FN_data *data) {
  FILE *fp;
  char *line, *buf;
  ssize_t size; size_t len = 0;
  int rcount = -1; int fcount = 0; int skip = 1;

  fp = fopen(path, "r");
  if (fp == NULL) { exit(EXIT_FAILURE); }

  data->rec = malloc(sizeof(FN_rec));

  while ((getline(&line, &len, fp)) != -1) {
    buf = trim(line);
    size = strlen(buf);
    if (size == 0) { skip = 1; continue; }
    if (size > 0 && skip) { new_record(data, &rcount, &fcount); }
    data->rec[rcount]->id = rcount + 1;
    FN_rec *f = data->rec[rcount];
    f->fields = realloc(f->fields, (fcount + 1) * sizeof(FN_field));
    f->field_num++;
    f->fields[fcount] = malloc(sizeof(FN_field));

    new_attr(buf, &f->fields[fcount]->name);
    new_attr(NULL, &f->fields[fcount]->val);
    fcount++; skip = 0;
  }

  fclose(fp);
  if (line) { free(line); }
}

