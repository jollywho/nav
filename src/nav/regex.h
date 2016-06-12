#ifndef NV_REGEX_H
#define NV_REGEX_H

#include "nav/plugins/plugin.h"

typedef struct LineMatch LineMatch;
typedef struct Pattern Pattern;

LineMatch* regex_new(Handle *hndl);
void regex_destroy(Handle *hndl);
void regex_build(LineMatch *lm, const char *);
void regex_del_matches(LineMatch *lm);
void regex_setsign(int sign);

Pattern* regex_pat_new(const char *);
void regex_pat_delete(Pattern *pat);
bool regex_match(Pattern *pat, const char *);

void regex_mk_pivot(LineMatch *lm);
void regex_pivot(LineMatch *lm);

void regex_hover(LineMatch *lm);
int regex_next(LineMatch *lm, int line, int dir);
char* regex_str(LineMatch *lm);
int regex_match_count(LineMatch *lm);

#endif
