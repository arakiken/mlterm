/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __MC_IO_H__
#define __MC_IO_H__

typedef enum {
  mc_io_exec = 5379,
  mc_io_set = 5379,
  mc_io_get = 5381,
  mc_io_set_save = 5383,
  mc_io_set_font = 5379,
  mc_io_get_font = 5381,
  mc_io_set_save_font = 5383,
  mc_io_set_color = 5379,
  mc_io_get_color = 5381,
  mc_io_set_save_color = 5383,
} mc_io_t;

int mc_exec(const char *cmd);

int mc_set_str_value(const char *key, const char *value);

int mc_set_flag_value(const char *key, int flag_val);

int mc_flush(mc_io_t io);

char *mc_get_str_value(const char *key);

int mc_get_flag_value(const char *key);

int mc_gui_is_win32(void);

int mc_set_font_name(mc_io_t io, const char *file, const char *cs, const char *font_name);

char *mc_get_font_name(const char *file, const char *cs);

int mc_set_color_name(mc_io_t io, const char *color, const char *value);

char *mc_get_color_name(const char *color);

#endif
