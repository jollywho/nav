#ifndef NV_CONFIG_H
#define NV_CONFIG_H

#include <stdio.h>
#include <stdbool.h>

void config_init();
void config_start(char *);
bool config_read(FILE *);
void config_load(const char *);
bool config_load_info();
void config_write_info();

typedef struct ConfFile ConfFile;
struct ConfFile {
  ConfFile *parent;
  char *path;
};

#endif
