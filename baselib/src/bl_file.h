/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_FILE_H__
#define __BL_FILE_H__

#include <stdio.h>

#include "bl_types.h" /* size_t */
#include "bl_def.h" /* HAVE_FGETLN */

typedef struct bl_file {
  FILE* file;
  char *buffer;

#ifndef HAVE_FGETLN
  size_t buf_size;
#endif

} bl_file_t;

bl_file_t *bl_file_new(FILE* fp);

void bl_file_destroy(bl_file_t *file);

bl_file_t *bl_file_open(const char *file_path, const char *mode);

void bl_file_close(bl_file_t *file);

FILE* bl_fopen_with_mkdir(const char *file_path, const char *mode);

char *bl_file_get_line(bl_file_t *from, size_t *len);

int bl_file_lock(int fd);

int bl_file_unlock(int fd);

int bl_file_set_cloexec(int fd);

int bl_file_unset_cloexec(int fd);

int bl_mkdir_for_file(char *file_path, mode_t mode);

#endif
