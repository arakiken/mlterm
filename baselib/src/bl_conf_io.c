/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_conf_io.h"

#include <stdio.h>  /* sprintf */
#include <string.h> /* strlen */
#include <stdlib.h> /* getenv */
#ifndef USE_WIN32API
#include <sys/stat.h>
#endif

#include "bl_str.h" /* bl_str_sep/bl_str_chop_spaces */
#include "bl_mem.h" /* malloc */
#include "bl_path.h"
#include "bl_debug.h"

/* --- static variables --- */

static const char *sysconfdir;

/* --- global functions --- */

void bl_set_sys_conf_dir(const char *dir) {
  sysconfdir = dir;
}

char *bl_get_sys_rc_path(const char *rcfile) {
  char *rcpath;

  if (sysconfdir == NULL) {
    return NULL;
  }

  if ((rcpath = malloc(strlen(sysconfdir) + 1 + strlen(rcfile) + 1)) == NULL) {
    return NULL;
  }

#ifdef USE_WIN32API
  sprintf(rcpath, "%s\\%s", sysconfdir, rcfile);
#else
  sprintf(rcpath, "%s/%s", sysconfdir, rcfile);
#endif

  return rcpath;
}

char *bl_get_user_rc_path(const char *rcfile) {
  char *homedir;
  char *dotrcpath;

#ifdef DEBUG
  if ((homedir = getenv("CONF_DIR"))) {
    bl_msg_printf("using %s as an user config dir.\n", homedir);
    /* conf path is overridden */;
  } else
#endif
      if ((homedir = bl_get_home_dir()) == NULL) {
    return NULL;
  }

#ifdef USE_WIN32API
  /* Enough for "%s\%s" */
  if ((dotrcpath = malloc(strlen(homedir) + 1 + strlen(rcfile) + 1))) {
    /* subdir doesn't contain "." in win32 native. */
    sprintf(dotrcpath, "%s\\%s", homedir, rcfile);
  }
#else
  /* Enough for "%s/.config/%s" */
  if ((dotrcpath = malloc(strlen(homedir) + 9 + strlen(rcfile) + 1))) {
    struct stat st;
    char *p;

    sprintf(dotrcpath, "%s/.config/%s", homedir, rcfile);
    p = strrchr(dotrcpath, '/');
    if (p > dotrcpath + strlen(homedir) + 8) {
      *p = '\0';
      if (stat(dotrcpath, &st) == 0) {
        *p = '/';

        /* ~/.config/mlterm exists. */
        goto end;
      }
    }

    sprintf(dotrcpath, "%s/.%s", homedir, rcfile);
  }
end:
#endif

  return dotrcpath;
}

bl_conf_write_t *bl_conf_write_open(char *name /* can break in this function. */
                                    ) {
  bl_conf_write_t *conf;
  bl_file_t *from;

  if ((conf = malloc(sizeof(bl_conf_write_t))) == NULL) {
    return conf;
  }

  if ((conf->lines = malloc(sizeof(char *) * 128)) == NULL) {
    free(conf);

    return NULL;
  }

  conf->num = 0;
  conf->scale = 1;

  from = bl_file_open(name, "r");
  if (from) {
    while (1) {
      char *line;
      size_t len;

      if (conf->num >= conf->scale * 128) {
        void *p;

        if ((p = realloc(conf->lines, sizeof(char *) * 128 * (conf->scale + 1))) == NULL) {
          goto error;
        }

        conf->scale++;
        conf->lines = p;
      }

      if ((line = bl_file_get_line(from, &len)) == NULL) {
        break;
      }

      conf->lines[conf->num++] = strdup(line);
    }

    bl_file_close(from);
  }

  if ((conf->to = bl_fopen_with_mkdir(name, "w")) == NULL) {
    goto error;
  }

  bl_file_lock(fileno(conf->to));

  return conf;

error : {
  u_int count;

  for (count = 0; count < conf->num; count++) {
    free(conf->lines[count]);
  }
}

  free(conf->lines);
  free(conf);

  return NULL;
}

int bl_conf_io_write(bl_conf_write_t *conf, const char *key, const char *val) {
  u_int count;
  char *p;

  if (key == NULL) {
    return 0;
  }

  if (val == NULL) {
    val = "\0";
  }

  for (count = 0; count < conf->num; count++) {
    if (*conf->lines[count] == '#') {
      continue;
    }

    p = conf->lines[count];

    while (*p == ' ' || *p == '\t') {
      p++;
    }

    if (strncmp(p, key, strlen(key)) != 0) {
      continue;
    }

    if ((p = malloc(strlen(key) + strlen(val) + 4)) == NULL) {
      continue;
    }
    sprintf(p, "%s = %s", key, val);

    free(conf->lines[count]);
    conf->lines[count] = p;

    return 1;
  }

  if (conf->num + 1 >= conf->scale * 128) {
    void *p;

    if ((p = realloc(conf->lines, sizeof(char *) * 128 * (++conf->scale))) == NULL) {
      return 0;
    }

    conf->lines = p;
  }

  if ((p = malloc(strlen(key) + strlen(val) + 4)) == NULL) {
    return 0;
  }
  sprintf(p, "%s = %s", key, val);

  conf->lines[conf->num++] = p;

  return 1;
}

void bl_conf_write_close(bl_conf_write_t *conf) {
  u_int count;

  for (count = 0; count < conf->num; count++) {
    fprintf(conf->to, "%s\n", conf->lines[count]);
    free(conf->lines[count]);
  }

  bl_file_unlock(fileno(conf->to));

  fclose(conf->to);

  free(conf->lines);
  free(conf);
}

int bl_conf_io_read(bl_file_t *from, char **key, char **val) {
  char *line;
  size_t len;

  while (1) {
    if ((line = bl_file_get_line(from, &len)) == NULL) {
      return 0;
    }

    if (len == 0 || *line == '#') {
      /* empty line or comment out */
      continue;
    }

    /*
     * finding key
     */

    while (*line == ' ' || *line == '\t') {
      line++;
    }

    *key = bl_str_sep(&line, "=");
    if (line == NULL) {
      /* not a conf line */

      continue;
    }

    *key = bl_str_chop_spaces(*key);

    /*
     * finding value
     */

    while (*line == ' ' || *line == '\t') {
      line++;
    }

    *val = bl_str_chop_spaces(line);

    /* Remove #comment after key=value. */
    if ((line = strrchr(line, '#')) && (*(--line) == ' ' || *line == '\t')) {
      *line = '\0';
      *val = bl_str_chop_spaces(*val);
    }

    return 1;
  }
}
