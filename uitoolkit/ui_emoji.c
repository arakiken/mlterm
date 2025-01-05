/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_emoji.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_util.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_str.h>
#include <pobl/bl_debug.h>

/* --- static variables --- */

static char *emoji_path;
static char *emoji_file_format1;
static char *emoji_file_format2;
static int emoji_path_type = -1; /* -1: not checked, 0: not exist, 1: dir, 2: font file */

/* --- static functions --- */

static int has_digit_format_specifiers(const char *format, int num) {
  while (*format) {
    if (*format++ == '%') {
      while (1) {
        if (*format == '\0') {
          goto end;
        }
        if (*format < '+' || '9' < *format) {
          /* + - . 0-9 (Actually , and / should not break (lazy processing)) */
          break;
        }
        format++;
      }

      if (*format == 'd' || *format == 'o' || *format == 'u' || (*format | 0x20) == 'x') {
        if (--num < 0) {
          return 0;
        }
      }

      format++;
    }
  }

end:
  if (num == 0) {
    return 1;
  }

  return 0;
}

/* --- global functions --- */

void ui_emoji_set_path(const char *path) {
  struct stat st;

  free(emoji_path);
  emoji_path = NULL;
  emoji_path_type = -1;

  if (*path == '\0' || stat(path, &st) != 0 || (emoji_path = strdup(path)) == NULL) {
    return;
  }

  if (st.st_mode & S_IFDIR) {
    emoji_path_type = 1;
  } else {
    emoji_path_type = 2;
  }
}

void ui_emoji_set_file_format(const char *format) {
  size_t format_len;
  const char *p;

  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  format_len = strlen(format);

  if ((p = strchr(format, ','))) {
    if ((emoji_file_format2 = malloc(format_len + 1))) {
      size_t len;

      len = format_len - (p + 1 - format) + 1 /* including '\0' */;
      memcpy(emoji_file_format2, p + 1, len);
      emoji_file_format1 = emoji_file_format2 + len;

      len = p - format;
      memcpy(emoji_file_format1, format, len);
      emoji_file_format1[len] = '\0';

      if (!has_digit_format_specifiers(emoji_file_format1, 1) ||
          !has_digit_format_specifiers(emoji_file_format2, 2)) {
        free(emoji_file_format2);
        emoji_file_format1 = emoji_file_format2 = NULL;
      }
    }
  } else if (*format != '\0') {
    if ((p = strrchr(format, '.')) == NULL ||
        (format < p && *(p - 1) == '%') /* %.4x */) {
      p = format + format_len;
    }

    if ((emoji_file_format2 = malloc((p - format) + 1 /* - */ + format_len + 1))) {
      memcpy(emoji_file_format2, format, p - format);
      emoji_file_format1 = emoji_file_format2 + (p - format);
      *emoji_file_format1++ = '-';
      strcpy(emoji_file_format1, format);
    }
  }
}

