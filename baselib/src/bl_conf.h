/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_CONF_H__
#define __BL_CONF_H__

#include "bl_def.h" /* REMOVE_FUNCS_MLTERM_UNUSE */
#include "bl_types.h"
#include "bl_map.h"

/*
 * all members should be allocated on the caller side.
 */
typedef struct bl_arg_opt {
  char opt;
  const char *long_opt;
  int is_boolean;
  char *key;
  const char *help;

} bl_arg_opt_t;

/*
 * all members are allocated internally.
 */
typedef struct bl_conf_entry {
  char *value;
#ifndef REMOVE_FUNCS_MLTERM_UNUSE
  char *default_value;
#endif

} bl_conf_entry_t;

BL_MAP_TYPEDEF(bl_conf_entry, char *, bl_conf_entry_t *);

typedef struct bl_conf {
  bl_arg_opt_t **arg_opts; /* 0x20 - 0x7f */
  int num_opts;
  char end_opt;

  BL_MAP(bl_conf_entry) conf_entries;

} bl_conf_t;

void bl_init_prog(const char *path, const char *version);

const char *bl_get_prog_path(void);

bl_conf_t *bl_conf_new(void);

void bl_conf_destroy(bl_conf_t *conf);

int bl_conf_add_opt(bl_conf_t *conf, char short_opt, const char *long_opt, int is_boolean,
                    char *key, const char *help);

int bl_conf_set_end_opt(bl_conf_t *conf, char opt, const char *long_opt, char *key,
                        const char *help);

int bl_conf_parse_args(bl_conf_t *conf, int *argc, char ***argv, int ignore_unknown_opt);

int bl_conf_write(bl_conf_t *conf, const char *filename);

int bl_conf_read(bl_conf_t *conf, const char *filename);

char *bl_conf_get_value(bl_conf_t *conf, char *key);

#ifndef REMOVE_FUNCS_MLTERM_UNUSE
int bl_conf_set_default_value(bl_conf_t *conf, char *key, char *default_value);
#endif

char *bl_conf_get_version(bl_conf_t *conf);

#endif
