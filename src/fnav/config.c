#include <unistd.h>
#include <wordexp.h>

#include "fnav/log.h"
#include "fnav/config.h"
#include "fnav/option.h"
#include "fnav/cmdline.h"
#include "fnav/cmd.h"
#include "fnav/info.h"

static void* edit_setting();
static void* edit_color();
static void* edit_variable();
static void* edit_mapping();
static void* add_source();

static const Cmd_T cmdtable[] = {
  {"set",    edit_setting,   0},
  {"hi",     edit_color,     0},
  {"let",    edit_variable,  0},
  {"map",    edit_mapping,   0},
  {"so",     add_source,     0},
  {"source", add_source,     0},
};

static const char *config_paths[] = {
  "$HOME/.fnavrc",
  "$HOME/.fnav/.fnavrc",
  "/etc/fnav/.fnavrc",
};

static const char *info_paths[] = {
  "$HOME/.fnavinfo",
  "$HOME/.fnav/.fnavinfo",
  "/etc/fnav/.fnavinfo",
};

char *p_sh = "/bin/sh";
char *p_cp = "cp";
char *p_mv = "mv";
char *p_rm = "rm";

void config_init()
{
  for (int i = 0; i < LENGTH(cmdtable); i++) {
    Cmd_T *cmd = malloc(sizeof(Cmd_T));
    cmd = memmove(cmd, &cmdtable[i], sizeof(Cmd_T));
    cmd_add(cmd);
  }
}

static bool file_exists(const char *path)
{
  return access(path, R_OK) != -1;
}

char* strip_whitespace(char *_str)
{
  if (*_str == '\0')
    return _str;

  char *strold = _str;
  while (*_str == ' ' || *_str == '\t')
    _str++;

  char *str = strdup(_str);
  free(strold);
  int i;

  /* seek forwards */
  for (i = 0; str[i] != '\0'; ++i);

  while (i >= 0 && (str[i] == ' ' || str[i] == '\t'))
    i--;

  str[i] = '\0';
  return str;
}

static char* get_config_path(const char **paths, ssize_t len)
{
  wordexp_t p;
  char *path;

  int i;
  for (i = 0; i < (int)(len / sizeof(char *)); ++i) {
    if (wordexp(paths[i], &p, 0) == 0) {
      path = p.we_wordv[0];
      if (file_exists(path)) {
        path = strdup(path);
        wordfree(&p);
        return path;
      }
      wordfree(&p);
    }
  }
  return NULL;
}

static char* read_line(FILE *file)
{
  int length = 0, size = 128;
  char *string = malloc(size);
  if (!string)
    return NULL;

  while (1) {
    int c = getc(file);
    if (c == EOF || c == '\n' || c == '\0')
      break;
    if (c == '\r')
      continue;

    if (length == size) {
      char *new_string = realloc(string, size *= 2);
      if (!new_string) {
        free(string);
        return NULL;
      }
      string = new_string;
    }
    string[length++] = c;
  }
  if (length + 1 == size) {
    char *new_string = realloc(string, length + 1);
    if (!new_string) {
      free(string);
      return NULL;
    }
    string = new_string;
  }
  string[length] = '\0';
  return string;
}

static FILE* config_open(const char *file, const char **defaults, char *mode)
{
  log_msg("CONFIG", "config_open");
  char *path;
  if (file)
    path = strdup(file);
  else
    path = get_config_path(defaults, sizeof(defaults));

  if (!path) {
    log_msg("CONFIG", "Unable to find a config file!");
    return NULL;
  }

  FILE *f = fopen(path, mode);
  free(path);
  if (!f) {
    fprintf(stderr, "Unable to open %s for reading", path);
    return NULL;
  }
  return f;
}

void config_load(const char *file)
{
  FILE *f = config_open(file, config_paths, "r");
  config_read(f);
  fclose(f);
}

void config_load_defaults()
{
  config_load(NULL);

  FILE *f = config_open(NULL, info_paths, "r");
  info_read(f);
  fclose(f);
}

void config_write_info()
{
  log_msg("CONFIG", "config_write_info");
  FILE *f = config_open(NULL, info_paths, "w");
  info_write_file(f);
  fclose(f);
}

bool info_read(FILE *file)
{
  int line_number = 0;
  char *line;
  while (!feof(file)) {
    line = read_line(file);
    line_number++;
    if (line[0] == '#' || !strpbrk(&line[0], "?/:'@")) {
      free(line);
      continue;
    }
    info_parse(line);
    free(line);
  }
  return 1;
}

bool config_read(FILE *file)
{
  int line_number = 0;
  char *line;
  while (!feof(file)) {
    line = read_line(file);
    line_number++;
    line = strip_whitespace(line);
    if (line[0] == '#' || strlen(line) < 1) {
      free(line);
      continue;
    }
    Cmdline cmd;
    cmdline_init_config(&cmd, line);
    cmdline_build(&cmd);
    cmdline_req_run(&cmd);
    cmdline_cleanup(&cmd);
    free(line);
  }
  return 1;
}

static void* edit_setting(List *args)
{
  log_msg("CONFIG", "edit_setting");
  return 0;
}

static void* edit_color(List *args)
{
  log_msg("CONFIG", "edit_color");
  if (utarray_len(args->items) < 3)
    return 0;

  int fg, bg;
  int err = 0;
  err += str_num(list_arg(args, 2, VAR_STRING), &fg);
  err += str_num(list_arg(args, 3, VAR_STRING), &bg);
  if (!err)
    return 0;

  fn_color col = {
    .key = strdup(list_arg(args, 1, VAR_STRING)),
    .fg = fg,
    .bg = bg,
  };
  set_color(&col);
  return 0;
}

static void* edit_variable(List *args)
{
  log_msg("CONFIG", "edit_variable");
  return 0;
}

static void* edit_mapping(List *args)
{
  return 0;
}

static void* add_source(List *args)
{
  String file = list_arg(args, 1, VAR_STRING);
  if (!file)
    return 0;

  wordexp_t p;
  char *path;

  if (wordexp(file, &p, 0) == 0) {
    path = p.we_wordv[0];
    if (file_exists(path)) {
      config_load(path);
    }
  }
  return 0;
}
