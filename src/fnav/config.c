#include <unistd.h>
#include <wordexp.h>

#include "fnav/log.h"
#include "fnav/config.h"
#include "fnav/option.h"
#include "fnav/cmdline.h"
#include "fnav/cmd.h"

static void* edit_setting();
static void* edit_color();
static void* edit_variable();
static void* edit_mapping();
static void* add_source();

#define CMDS_SIZE ARRAY_SIZE(cmdtable)
static const Cmd_T cmdtable[] = {
  {"set",    edit_setting,   0},
  {"hi",     edit_color,     0},
  {"let",    edit_variable,  0},
  {"map",    edit_mapping,   0},
  {"so",     add_source,     0},
  {"source", add_source,     0},
};

void config_init()
{
  for (int i = 0; i < (int)CMDS_SIZE; i++) {
    Cmd_T *cmd = malloc(sizeof(Cmd_T));
    cmd = memcpy(cmd, &cmdtable[i], sizeof(Cmd_T));
    cmd_add(cmd);
  }
}

static bool file_exists(const char *path) {
	return access(path, R_OK) != -1;
}

char* strip_whitespace(char *_str)
{
  if (*_str == '\0')
    return _str;
  char *strold = _str;
  while (*_str == ' ' || *_str == '\t') {
    _str++;
  }
  char *str = strdup(_str);
  free(strold);
  int i;
  for (i = 0; str[i] != '\0'; ++i);
  do {
    i--;
  } while (i >= 0 && (str[i] == ' ' || str[i] == '\t'));
  str[i + 1] = '\0';
  return str;
}

static char *get_config_path(void)
{
	static const char *config_paths[] = {
		"$HOME/.fnavrc",
		"$HOME/.fnav/.fnavrc",
		"/etc/fnav/.fnavrc",
	};
	wordexp_t p;
	char *path;

	int i;
	for (i = 0; i < (int)(sizeof(config_paths) / sizeof(char *)); ++i) {
		if (wordexp(config_paths[i], &p, 0) == 0) {
			path = p.we_wordv[0];
			if (file_exists(path)) {
				return path;
			}
		}
	}
  return NULL;
}

static char *read_line(FILE *file)
{
	int length = 0, size = 128;
	char *string = malloc(size);
	if (!string) {
		return NULL;
	}
	while (1) {
		int c = getc(file);
		if (c == EOF || c == '\n' || c == '\0') {
			break;
		}
		if (c == '\r') {
			continue;
		}
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

bool config_load(const char* file)
{
	log_msg("CONFIG", "config_load");
  char *path;
  if (file != NULL) {
    path = strdup(file);
  } else {
    path = get_config_path();
  }

	if (path == NULL) {
		log_msg("CONFIG", "Unable to find a config file!");
		return false;
	}

	FILE *f = fopen(path, "r");
	free(path);
	if (!f) {
		fprintf(stderr, "Unable to open %s for reading", path);
		return false;
	}

	bool config_load_success;
	config_load_success = config_read(f);

	fclose(f);

	return config_load_success;
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
      continue;
    }
    log_msg("CMD", "conf: %s", line);
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
  log_msg("CMD", "edit_setting");
  return 0;
}

static void* edit_color(List *args)
{
  log_msg("CMD", "edit_color");
  if (utarray_len(args->items) < 3) return 0;
  Token *word = (Token*)utarray_eltptr(args->items, 1);
  Token *arg1 = (Token*)utarray_eltptr(args->items, 2);
  Token *arg2 = (Token*)utarray_eltptr(args->items, 3);

  fn_color *col = malloc(sizeof(fn_color));
  col->key = strdup(TOKEN_STR(word->var));
  col->fg  = TOKEN_NUM(arg1->var);
  col->bg  = TOKEN_NUM(arg2->var);
  set_color(col);
  return 0;
}

static void* edit_variable(List *args)
{
  log_msg("CMD", "edit_variable");
  return 0;
}

static void* edit_mapping(List *args)
{
  return 0;
}

static void* add_source(List *args)
{
  if (utarray_len(args->items) < 1) return 0;
  Token *word = (Token*)utarray_eltptr(args->items, 1);
  String file = TOKEN_STR(word->var);

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
