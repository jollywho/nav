#include <stdio.h>
#include <uv.h>
#include <termkey.h>

#include "nav/ascii.h"
#include "nav/event/input.h"
#include "nav/tui/window.h"
#include "nav/event/event.h"
#include "nav/log.h"
#include "nav/option.h"
#include "nav/event/shell.h"
#include "nav/util.h"

static uv_poll_t poll_handle;
static TermKey *tk;
static Keyarg ca;

typedef unsigned char char_u;

static struct modmasktable {
  short mod_mask;               /* Bit-mask for particular key modifier */
  short mod_flag;               /* Bit(s) for particular key modifier */
  char_u name;                  /* Single letter name of modifier */
} mod_mask_table[] = {
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
  char_u  *name;        /* Name of key */
} key_names_table[] = {
  {' ',               (char_u *)"Space"},
  {TAB,               (char_u *)"Tab"},
  {NL,                (char_u *)"NL"},
  {NL,                (char_u *)"NewLine"},     /* Alternative name */
  {NL,                (char_u *)"LineFeed"},    /* Alternative name */
  {NL,                (char_u *)"LF"},          /* Alternative name */
  {CAR,               (char_u *)"CR"},
  {CAR,               (char_u *)"Return"},      /* Alternative name */
  {CAR,               (char_u *)"Enter"},       /* Alternative name */
  {ESC,               (char_u *)"Esc"},
  {ESC,               (char_u *)"Escape"},
  {CSI,               (char_u *)"CSI"},
  {'|',               (char_u *)"Bar"},
  {'\\',              (char_u *)"Bslash"},
  {BS,                (char_u *)"Backspace"},
  {BS,                (char_u *)"Del"},
  {DEL,               (char_u *)"Delete"},      /* Alternative name */
  {K_UP,              (char_u *)"Up"},
  {K_DOWN,            (char_u *)"Down"},
  {K_LEFT,            (char_u *)"Left"},
  {K_RIGHT,           (char_u *)"Right"},
  {K_INS,             (char_u *)"Insert"},
  {K_INS,             (char_u *)"Ins"},         /* Alternative name */
  {K_HOME,            (char_u *)"Home"},
  {K_END,             (char_u *)"End"},
  {K_PAGEUP,          (char_u *)"PageUp"},
  {K_PAGEDOWN,        (char_u *)"PageDown"},
  {0,                 NULL}
};

static fn_reg reg_tbl[] = {
  {NUL,  {0}},
  {'0',  {0}},
  {'1',  {0}},
  {'_',  {0}},
};

static fn_oper key_operators[] = {
  {NUL,      NUL,    NULL},
  {'y',      NUL,    buf_yank},
  {'m',      NUL,    buf_mark},
  {'\'',     NUL,    buf_gomark},
  {'g',      NUL,    buf_g},
  {'d',      NUL,    buf_del},
  {Ctrl_W,   NUL,    win_move},
};

typedef struct fn_map fn_map;
struct fn_map {
  char *lhs;
  char *rhs;
  int len;
};

static fn_map *key_maps[2][256];

