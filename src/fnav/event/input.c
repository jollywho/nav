#include <stdio.h>
#include <uv.h>
#include <termkey.h>

#include "fnav/ascii.h"
#include "fnav/event/input.h"
#include "fnav/tui/window.h"
#include "fnav/event/loop.h"
#include "fnav/event/event.h"
#include "fnav/log.h"

uv_poll_t poll_handle;
uv_loop_t loop;

TermKey *tk;
void event_input();

typedef unsigned char char_u;

static struct modmasktable {
  short mod_mask;               /* Bit-mask for particular key modifier */
  short mod_flag;               /* Bit(s) for particular key modifier */
  char_u name;                  /* Single letter name of modifier */
} mod_mask_table[] =
{
  {MOD_MASK_ALT,              MOD_MASK_ALT,           (char_u)'M'},
  {MOD_MASK_META,             MOD_MASK_META,          (char_u)'T'},
  {MOD_MASK_CTRL,             MOD_MASK_CTRL,          (char_u)'C'},
  {MOD_MASK_SHIFT,            MOD_MASK_SHIFT,         (char_u)'S'},
  /* 'A' must be the last one */
  {MOD_MASK_ALT,              MOD_MASK_ALT,           (char_u)'A'},
  {0, 0, NUL}
};

static struct key_name_entry {
  int key;              /* Special key code or ascii value */
  char  *name;        /* Name of key */
} key_names_table[] =
{
  {' ',               (char *)"Space"},
  {TAB,               (char *)"Tab"},
  {NL,                (char *)"NL"},
  {NL,                (char *)"NewLine"},     /* Alternative name */
  {NL,                (char *)"LineFeed"},    /* Alternative name */
  {NL,                (char *)"LF"},          /* Alternative name */
  {CAR,               (char *)"CR"},
  {CAR,               (char *)"Return"},      /* Alternative name */
  {CAR,               (char *)"Enter"},       /* Alternative name */
  {ESC,               (char *)"Esc"},
  {ESC,               (char *)"Escape"},
  {CSI,               (char *)"CSI"},
  {'|',               (char *)"Bar"},
  {'\\',              (char *)"Bslash"},
  {BS,                (char *)"Del"},
  {DEL,               (char *)"Delete"},      /* Alternative name */
  {0,                 NULL}
};

int get_special_key_code(char *name)
{
  char  *table_name;
  int i;

  for (i = 0; key_names_table[i].name != NULL; i++) {
    table_name = key_names_table[i].name;
    if (strcasecmp(name, table_name) == 0) {
      log_msg("_", "%s", key_names_table[i].name);
      return key_names_table[i].key;
    }
  }

  return 0;
}

int name_to_mod_mask(int c)
{
  int i;

  c = TOUPPER_ASC(c);
  for (i = 0; mod_mask_table[i].mod_mask != 0; i++)
    if (c == mod_mask_table[i].name)
      return mod_mask_table[i].mod_flag;
  return 0;
}

void input_check()
{
  log_msg("INPUT", "INPUT CHECK");
  event_input();
}

void input_init(void)
{
  log_msg("INIT", "INPUT");
  tk = termkey_new(0,0);
  termkey_set_flags(tk, TERMKEY_FLAG_UTF8 | TERMKEY_CANON_DELBS);

  uv_poll_init(eventloop(), &poll_handle, 0);
  uv_poll_start(&poll_handle, UV_READABLE, input_check);
}

int extract_modifiers(int key, int *modp)
{
  int modifiers = *modp;

  if ((modifiers & MOD_MASK_SHIFT) && ASCII_ISALPHA(key)) {
    key = TOUPPER_ASC(key);
    modifiers &= ~MOD_MASK_SHIFT;
  }
  if ((modifiers & MOD_MASK_CTRL)
      && ((key >= '?' && key <= '_') || ASCII_ISALPHA(key))
      ) {
    key = Ctrl_chr(key);
    modifiers &= ~MOD_MASK_CTRL;
    /* <C-@> is <Nul> */
    if (key == 0)
      key = K_ZERO;
  }
  if ((modifiers & MOD_MASK_ALT) && key < 0x80) {
    key |= 0x80;
    modifiers &= ~MOD_MASK_ALT;         /* remove the META modifier */
  }

  *modp = modifiers;
  return key;
}

