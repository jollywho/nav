/*
 ** parser.c - a primitive file parser
 */

#include "parser.h"

void
new_record(FN_rec **rec, int *rcount, int *fcount) {
  (*rcount)++;
  (*fcount) = 0;
  rec[*rcount] = realloc(rec[*rcount], sizeof(FN_rec));
}

void
new_attr(char *line, char **ptr) {
  char *lbl = strtok(line, ":");
  (*ptr) = malloc(sizeof(char)*strlen(lbl));
  strncpy(*ptr, lbl, strlen(lbl));
}

void
parse_file(const char *path, FN_data *data) {
  FILE *fp;
  char *line;
  ssize_t size; size_t len = 0;
  int rcount = -1; int fcount = 0; int skip = 1;
  fp = fopen(path, "r");

  if (fp == NULL) { exit(EXIT_FAILURE); }

  data->rec = malloc(sizeof(FN_rec));

  while ((size = getline(&line, &len, fp)) != -1) {
    if (size == 1) { skip = 1; continue; } //todo: trim lines
    if (size > 1 && skip) { new_record(data->rec, &rcount, &fcount); }
    size_t ln = strlen(line) - 1;
    line[ln] = '\0';

    data->rec[rcount]->id = rcount + 1;
    data->rec[rcount]->fields = realloc(data->rec[rcount]->fields, (fcount + 1) * sizeof(FN_field));
    data->rec[rcount]->fields[fcount] = malloc(sizeof(FN_field));

    new_attr(line, &data->rec[rcount]->fields[fcount]->name);
    new_attr(NULL, &data->rec[rcount]->fields[fcount]->val);
    fcount++; skip = 0;
  }

  fclose(fp);
  if (line) { free(line); }
}
