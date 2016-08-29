/*
 *	$Id$
 */

#include "bl_args.h"

#include <string.h> /* strchr */

#include "bl_debug.h"

#if 0
#define __DEBUG
#endif

/* --- global functions --- */

/*
 * supported option syntax.
 *
 *  -x(=xxx)
 *  --x(=xxx)
 *  -xxx(=xxx)
 *  --xxx(=xxx)
 *
 *  "--" cancels parsing options.
 *
 * !! NOTICE !!
 * after bl_parse_options() , argv points to an argument next to a successfully
 *parsed one.
 */

int bl_parse_options(char **opt, char **opt_val, int *argc, char ***argv) {
  char *arg_p;

  if (*argc == 0 || (arg_p = (*argv)[0]) == NULL) {
    /* end of argv */

    return 0;
  }

  if (*arg_p != '-') {
    /* not option */

    return 0;
  }
  arg_p++;

  if (*arg_p == '-') {
    arg_p++;

    if (*arg_p == '\0') {
      /* "--" */

      return 0;
    }
  }

  *opt = arg_p;

  if ((arg_p = strchr(arg_p, '=')) == NULL) {
    *opt_val = NULL;
  } else {
    *arg_p = '\0';
    *opt_val = arg_p + 1;
  }

  (*argv)++;
  (*argc)--;

  return 1;
}

char **_bl_arg_str_to_array(char **argv, int *argc, char *args) {
  char *args_dup;
  char *p;

  /*
   * parsing options.
   */

  *argc = 0;
  args_dup = args;
  if ((args = bl_str_alloca_dup(args)) == NULL) {
    return NULL;
  }

  p = args_dup;

  while (*args) {
    int quoted;

    while (*args == ' ' /* || *args == '\t' */) {
      if (*(++args) == '\0') {
        goto parse_end;
      }
    }

    if (*args == '\"' || *args == '\'') {
      quoted = 1;
      args++;
    } else {
      quoted = 0;
    }

    while (*args) {
      if (quoted) {
        if (*args == '\"' || *args == '\'') {
          args++;

          break;
        }
      } else {
        if (*args == ' ' /* || *args == '\t' */) {
          args++;

          break;
        }
      }

      if (*args == '\\' && (args[1] == '\"' || args[1] == '\'' ||
                            (!quoted && (args[1] == ' ' /* || args[1] == '\t' */)))) {
        *(p++) = *(++args);
      } else {
        *(p++) = *args;
      }

      args++;
    }

    *(p++) = '\0';
    argv[(*argc)++] = args_dup;
    args_dup = p;
  }

parse_end:
  /* NULL terminator (POSIX exec family style) */
  argv[*argc] = NULL;

  return argv;
}

#ifdef __DEBUG

#include <stdio.h> /* printf */

int main(void) {
  int argc;
  char **argv;
  char args[] = "mlclient -l \"hoge fuga \\\" \" \' a b c \' \\\' \\\" a\\ b \"a\\ b\"";
  char *argv_correct[] = {"mlclient", "-l", "hoge fuga \" ", " a b c ", "\'", "\"", "a b", "a\\ b"};
  int count;

  argv = bl_arg_str_to_array(&argc, args);

  printf("%d\n", argc);
  for (count = 0; count < argc; count++) {
    if (strcmp(argv_correct[count], argv[count]) != 0) {
      printf("CORRECT %s <=> WRONG %s\n", argv_correct[count], argv[count]);

      return 1;
    }
  }

  printf("PASS bl_args test.\n");

  return 0;
}

#endif