char *ui_emoji_get_path(u_int32_t code1, u_int32_t code2, int check_exist) {
  char *file_path;
  struct stat st;
  size_t file_format_len;

  if (check_exist && emoji_path_type == 0) {
    return NULL;
  }

  if (emoji_path_type == 2) {
    /* emoji_path is always non-NULL. */

    if (!(file_path = malloc(strlen(emoji_path) + 2 + DIGIT_STR_LEN(u_int32_t) * 2 + 1))) {
      return NULL;
    }

    if (code2 > 0) {
      /* XXX %x-%x is not supported for now. (See gdk_pixbuf_new_from_otf() in c_imagelib.c) */
      sprintf(file_path, "%s/%x-%x", emoji_path, code1, code2);
    } else {
      sprintf(file_path, "%s/%x", emoji_path, code1);
    }

    return file_path;
  }

  /* emoji_path_type == 1 or -1 */

  if (emoji_file_format2 == NULL || (file_format_len = strlen(emoji_file_format2)) < 5) {
    file_format_len = 5; /* -.png of %.4x-%.4x.png */
  }

  if (emoji_path) {
    size_t path_len;

    path_len = strlen(emoji_path);
    if (!(file_path = malloc(path_len + file_format_len + DIGIT_STR_LEN(u_int32_t) * 2 + 1))) {
      return NULL;
    }

    memcpy(file_path, emoji_path, path_len);
    file_path[path_len] = '/';
    if (code2 > 0) {
      sprintf(file_path + path_len + 1, emoji_file_format2 ? emoji_file_format2 : "%.4x-%.4x.png",
              code1, code2);
    } else {
      sprintf(file_path + path_len + 1, emoji_file_format1 ? emoji_file_format1 : "%.4x.png",
              code1);
    }
  } else {
    if (!(file_path = alloca(13 + file_format_len + DIGIT_STR_LEN(u_int32_t) * 2 + 1))) {
      return NULL;
    }

    memcpy(file_path, "mlterm/emoji/", 13);
    if (code2 > 0) {
      sprintf(file_path + 13, emoji_file_format2 ? emoji_file_format2 : "%.4x-%.4x.png",
              code1, code2);
    } else {
      sprintf(file_path + 13, emoji_file_format1 ? emoji_file_format1 : "%.4x.png", code1);
    }

    if (!(file_path = bl_get_user_rc_path(file_path))) {
      return NULL;
    }
  }

  if (check_exist && stat(file_path, &st) != 0) {
    if (emoji_path_type == -1) {
      /* Check if the directory exists or not. */
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

  return file_path;
}

char *ui_emoji_get_file_format(void) {
  char *format;

  if ((format = malloc(strlen(emoji_file_format1) + 1 + strlen(emoji_file_format2) + 1))) {
    sprintf(format, "%s,%s", emoji_file_format1, emoji_file_format2);

    return format;
  } else {
    return NULL;
  }
}

#ifdef BL_DEBUG

#include <assert.h>

void TEST_ui_emoji(void) {
  char *orig_emoji_path = emoji_path;
  char *orig_emoji_file_format1 = emoji_file_format1;
  char *orig_emoji_file_format2 = emoji_file_format2;
  int orig_emoji_path_type = emoji_path_type;

  emoji_path = emoji_file_format1 = emoji_file_format2 = NULL;

#ifndef USE_WIN32API
  ui_emoji_set_path("/usr");
  assert(strcmp("/usr", emoji_path) == 0);
  free(emoji_path);
  emoji_path = NULL;

  ui_emoji_set_path("/usr/");
  assert(strcmp("/usr/", emoji_path) == 0);
  free(emoji_path);
  emoji_path = NULL;

  ui_emoji_set_path("/etc/profile");
  assert(strcmp("/etc/profile", emoji_path) == 0);
  free(emoji_path);
  emoji_path = NULL;
#else
  ui_emoji_set_path("c:\\Users");
  assert(strcmp("c:\\Users", emoji_path) == 0);
  free(emoji_path);
  emoji_path = NULL;
#endif

  ui_emoji_set_file_format("%.4x.png");
  assert(strcmp("%.4x.png", emoji_file_format1) == 0);
  assert(strcmp("%.4x-%.4x.png", emoji_file_format2) == 0);
  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  ui_emoji_set_file_format("%.4X.png,%.4X-%.4X.png");
  assert(strcmp("%.4X.png", emoji_file_format1) == 0);
  assert(strcmp("%.4X-%.4X.png", emoji_file_format2) == 0);
  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  ui_emoji_set_file_format("%.4X");
  assert(strcmp("%.4X", emoji_file_format1) == 0);
  assert(strcmp("%.4X-%.4X", emoji_file_format2) == 0);
  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  ui_emoji_set_file_format("%x.jpeg,%x-%x.jpeg");
  assert(strcmp("%x.jpeg", emoji_file_format1) == 0);
  assert(strcmp("%x-%x.jpeg", emoji_file_format2) == 0);
  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  ui_emoji_set_file_format("%2d-%2d.png,%2d-%2d-%2d.png");
  assert(emoji_file_format1 == NULL);
  assert(emoji_file_format2 == NULL);
  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  ui_emoji_set_file_format("%.2s.png,%.2d-%.2s.png");
  assert(emoji_file_format1 == NULL);
  assert(emoji_file_format2 == NULL);
  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  ui_emoji_set_file_format("%.2lu.png,%.2d-%.2s.png");
  assert(emoji_file_format1 == NULL);
  assert(emoji_file_format2 == NULL);
  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  ui_emoji_set_file_format("%&a%.2d.gif");
  assert(strcmp("%&a%.2d.gif", emoji_file_format1) == 0);
  assert(strcmp("%&a%.2d-%&a%.2d.gif", emoji_file_format2) == 0);
  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  ui_emoji_set_file_format("%2d-%%2d.png,%2d-%%2d-%2d.png");
  assert(strcmp("%2d-%%2d.png", emoji_file_format1) == 0);
  assert(strcmp("%2d-%%2d-%2d.png", emoji_file_format2) == 0);
  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  ui_emoji_set_file_format("%2d-%%2d.gif");
  assert(strcmp("%2d-%%2d.gif", emoji_file_format1) == 0);
  assert(strcmp("%2d-%%2d-%2d-%%2d.gif", emoji_file_format2) == 0);
  free(emoji_file_format2);
  emoji_file_format1 = emoji_file_format2 = NULL;

  bl_msg_printf("PASS ui_emoji test.\n");

  emoji_path = orig_emoji_path;
  emoji_file_format1 = orig_emoji_file_format1;
  emoji_file_format2 = orig_emoji_file_format2;
  emoji_path_type = orig_emoji_path_type;
}

#endif
