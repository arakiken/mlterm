/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_io.h"

#include <pobl/bl_def.h>
#ifndef USE_WIN32API
#include <stdio.h>
#include <unistd.h> /* select */
#include <sys/time.h>
#endif
#include <pobl/bl_str.h> /* strdup */

#if 0
#define __DEBUG
#endif

void mc_exec_file(const char *cmd);
void mc_set_str_value_file(const char *key, const char *value);
void mc_set_flag_value_file(const char *key, int flag_val);
void mc_flush_file(mc_io_t io);
char *mc_get_str_value_file(const char *key);
int mc_get_flag_value_file(const char *key);
const char *mc_get_gui_file(void);
void mc_set_font_name_file(mc_io_t io, const char *file, const char *cs, const char *font_name);
char *mc_get_font_name_file(const char *file, const char *cs);
void mc_set_color_rgb_file(mc_io_t io, const char *color, const char *value);
char *mc_get_color_rgb_file(const char *color);

void mc_exec_pty(const char *cmd);
void mc_set_str_value_pty(const char *key, const char *value);
void mc_set_flag_value_pty(const char *key, int flag_val);
void mc_flush_pty(mc_io_t io);
char *mc_get_str_value_pty(const char *key);
int mc_get_flag_value_pty(const char *key);
const char *mc_get_gui_pty(void);
void mc_set_font_name_pty(mc_io_t io, const char *file, const char *cs, const char *font_name);
char *mc_get_font_name_pty(const char *file, const char *cs);
void mc_set_color_rgb_pty(mc_io_t io, const char *color, const char *value);
char *mc_get_color_rgb_pty(const char *color);

/* --- static variables --- */

static int is_file_io = 0;

/* --- global functions --- */

void mc_io_set_use_file(int flag) {
  is_file_io = flag;
}

int mc_io_is_file(void) {
#ifdef USE_WIN32API
  return is_file_io;
#else
  if (!is_file_io) {
    struct timeval tval;
    fd_set read_fds;

    printf("\x1b]5381;gui\x07");
    fflush(stdout);
    tval.tv_usec = 750000; /* 750 msec */
    tval.tv_sec = 0;
    FD_SET(STDIN_FILENO, &read_fds);
    if (select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &tval) == 1) {
      char c;

      do {
        read(STDIN_FILENO, &c, 1);
      } while (c != '\n');
    } else {
      is_file_io = 1;
    }
  }

  return is_file_io;
#endif
}

void mc_exec(const char *cmd) {
  if (mc_io_is_file()) {
    mc_exec_file(cmd);
  } else {
    mc_exec_pty(cmd);
  }
}

void mc_set_str_value(const char *key, const char *value) {
  if (mc_io_is_file()) {
    mc_set_str_value_file(key, value);
  } else {
    mc_set_str_value_pty(key, value);
  }
}

void mc_set_flag_value(const char *key, int flag_val) {
  if (mc_io_is_file()) {
    mc_set_flag_value_file(key, flag_val);
  } else {
    mc_set_flag_value_pty(key, flag_val);
  }
}

void mc_flush(mc_io_t io) {
  if (mc_io_is_file()) {
    mc_flush_file(io);
  } else {
    mc_flush_pty(io);
  }
}

char *mc_get_str_value(const char *key) {
  if (mc_io_is_file()) {
    return mc_get_str_value_file(key);
  } else {
    return mc_get_str_value_pty(key);
  }
}

int mc_get_flag_value(const char *key) {
  if (mc_io_is_file()) {
    return mc_get_flag_value_file(key);
  } else {
    return mc_get_flag_value_pty(key);
  }
}

const char *mc_get_gui(void) {
  if (mc_io_is_file()) {
    return mc_get_gui_file();
  } else {
    return mc_get_gui_pty();
  }
}

void mc_set_font_name(mc_io_t io, const char *file, const char *cs, const char *font_name) {
  if (mc_io_is_file()) {
    mc_set_font_name_file(io, file, cs, font_name);
  } else {
    mc_set_font_name_pty(io, file, cs, font_name);
  }
}

char *mc_get_font_name(const char *file, const char *cs) {
  if (mc_io_is_file()) {
    return mc_get_font_name_file(file, cs);
  } else {
    return mc_get_font_name_pty(file, cs);
  }
}

void mc_set_color_rgb(mc_io_t io, const char *color, const char *value) {
  if (mc_io_is_file()) {
    mc_set_color_rgb_file(io, color, value);
  } else {
    mc_set_color_rgb_pty(io, color, value);
  }
}

char *mc_get_color_rgb(const char *color) {
  if (strcmp(color, "lightgray") == 0) {
    return strdup("#d3d3d3");
  } else if (strcmp(color, "gray") == 0) {
    return strdup("#bebebe");
  }

  if (mc_io_is_file()) {
    return mc_get_color_rgb_file(color);
  } else {
    return mc_get_color_rgb_pty(color);
  }
}
