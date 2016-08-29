/*
 *	$Id$
 */

#include "vt_config_proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>  /* open/creat */
#include <unistd.h> /* close */
#include <time.h>   /* time */
#include <sys/stat.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_util.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#if 0
#define __DEBUG
#endif

/* --- static functions --- */

static char *challenge;
static char *path;

/* --- static functions --- */

static int read_challenge(void) {
  FILE *file;
  struct stat st;

  if ((file = fopen(path, "r")) == NULL) {
    return 0;
  }

  fstat(fileno(file), &st);

  if (st.st_size > DIGIT_STR_LEN(int)) {
    return 0;
  }

  free(challenge);

  if ((challenge = malloc(DIGIT_STR_LEN(int)+1)) == NULL) {
    return 0;
  }

  fread(challenge, st.st_size, 1, file);
  challenge[st.st_size] = '\0';

  fclose(file);

  return 1;
}

/* --- global functions --- */

int vt_config_proto_init(void) {
  if ((path = bl_get_user_rc_path("mlterm/challenge")) == NULL) {
    return 0;
  }

  bl_mkdir_for_file(path, 0700);

  return vt_gen_proto_challenge();
}

int vt_config_proto_final(void) {
  free(path);
  free(challenge);

  return 1;
}

int vt_gen_proto_challenge(void) {
  int fd;

  if ((fd = creat(path, 0600)) == -1) {
    bl_error_printf("Failed to create %s.\n", path);

    return 0;
  }

  free(challenge);

  if ((challenge = malloc(DIGIT_STR_LEN(int)+1)) == NULL) {
    return 0;
  }

  srand((u_int)(time(NULL) + (int)challenge));
  sprintf(challenge, "%d", rand());

  write(fd, challenge, strlen(challenge));

  close(fd);

  return 1;
}

char *vt_get_proto_challenge(void) { return challenge; }

/*
 * Returns 0 if illegal format.
 * Returns -1 if do_challenge is 1 and challenge failed.
 */
int vt_parse_proto_prefix(char **dev, /* can be NULL */
                          char **str, int do_challenge) {
  char *p;

  p = *str;

  while (do_challenge) {
    char *chal;

    chal = p;

    if ((p = strchr(p, ';'))) {
      *(p++) = '\0';

      if ((challenge && strcmp(chal, challenge) == 0) ||
          /* Challenge could have been re-generated. */
          (read_challenge() && challenge && strcmp(chal, challenge) == 0)) {
        /* challenge succeeded. */
        break;
      }
    }

    return -1;
  }

  *str = p; /* for no_dev */

  if (strncmp(p, "/dev/", 5) == 0) {
    p += 4;
    while (*(++p) != ':') {
      /* Don't contain ';' in "/dev/...". */
      if (*p == ';' || *p == '\0') {
/* Illegal format */
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " Illegal protocol format.\n");
#endif

        goto no_dev;
      }
    }
  } else {
    if (strncmp(p, "color:", 6) == 0) {
      p += 5;
    } else {
      if (*p == 't' || *p == 'v') {
        p++;
      }
      if (*p == 'a' && *(p + 1) == 'a') {
        p += 2;
      }
      if (strncmp(p, "font:", 5) == 0) {
        p += 4;
      } else {
        goto no_dev;
      }
    }
  }

  if (dev) {
    *dev = *str;
  }

  *(p++) = '\0';
  *str = p;

  return 1;

no_dev:
  if (dev) {
    *dev = NULL;
  }

  return 1;
}

/*
 * Returns 0 if illegal format.
 * Returns -1 if do_challenge is 1 and challenge failed.
 * If finished parsing str, *str is set NULL(see *str = strchr( p , ';')).
 */
int vt_parse_proto(char **dev, /* can be NULL */
                   char **key, /* can be NULL. *key is never NULL. */
                   char **val, /* can be NULL */
                   char **str, int do_challenge, int sep_by_semicolon) {
  char *p;

  p = *str;

  if (vt_parse_proto_prefix(dev, &p, do_challenge) < 0) {
    return -1;
  }

  if (sep_by_semicolon) {
    if ((*str = strchr(p, ';'))) {
      /* *str points next key=value. */
      *((*str)++) = '\0';
    }
  } else {
    *str = NULL;
  }

  if (key) {
    *key = p;
  }

  if ((p = strchr(p, '='))) {
    *(p++) = '\0';

    if (val) {
      *val = p;
    }
  } else {
    if (val) {
      *val = NULL;
    }
  }

#ifdef __DEBUG
  bl_debug_printf("%s %s %s\n", key ? *key : NULL, val ? *val : NULL, dev ? *dev : NULL);
#endif

  return 1;
}
