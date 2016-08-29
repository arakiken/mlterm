/*
 *	$Id$
 */

#include "bl_conf.h"

#include <stdio.h>
#include <string.h> /* memset */

#include "bl_str.h" /* bl_str_sep/strdup */
#include "bl_mem.h" /* malloc/alloca */
#include "bl_file.h"
#include "bl_conf_io.h"
#include "bl_args.h"
#include "bl_util.h" /* DIGIT_STR_LEN */

#define CH2IDX(ch) ((ch)-0x20)

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static char *prog_path;
static char *prog_name;
static char *prog_version;

/* --- static functions --- */

static void __exit(bl_conf_t *conf, int status) {
#ifdef DEBUG
  bl_conf_delete(conf);
  bl_mem_free_all();
#endif

  exit(status);
}

static void version(bl_conf_t *conf) { printf("%s version %s\n", prog_name, prog_version); }

static void usage(bl_conf_t *conf) {
  int count;
  bl_arg_opt_t *end_opt;

  printf("usage: %s", prog_name);

  for (count = 0; count < conf->num_of_opts; count++) {
    if (conf->arg_opts[count] != NULL && conf->arg_opts[count]->opt != conf->end_opt) {
      printf(" [options]");

      break;
    }
  }

  if (conf->end_opt > 0) {
    printf(" -%c ...", conf->end_opt);
  }

  printf("\n\noptions:\n");

  end_opt = NULL;
  for (count = 0; count < conf->num_of_opts; count++) {
    if (conf->arg_opts[count] != NULL) {
      if (conf->arg_opts[count]->opt == conf->end_opt) {
        end_opt = conf->arg_opts[count];
      } else {
        char *str;
        size_t len;

        len = 3 + 8 + 1;
        if (conf->arg_opts[count]->long_opt) {
          len += (3 + strlen(conf->arg_opts[count]->long_opt) + 1);
        }

        if ((str = alloca(len)) == NULL) {
#ifdef DEBUG
          bl_warn_printf(BL_DEBUG_TAG " alloca() failed.\n");
#endif

          return;
        }

        /* 3 bytes */
        if (conf->arg_opts[count]->opt) {
          sprintf(str, " -%c", conf->arg_opts[count]->opt);
        } else {
          strcpy(str, "   ");
        }

        if (conf->arg_opts[count]->long_opt) {
          /* 3 bytes */
          strcat(str, conf->arg_opts[count]->opt ? "/--" : " --");

          strcat(str, conf->arg_opts[count]->long_opt);
        }

        if (conf->arg_opts[count]->is_boolean) {
          /* 8 bytes or ... */
          strcat(str, "(=bool) ");
        } else {
          /* 7 bytes */
          strcat(str, "=value ");
        }

        printf("%-20s: %s\n", str, conf->arg_opts[count]->help);
      }
    }
  }

  if (end_opt) {
    printf("\nend option:\n -%c", end_opt->opt);

    if (end_opt->long_opt) {
      printf(" --%s", end_opt->long_opt);
    }

    printf(" ... : %s\n", end_opt->help);
  }

  printf("\nnotice:\n");
  printf("(=bool) is \"=true\" or \"=false\".\n");
}

static bl_conf_entry_t *create_new_conf_entry(bl_conf_t *conf, char *key) {
  bl_conf_entry_t *entry;
  int result;

  if ((entry = malloc(sizeof(bl_conf_entry_t))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc() failed.\n");
#endif

    return NULL;
  }
  memset(entry, 0, sizeof(bl_conf_entry_t));

  if ((key = strdup(key)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " strdup() failed.\n");
#endif

    free(entry);

    return NULL;
  }

  bl_map_set(result, conf->conf_entries, key, entry);
  if (!result) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " bl_map_set() failed.\n");
#endif

    free(key);
    free(entry);

    return NULL;
  }

  return entry;
}

/* --- global functions --- */

int bl_init_prog(char *path,   /* should be static data */
                 char *version /* should be static data */
                 ) {
  prog_path = path;

  if ((prog_name = strrchr(path, '/')) || (prog_name = strrchr(path, '\\'))) {
    prog_name++;
  } else {
    prog_name = prog_path;
  }

  prog_version = version;

  return 1;
}

char *bl_get_prog_path(void) { return prog_path; }

