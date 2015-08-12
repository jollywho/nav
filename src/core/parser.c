/*
 ** parser.c - a primitive file parser
 */

#include "parser.h"

void
parse_file(const char *path, FN_data *data) {

  FILE *fp;
  ssize_t size;
  size_t len = 0;
  char *line, *spl;
  fp = fopen(path, "r");
  int rcount = 0; int fcount = 0;

  if (fp == NULL)
    exit(EXIT_FAILURE);

  while ((size = getline(&line, &len, fp)) != -1) {
    rcount++;
    data->list = realloc(data->list, rcount * sizeof(FN_rec));
    data->list[rcount - 1] = malloc(sizeof(FN_rec));

    //todo: iterate strtok
    //todo: check unique fields
    spl = strtok(line, ":");
    data->list[rcount - 1]->id = rcount;
    data->list[rcount - 1]->fields = malloc(sizeof(FN_field));
    data->list[rcount - 1]->fields[fcount] = malloc(sizeof(FN_field));
    data->list[rcount - 1]->fields[fcount]->name = spl;
  }

  fclose(fp);
  if (line)
    free(line);
}
