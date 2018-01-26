/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_emoji.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_util.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_str.h>

/* --- static variables --- */

static char *emoji_path;
static int emoji_path_type = -1; /* -1: not checked, 0: not exist, 1: dir, 2: font file */

/* --- global functions --- */

void ui_emoji_set_path(const char *path) {
  struct stat st;

  free(emoji_path);

  if (*path == '\0' || stat(path, &st) != 0) {
    emoji_path = NULL;
    emoji_path_type = 0;
  }

  if (st.st_mode & S_IFDIR) {
    emoji_path_type = 1;
  } else {
    emoji_path_type = 2;
  }

  emoji_path = strdup(path);
}

char *ui_emoji_get_path(u_int32_t code1, u_int32_t code2) {
  char *file_path;
  struct stat st;

  if (emoji_path_type == 0) {
    return NULL;
  }

  if (emoji_path) {
    /* emoji_path_type == 1 | 2 */
    if (!(file_path = malloc(strlen(emoji_path) + 5 + DIGIT_STR_LEN(u_int32_t) * 2 + 1))) {
      return NULL;
    }

    if (code2 > 0) {
      sprintf(file_path, "%s/%x-%x", emoji_path, code1, code2);
    } else {
      sprintf(file_path, "%s/%x", emoji_path, code1);
    }

    if (emoji_path_type == 2) {
      return file_path;
    }

    strcat(file_path, ".gif");
  } else {
    if (!(file_path = alloca(18 + DIGIT_STR_LEN(u_int32_t) * 2 + 1))) {
      return NULL;
    }

    if (code2 > 0) {
      sprintf(file_path, "mlterm/emoji/%x-%x.gif", code1, code2);
    } else {
      sprintf(file_path, "mlterm/emoji/%x.gif", code1);
    }

    if (!(file_path = bl_get_user_rc_path(file_path))) {
      return NULL;
    }
  }

  if (stat(file_path, &st) != 0) {
    strcpy(file_path + strlen(file_path) - 3, "png");
    if (stat(file_path, &st) != 0) {
      if (emoji_path_type == -1) {
        char *p = strrchr(file_path, '/'); /* always returns non-NULL value */
        *p = '\0';
        if (stat(file_path, &st) == 0) {
          emoji_path_type = 1;
        } else {
          emoji_path_type = 0;
        }
      }

      free(file_path);

      return NULL;
    }
  }

  return file_path;
}
