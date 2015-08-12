/*
 ** parser.c - a primitive file parser
 */

#include "parser.h"

void
parse_file(const char *path, FN_data *data) {

  FILE *fp;
  ssize_t size;
  size_t len = 0;
  char *line;
  fp = fopen(path, "r");
  int rcount = 0; int fcount = 0;

  if (fp == NULL)
    exit(EXIT_FAILURE);

  data->list = malloc(sizeof(FN_rec));
  data->list[rcount] = malloc(sizeof(FN_rec));
  data->list[rcount]->fields = malloc(2 * sizeof(FN_field));
  data->list[rcount]->id = rcount + 1;
  while ((size = getline(&line, &len, fp)) != -1) {
    char *label = strtok(line, ":");
    data->list[rcount]->fields[fcount] = malloc(sizeof(FN_field));
    data->list[rcount]->fields[fcount]->name = malloc(sizeof(char)*strlen(label));
    strncpy(data->list[rcount]->fields[fcount]->name, label, strlen(label));

    char *val = strtok(NULL, ":");
    int ival = strtol(val, NULL, 10);
    if (ival)
      data->list[rcount]->fields[fcount]->val.ival = ival;
      else {
      data->list[rcount]->fields[fcount]->val.cval = malloc(sizeof(char)*strlen(val));
      strncpy(data->list[rcount]->fields[fcount]->val.cval, val, strlen(val));
    }
    fcount++;
  }

  fclose(fp);
  if (line)
    free(line);
}
