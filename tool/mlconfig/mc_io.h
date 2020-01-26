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

void mc_io_set_use_file(int flag);

int mc_io_is_file(void);

void mc_exec(const char *cmd);

void mc_set_str_value(const char *key, const char *value);

void mc_set_flag_value(const char *key, int flag_val);

void mc_flush(mc_io_t io);

char *mc_get_str_value(const char *key);

int mc_get_flag_value(const char *key);

const char *mc_get_gui(void);

void mc_set_font_name(mc_io_t io, const char *file, const char *cs, const char *font_name);

char *mc_get_font_name(const char *file, const char *cs);

void mc_set_color_rgb(mc_io_t io, const char *color, const char *value);

char *mc_get_color_rgb(const char *color);

#endif
