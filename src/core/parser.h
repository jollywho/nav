#ifndef _PARSER_H
#define _PARSER_H

typedef struct FN_field FN_field;
typedef struct FN_rec FN_rec;
typedef struct FN_data FN_data;

void parse_file(const char *path, FN_data *rec);

#endif
