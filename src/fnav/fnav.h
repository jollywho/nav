#ifndef FN_CORE_FNAV_H
#define FN_CORE_FNAV_H

typedef char* String;

typedef struct {
  String name;
  String val;
} FN_field;

typedef FN_field** FN_field_lst;

typedef struct  {
  int id;
  String parent;
  FN_field_lst fields;
  int field_num;
} FN_rec;

typedef FN_rec** FN_rec_lst;

typedef struct {
  FN_rec_lst records;
  int rec_count;
} FN_data;

#endif
