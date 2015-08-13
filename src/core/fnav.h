#ifndef _FNAV_H
#define _FNAV_H

#define PORT "9034"

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

#endif