int get_special_key_code(char *name)
{
  char_u *table_name;
  int i, j;

  for (i = 0; key_names_table[i].name != NULL; i++) {
    table_name = key_names_table[i].name;

    for (j = 0; table_name[j] != NUL; j++)
      if (TOLOWER_ASC(table_name[j]) != TOLOWER_ASC(name[j]))
        break;
    if (table_name[j] == NUL)
      return key_names_table[i].key;
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

void input_init(void)
{
  log_msg("INIT", "INPUT");
  tk = termkey_new(0,0);
  termkey_set_flags(tk, TERMKEY_FLAG_UTF8 | TERMKEY_CANON_DELBS);

  uv_poll_init(eventloop(), &poll_handle, 0);
  uv_poll_start(&poll_handle, UV_READABLE, input_check);
}

void input_cleanup(void)
{
  for (int j = 0; j < 2; j++)
  for (int i = 0; i < 256; i++) {
    if (!key_maps[j][i])
      continue;
    free(key_maps[j][i]->lhs);
    free(key_maps[j][i]->rhs);
    free(key_maps[j][i]);
  }
  termkey_destroy(tk);
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

static int trans_special(char **bp)
{
  if (*bp[0] != '<')
    return **bp;
  char *end = strstr(*bp, ">");
  if (!end)
    return **bp;

  (*bp)++;
  *end = '\0';

  int newkey = get_special_key_code(*bp);
  int keycode = *(*bp+2);
  if (newkey)
    keycode = newkey;
  else if (strcasecmp(HC_S_TAB, *bp) == 0)
    keycode = K_S_TAB;

  int mask = 0x0;
  mask |= name_to_mod_mask(*bp[0]);

  if (!IS_SPECIAL(keycode))
    keycode  = extract_modifiers(keycode, &mask);

  *end = '>';
  *bp = (end);
  return keycode;
}

static int replace_termcodes(char **to, char *from)
{
  int len = strlen(from);
  char buf[len];
  int i = 0;
  while (*from) {
    buf[i] = trans_special(&from);
    i++;
    from++;
  }
  *to = malloc((i)*sizeof(char*));
  strncpy(*to, buf, i);
  return i;
}

void set_map(char *from, char *to)
{
  log_msg("INPUT", "<<-- from: %s to: %s", from, to);
  fn_map *mp = malloc(sizeof(fn_map));
  replace_termcodes(&mp->lhs, from);
  mp->len = replace_termcodes(&mp->rhs, to);

  fn_map *get = key_maps[0][(int)mp->lhs[0]];
  if (get) {
    free(get->lhs);
    free(get->rhs);
    free(get);
  }
  key_maps[0][(int)mp->lhs[0]] = mp;
}

bool input_map_exists(int key)
{
  //FIXME: key_maps handle negative termcap keys
  if (key < 0)
    return false;
  return key_maps[0][key];
}

void do_map(Keyarg *ca, int key)
{
  log_msg("INPUT", "<<<<<<<<<<<<<<<<<<");
  for (int i = 0; i < key_maps[0][key]->len; i++) {
    int ch = key_maps[0][key]->rhs[i];
    Keyarg ca = {.key = ch, .utf8 = (char[7]){ch,0,0,0,0,0,0}};
    window_input(&ca);
  }
}

static int process_key(TermKeyKey *key)
{
  size_t len;
  char buf[64];
  char *bp;
  len = termkey_strfkey(tk, buf, sizeof(buf), key, TERMKEY_FORMAT_VIM);

  if (buf[0] == '<') {
    bp = buf;
    bp++;
    bp[len - 2] = '\0';
    int newkey = get_special_key_code(bp);
    if (newkey)
      key->code.number = newkey;
    else if (strcasecmp(HC_S_TAB, bp) == 0)
      key->code.number = K_S_TAB;

    if (key->modifiers) {
      int mask = 0x0;
      mask |= name_to_mod_mask(*bp);
      int keycode = key->code.number;
      if (!IS_SPECIAL(keycode))
        key->code.number  = extract_modifiers(keycode, &mask);
    }
  }
  return key->code.number;
}

void input_check()
{
  log_msg("INPUT", ">>>>>>>>>>>>>>>>>>>>>>>>>");
  TermKeyKey key;

  termkey_advisereadable(tk);

  while ((termkey_getkey_force(tk, &key)) == TERMKEY_RES_KEY) {
    ca.key = process_key(&key);
    ca.utf8 = key.utf8;
    window_input(&ca);
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
void input_setup_tbl(fn_keytbl *kt)
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

void oper(void *none, Keyarg *ca)
{
  log_msg("INPUT", "oper");
  clearop(ca);
  if (ca->type == NCH_S)
    ca->oap.key = OP_G;
  else
    ca->oap.key = ca->arg;

  ca->nkey = ca->key;
}

static int do_op(Keyarg *ca, void *obj)
{
  key_operators[ca->oap.key].cmd_func(obj, ca);
  if (ca->oap.is_unused)
    return 0;

  clearop(ca);
  return 1;
}

static int find_do_op(Keyarg *ca, void *obj)
{
  log_msg("INPUT", "find_do_op");

  log_msg("INPUT", "%c %c %c", ca->key, ca->oap.key, ca->nkey);
  if (ca->type == NCH_A || ca->key == ca->nkey)
    return do_op(ca, obj);

  if (ca->type == NCH_S) {
    if (ca->nkey == key_operators[ca->oap.key].nchar)
      return do_op(ca, obj);
    else
      clearop(ca);
  }

  if (ca->type == NCH) {
    if (ca->nkey != OP_NOP)
      return do_op(ca, obj);
    else
      ca->nkey = ca->key;
  }
  clearop(ca);
  return 0;
}

void clearop(Keyarg *ca)
{
  log_msg("INPUT", "clear_oparg");
  memset(&ca->oap, 0, sizeof(Oparg));
  ca->nkey = OP_NOP;
  ca->mkey = OP_NOP;
  ca->opcount = 0;
}

bool op_pending(Keyarg *arg)
{
  return (arg->oap.key != OP_NOP);
}

int find_do_key(fn_keytbl *kt, Keyarg *ca, void *obj)
{
  log_msg("INPUT", "dokey");
  if (op_pending(ca)) {
    if (ca->key == ESC) {
      clearop(ca);
      reg_clear_dcur();
      return 0;
    }
    if (ca->optbl == kt) {
      int ret = find_do_op(ca, obj);
      if (ret)
        return ret;
    }
  }

  if (input_map_exists(ca->key)) {
    do_map(ca, ca->key);
    return 1;
  }

  if (ISDIGIT(ca->key)) {
    ca->opcount *= 10;
    ca->opcount += ca->key - '0';
    return 1;
  }

  int idx = find_command(kt, ca->key);
  if (idx >= 0) {
    ca->arg  = kt->tbl[idx].cmd_arg;
    ca->type = kt->tbl[idx].cmd_flags;
    ca->optbl = kt;
    kt->tbl[idx].cmd_func(obj, ca);
    ca->opcount = 0;
    return 1;
  }

  return 0;
}

fn_reg* reg_get(int ch)
{
  for (int i = 0; i < LENGTH(reg_tbl); i++)
    if (reg_tbl[i].key == ch)
      return &reg_tbl[i];
  return NULL;
}

fn_reg* reg_dcur()
{
  fn_reg *reg1 = reg_get('1');
  if (reg1->value.argc > 0)
    return reg1;
  return reg_get(NUL);
}

void reg_clear_dcur()
{
  reg_set('1', (varg_T){0,0});
}

void reg_set(int ch, varg_T args)
{
  log_msg("INPUT", "reg_set");
  fn_reg *find = reg_get(ch);
  if (!find)
    return;
  if (find->value.argc > 0) {
    for (int i = 0; i < find->value.argc; i++)
      free(find->value.argv[i]);
    free(find->value.argv);
  }
  find->value = args;

  if (ch == '0') {
    char *cpy;
    char *src = lines2yank(args.argc, args.argv);
    asprintf(&cpy, "echo -n \"%s\" | %s", src, get_opt_str("copy-pipe"));
    shell_exec(cpy, NULL, focus_dir(), NULL);
    free(src);
    free(cpy);
  }
}
