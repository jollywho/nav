#include "fnav/compl.h"

#define HASH_INS(t,obj) \
  HASH_ADD_KEYPTR(hh, t, obj.key, strlen(obj.key), &obj);

static compl_list* compl_win();

#define DEFAULT_SIZE ARRAY_SIZE(compl_defaults)
static compl_entry compl_defaults[] = {
  { "win",     compl_win     },
};

compl_entry *compl_table;

void compl_init()
{
  for (int i = 0; i < (int)DEFAULT_SIZE; i++) {
    HASH_INS(compl_table, compl_defaults[i]);
  }
}

compl_list* compl_of(String name)
{
  compl_entry *find;
  HASH_FIND_STR(compl_table, name, find);
  if (!find) return NULL;
  return find->gen();
}

static compl_list* compl_win()
{
  return 0;
}