void event_input()
{
  TermKeyKey key;
  TermKeyResult ret;

  termkey_advisereadable(tk);

  while ((ret = termkey_getkey_force(tk, &key)) == TERMKEY_RES_KEY) {
    size_t len;
    char buf[64];
    char *bp;
    len = termkey_strfkey(tk, buf, sizeof(buf), &key, TERMKEY_FORMAT_VIM);

    if (buf[0] == '<') {
      bp = buf;
      bp++;
      bp[len - 2] = '\0';
      int newkey = get_special_key_code(bp);
      if (newkey) {
        key.code.number = newkey;
      }
      else if (strcasecmp(HC_S_TAB, bp) == 0) {
        key.code.number = K_S_TAB;
      }
      if (key.modifiers) {
        int mask = 0x0;
        mask |= name_to_mod_mask(*bp);
        int keycode = key.code.number;
        if (!IS_SPECIAL(keycode))
          key.code.number  = extract_modifiers(keycode, &mask);
      }
    }
    window_input(key.code.number);
  }
}

/*
 * Compare functions for qsort() below, that checks the command character
 * through the index in nv_cmd_idx[].
 */
static int nv_compare(const void *s1, const void *s2, void *arg)
{
  int c1, c2;
  fn_keytbl *kt = (fn_keytbl*)arg;

  /* The commands are sorted on absolute value. */
  c1 = kt->tbl[*(const short *)s1].cmd_char;
  c2 = kt->tbl[*(const short *)s2].cmd_char;
  if (c1 < 0)
    c1 = -c1;
  if (c2 < 0)
    c2 = -c2;
  return c1 - c2;
}

/*
 * Initialize the nv_cmd_idx[] table.
 */
void input_init_tbl(fn_keytbl *kt)
{
  /* Fill the index table with a one to one relation. */
  for (short int i = 0; i < (short int)kt->maxsize; ++i) {
    kt->cmd_idx[i] = i;
  }

  /* Sort the commands by the command character.  */
  qsort_r(kt->cmd_idx, kt->maxsize, sizeof(short), nv_compare, kt);

  /* Find the first entry that can't be indexed by the command character. */
  short int i;
  for (i = 0; i < (short int)kt->maxsize; ++i) {
    if (i != kt->tbl[kt->cmd_idx[i]].cmd_char) {
      break;
    }
  }
  kt->maxlinear = i - 1;
}

/*
 * Search for a command in the commands table.
 * Returns -1 for invalid command.
 */
int find_command(fn_keytbl *kt, int cmdchar)
{
  int i;
  int idx;
  int top, bot;
  int c;

  /* A multi-byte character is never a command. */
  if (cmdchar >= 0x100)
    return -1;

  /* We use the absolute value of the character.  Special keys have a
   * negative value, but are sorted on their absolute value. */
  if (cmdchar < 0)
    cmdchar = -cmdchar;

  /* If the character is in the first part: The character is the index into
   * nv_cmd_idx[]. */
  if (cmdchar <= kt->maxlinear)
    return kt->cmd_idx[cmdchar];

  /* Perform a binary search. */
  bot = kt->maxlinear + 1;
  top = kt->maxsize - 1;
  idx = -1;
  while (bot <= top) {
    i = (top + bot) / 2;
    c = kt->tbl[kt->cmd_idx[i]].cmd_char;
    if (c < 0)
      c = -c;
    if (cmdchar == c) {
      idx = kt->cmd_idx[i];
      break;
    }
    if (cmdchar > c)
      bot = i + 1;
    else
      top = i - 1;
  }
  return idx;
}
