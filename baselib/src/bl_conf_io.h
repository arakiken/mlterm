/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_CONF_IO_H__
#define __BL_CONF_IO_H__

#include "bl_file.h"

typedef struct bl_conf_write {
  char *path;
  char **lines;
  u_int scale;
  u_int num;

} bl_conf_write_t;

void bl_set_sys_conf_dir(const char *dir);

char *bl_get_sys_rc_path(const char *rcfile);

char *bl_get_user_rc_path(const char *rcfile);

bl_conf_write_t *bl_conf_write_open(const char *path);

int bl_conf_io_write(bl_conf_write_t *conf, const char *key, const char *val);

void bl_conf_write_close(bl_conf_write_t *conf);

int bl_conf_io_read(bl_file_t *from, char **key, char **val);

#endif
