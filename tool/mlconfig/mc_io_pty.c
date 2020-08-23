/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_io.h"

#include <stdio.h>
#include <pobl/bl_mem.h>    /* malloc */
#include <pobl/bl_unistd.h> /* STDIN_FILENO */
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_def.h> /* USE_WIN32API */

#include <gtk/gtk.h>

/* --- static variables --- */

static char *message;
static const char *gui; /* XXX leaked */

/* --- static functions --- */

static int append_value(const char *key, const char *value) {
  if (message == NULL) {
    if ((message = malloc(strlen(key) + 1 + strlen(value) + 1)) == NULL) {
      return 0;
    }

    sprintf(message, "%s=%s", key, value);
  } else {
    void *p;
    int len;

    len = strlen(message);
    if ((p = realloc(message, len + 1 + strlen(key) + 1 + strlen(value) + 1)) == NULL) {
      return 0;
    }

    message = p;

    sprintf(message + len, ";%s=%s", key, value);
  }

  return 1;
}

static char *get_value(const char *key, mc_io_t io) {
#define RET_SIZE 1024
  int count;
  char ret[RET_SIZE];
  char c;
  char *p;

  printf("\x1b]%d;%s\x07", io, key);
  fflush(stdout);

  for (count = 0; count < RET_SIZE; count++) {
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (c != '\n') {
        ret[count] = c;
      } else {
        break;
      }
    } else {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " %s return from mlterm is illegal.\n", key);
#endif

      return NULL;
    }
  }

  if (count == RET_SIZE) return NULL;

  ret[count] = '\0';

  p = strchr(ret, '=');
  if (p == NULL || strcmp(ret, "#error") == 0) return NULL;

  /* #key=value */
  return strdup(p + 1);

#undef RET_SIZE
}

/* --- global functions --- */

void mc_exec_pty(const char *cmd) {
#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s\n", cmd);
#endif

  printf("\x1b]%d;%s\x07", mc_io_exec, cmd);
  fflush(stdout);
}

void mc_set_str_value_pty(const char *key, const char *value) {
  if (value == NULL) {
    return;
  }

  if (strcmp(key, "font_policy") == 0) {
    if (strcmp(value, "unicode") == 0) {
      mc_set_flag_value("only_use_unicode_font", 1);

      return;
    } else if (strcmp(value, "nounicode") == 0) {
      mc_set_flag_value("not_use_unicode_font", 1);

      return;
    } else {
      mc_set_flag_value("only_use_unicode_font", 0);
      mc_set_flag_value("not_use_unicode_font", 0);

      return;
    }
  } else if (strcmp(key, "logging_vt_seq") == 0) {
    if (strcmp(key, "no") == 0) {
      mc_set_flag_value("logging_vt_seq", 0);

      return;
    } else {
      mc_set_flag_value("logging_vt_seq", 1);
      mc_set_str_value("vt_seq_format", value);

      return;
    }
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s=%s\n", key, value);
#endif

  append_value(key, value);
}

void mc_set_flag_value_pty(const char *key, int flag_val) {
#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s=%s\n", key, flag_val ? "true" : "false");
#endif

  append_value(key, (flag_val ? "true" : "false"));
}

void mc_flush_pty(mc_io_t io) {
  char *chal;

  if (message == NULL) {
    return;
  }

  if (io == mc_io_set_save && (chal = get_value("challenge", mc_io_get))) {
    printf("\x1b]%d;%s;%s\x07", io, chal, message);
  } else {
    printf("\x1b]%d;%s\x07", io, message);
  }

  if (strstr(message, "depth=") && io == mc_io_set) {
    GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
                                               "\"True Transprent\" setting is discarded.\n"
                                               "Press \"Save&Exit\" button.");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }

  fflush(stdout);

#if 0
  fprintf(stderr, "%s\n", message);
#endif

  free(message);
  message = NULL;
}

char *mc_get_str_value_pty(const char *key) {
  char *value;

  if (strcmp(key, "font_policy") == 0) {
    if (mc_get_flag_value("only_use_unicode_font")) {
      return strdup("unicode");
    } else if (mc_get_flag_value("not_use_unicode_font")) {
      return strdup("nounicode");
    } else {
      return strdup("noconv");
    }
  } else if (strcmp(key, "logging_vt_seq") == 0) {
    if (mc_get_flag_value("logging_vt_seq")) {
      return mc_get_str_value("vt_seq_format");
    } else {
      return strdup("no");
    }
  }

  if ((value = get_value(key, mc_io_get)) == NULL) {
    return strdup("error");
  } else {
    return value;
  }
}

int mc_get_flag_value_pty(const char *key) {
  char *value;

  if ((value = get_value(key, mc_io_get)) == NULL) {
    return 0;
  }

  if (strcmp(value, "true") == 0) {
    free(value);

    return 1;
  } else {
    free(value);

    return 0;
  }
}

const char *mc_get_gui_pty(void) {
  if (!gui && !(gui = get_value("gui", mc_io_get))) {
    return "xlib";
  }

  return gui;
}

void mc_set_font_name_pty(mc_io_t io, const char *file, const char *cs, const char *font_name) {
  char *chal;

  if (io == mc_io_set_save_font && (chal = get_value("challenge", mc_io_get))) {
    printf("\x1b]%d;%s;%s:%s=%s\x07", io, chal, file, cs, font_name);
  } else {
    printf("\x1b]%d;%s:%s=%s\x07", io, file, cs, font_name);
  }

  fflush(stdout);
}

char *mc_get_font_name_pty(const char *file, const char *cs) {
  size_t len;
  char *value;
  char *key;

  len = strlen(cs) + 1;
  if (file) {
    len += (strlen(file) + 2);
  }

  if ((key = alloca(len))) {
    sprintf(key, "%s%s%s", file ? file : "", file ? ":" : "", cs);

    if ((value = get_value(key, mc_io_get_font))) {
      return value;
    }
  }

  return strdup("error");
}

void mc_set_color_rgb_pty(mc_io_t io, const char *color, const char *value) {
  char *chal;

  if (io == mc_io_set_save_font && (chal = get_value("challenge", mc_io_get))) {
    printf("\x1b]%d;%s;color:%s=%s\x07", io, chal, color, value);
  } else {
    printf("\x1b]%d;color:%s=%s\x07", io, color, value);
  }

  fflush(stdout);
}

char *mc_get_color_rgb_pty(const char *color) {
  char *key;
  char *value;

  if ((key = alloca(6 + strlen(color) + 1))) {
    sprintf(key, "color:%s", color);

    if ((value = get_value(key, mc_io_get_color))) {
      return value;
    }
  }

  return strdup("error");
}
