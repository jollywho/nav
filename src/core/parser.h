#ifndef _PARSER_H
#define _PARSER_H

typedef struct {
  char *val;
  char *name;
} FN_field;

typedef struct {
  int id;
  char *parent;
  FN_field **fields;
  int field_num;
} FN_rec;

typedef struct {
  FN_rec **rec;
  int rec_num;
} FN_data;

void parse_file(const char *path, FN_data *rec);
#endif