bl_conf_t *bl_conf_new(void) {
  bl_conf_t *conf;

  if ((conf = malloc(sizeof(bl_conf_t))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc() failed.\n");
#endif

    return NULL;
  }

  conf->num_of_opts = 0x60;

  if ((conf->arg_opts = malloc(conf->num_of_opts * sizeof(bl_arg_opt_t *))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc() failed.\n");
#endif

    free(conf);

    return NULL;
  }

  memset(conf->arg_opts, 0, conf->num_of_opts * sizeof(bl_arg_opt_t *));

  conf->end_opt = '\0';

  bl_map_new_with_size(char *, bl_conf_entry_t *, conf->conf_entries, bl_map_hash_str,
                       bl_map_compare_str_nocase, 16);

  return conf;
}

int bl_conf_delete(bl_conf_t *conf) {
  int count;
  BL_PAIR(bl_conf_entry) * pairs;
  u_int size;

  for (count = 0; count < conf->num_of_opts; count++) {
    if (conf->arg_opts[count]) {
      free(conf->arg_opts[count]);
    }
  }

  free(conf->arg_opts);

  bl_map_get_pairs_array(conf->conf_entries, pairs, size);

  for (count = 0; count < size; count++) {
    free(pairs[count]->key);
    free(pairs[count]->value->value);
#ifndef REMOVE_FUNCS_MLTERM_UNUSE
    free(pairs[count]->value->default_value);
#endif
    free(pairs[count]->value);
  }

  bl_map_delete(conf->conf_entries);

  free(conf);

  return 1;
}

int bl_conf_add_opt(bl_conf_t *conf, char short_opt, /* '\0' is accepted */
                    char *long_opt,                  /* should be static data. NULL is accepted */
                    int is_boolean, char *key,       /* should be static data */
                    char *help                       /* should be static data */
                    ) {
  bl_arg_opt_t **opt;

  if (short_opt == '\0') {
    bl_arg_opt_t **arg_opts;

    if (long_opt == NULL) {
      /* it is not accepted that both opt and long_opt are NULL. */
      return 0;
    }

    if ((arg_opts = realloc(conf->arg_opts, (conf->num_of_opts + 1) * sizeof(bl_arg_opt_t *))) ==
        NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " realloc() failed.\n");
#endif

      return 0;
    }

    conf->arg_opts = arg_opts;
    opt = &arg_opts[conf->num_of_opts++];
    *opt = NULL;
  } else if (short_opt < ' ') {
    return 0;
  } else {
    opt = &conf->arg_opts[CH2IDX(short_opt)];
  }

  if (*opt == NULL) {
    if ((*opt = malloc(sizeof(bl_arg_opt_t))) == NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " malloc() failed.\n");
#endif

      return 0;
    }
  }

  (*opt)->opt = short_opt;
  (*opt)->long_opt = long_opt;
  (*opt)->key = key;
  (*opt)->is_boolean = is_boolean;
  (*opt)->help = help;

  return 1;
}

int bl_conf_set_end_opt(bl_conf_t *conf, char opt, char *long_opt, /* should be static data */
                        char *key,                                 /* should be static data */
                        char *help                                 /* should be static data */
                        ) {
  conf->end_opt = opt;

  /* is_boolean is always true */
  return bl_conf_add_opt(conf, opt, long_opt, 1, key, help);
}

