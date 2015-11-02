#ifndef FN_CMDSTR_H
#define FN_CMDSTR_H

#include "fnav/lib/utarray.h"

typedef struct{
  UT_array *tokens;
} Cmdstr;

void cmdstr_init(Cmdstr *cmdstr);
void cmdstr_cleanup(Cmdstr *cmdstr);
void tokenize_line(Cmdstr *cmdstr, char * const *str);
int cmdstr_prev_word(Cmdstr *cmdstr, int pos);

#endif