int bl_conf_parse_args(bl_conf_t *conf, int *argc, char ***argv, int ignore_unknown_opt) {
  char *opt_name;
  char *opt_val;
  BL_PAIR(bl_conf_entry) pair;
  bl_conf_entry_t *entry;

  /* passing argv[0] 'cause it may be the program name. */
  (*argv)++;
  (*argc)--;

  while (bl_parse_options(&opt_name, &opt_val, argc, argv)) {
    char short_opt;
    bl_arg_opt_t *opt;

    if (strlen(opt_name) == 1) {
      short_opt = *opt_name;

      if (short_opt < ' ' || (opt = conf->arg_opts[CH2IDX(short_opt)]) == NULL) {
        if (ignore_unknown_opt) {
          continue;
        }

        goto error_with_msg;
      }
    } else if (strlen(opt_name) > 1) {
      /* long opt -> short opt */

      int count;

      opt = NULL;

      for (count = 0; count < conf->num_of_opts; count++) {
        if (conf->arg_opts[count] && conf->arg_opts[count]->long_opt &&
            strcmp(opt_name, conf->arg_opts[count]->long_opt) == 0) {
          opt = conf->arg_opts[count];

          break;
        }
      }

      if (!opt) {
        if (ignore_unknown_opt) {
          continue;
        }

        goto error_with_msg;
      }

      short_opt = opt->opt;
    } else {
      if (ignore_unknown_opt) {
        continue;
      }

      goto error_with_msg;
    }

    bl_map_get(conf->conf_entries, opt->key, pair);
    if (!pair) {
      if ((entry = create_new_conf_entry(conf, opt->key)) == NULL) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " create_new_conf_entry() failed.\n");
#endif

        return 0;
      }
    } else {
      entry = pair->value;

      if (entry->value) {
        free(entry->value);
      }
    }

    if (short_opt == 'h') {
      usage(conf);

      __exit(conf, 1);
    } else if (short_opt == 'v') {
      version(conf);

      __exit(conf, 1);
    }

    if (opt->is_boolean) {
      if (opt_val) {
        /* "-[opt]=true" format */

        entry->value = strdup(opt_val);
      } else if (*argc && (*argv)[0] != NULL &&
                 (strcmp((*argv)[0], "true") == 0 || strcmp((*argv)[0], "false") == 0)) {
        /* "-[opt] true" format */

        entry->value = strdup((*argv)[0]);
        (*argv)++;
        (*argc)--;
      } else {
        /* "-[opt]" format */

        entry->value = strdup("true");
      }
    } else {
      if (opt_val == NULL) {
        /* "-[opt] [opt_val]" format */

        if (*argc == 0 || (*argv)[0] == NULL) {
          bl_msg_printf("%s option requires value.\n", opt_name);

          entry->value = NULL;

          goto error;
        }

        entry->value = strdup((*argv)[0]);
        (*argv)++;
        (*argc)--;
      } else {
        /* "-[opt]=[opt_val]" format */

        entry->value = strdup(opt_val);
      }
    }

    if (short_opt == conf->end_opt) {
      /* the value of conf->end_opt should be "true" */

      break;
    }
  }

  return 1;

error_with_msg:
  bl_msg_printf("%s is unknown option.\n", opt_name);

error:
  usage(conf);

  return 0;
}

int bl_conf_write(bl_conf_t *conf, char *filename) {
  FILE *to;
  BL_PAIR(bl_conf_entry) * pairs;
  u_int size;
  int count;

  if (!(to = fopen(filename, "w"))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " %s couldn't be opened.\n", filename);
#endif

    return 0;
  }

  bl_map_get_pairs_array(conf->conf_entries, pairs, size);

  for (count = 0; count < size; count++) {
    fprintf(to, "%s = %s\n", pairs[count]->key, pairs[count]->value->value
#ifndef REMOVE_FUNCS_MLTERM_UNUSE
                                                    ? pairs[count]->value->value
                                                    : pairs[count]->value->default_value
#endif
            );
  }

  fclose(to);

  return 1;
}

int bl_conf_read(bl_conf_t *conf, char *filename) {
  bl_file_t *from;
  char *key;
  char *value;
  bl_conf_entry_t *entry;
  BL_PAIR(bl_conf_entry) pair;

  if (!(from = bl_file_open(filename, "r"))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " %s couldn't be opened.\n", filename);
#endif

    return 0;
  }

  while (bl_conf_io_read(from, &key, &value)) {
    value = strdup(value);

    bl_map_get(conf->conf_entries, key, pair);
    if (!pair) {
      if ((entry = create_new_conf_entry(conf, key)) == NULL) {
        return 0;
      }
    } else {
      entry = pair->value;

      if (entry->value) {
        free(entry->value);
      }
    }

    entry->value = value;
  }

  bl_file_close(from);

  return 1;
}

char *bl_conf_get_value(bl_conf_t *conf, char *key) {
  BL_PAIR(bl_conf_entry) pair;

  bl_map_get(conf->conf_entries, key, pair);

  if (!pair) {
#ifdef __DEBUG
    bl_warn_printf(BL_DEBUG_TAG " no such key[%s] in conf map.\n", key);
#endif

    return NULL;
  } else {
    return pair->value->value
#ifndef REMOVE_FUNCS_MLTERM_UNUSE
               ? pair->value->value
               : pair->value->default_value
#endif
        ;
  }
}

#ifndef REMOVE_FUNCS_MLTERM_UNUSE
int bl_conf_set_default_value(bl_conf_t *conf, char *key, char *default_value) {
  bl_conf_entry_t *entry;
  BL_PAIR(bl_conf_entry) pair;

  key = strdup(key);

  bl_map_get(conf->conf_entries, key, pair);
  if (!pair) {
    if ((entry = create_new_conf_entry(conf, key)) == NULL) {
      return 0;
    }
  } else {
    entry = pair->value;

    free(entry->default_value);
  }

  entry->default_value = default_value;

  return 1;
}

#endif
