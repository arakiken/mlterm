/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_file.h"

#include <fcntl.h>    /* fcntl() */
#include <sys/file.h> /* flock() */
#include <string.h>   /* memcpy */
#include <errno.h>
#include <sys/stat.h> /* stat */

#include "bl_mem.h" /* malloc */
#include "bl_debug.h"

#define BUF_UNIT_SIZE 512

/* --- global functions --- */

bl_file_t *bl_file_new(FILE* fp) {
  bl_file_t *file;

  if ((file = malloc(sizeof(bl_file_t))) == NULL) {
    return NULL;
  }

  file->file = fp;
  file->buffer = NULL;
#ifndef HAVE_FGETLN
  file->buf_size = 0;
#endif

  return file;
}

void bl_file_destroy(bl_file_t *file) {
  /* not fclose(file->fp) */

  free(file->buffer);
  free(file);
}

bl_file_t *bl_file_open(const char *file_path, const char *mode) {
  FILE* fp;

  if ((fp = fopen(file_path, mode)) == NULL) {
    return NULL;
  }

  return bl_file_new(fp);
}

void bl_file_close(bl_file_t *file) {
  fclose(file->file);
  bl_file_destroy(file);
}

FILE* bl_fopen_with_mkdir(const char *file_path, const char *mode) {
  FILE* fp;
  char *p;

  if ((fp = fopen(file_path, mode))) {
    return fp;
  }

  if ((p = alloca(strlen(file_path) + 1)) == NULL ||
      !bl_mkdir_for_file(strcpy(p, file_path), 0700)) {
    return NULL;
  }

  return fopen(file_path, mode);
}

#ifdef HAVE_FGETLN

char *bl_file_get_line(bl_file_t *from, size_t *len) {
  char *line;

  if ((line = fgetln(from->file, len)) == NULL) {
    return NULL;
  }

  if (line[*len - 1] == '\n') {
    if (*len > 1 && line[*len - 2] == '\r') {
      (*len) -= 2; /* \r\n -> \0\n */
    } else {
      (*len)--; /* \n -> \0 */
    }
  } else {
    void *p;

    /* If 'line' doesn't end with '\n', append '\0' to it. */
    if ((p = realloc(from->buffer, *len + 1)) == NULL) {
      return NULL;
    }

    line = memcpy((from->buffer = p), line, *len);
  }

  line[*len] = '\0';

  return line;
}

#else

/*
 * This behaves like fgetln().
 *
 * This returns the pointer to the beginning of line , and it becomes invalid
 * after the next bl_file_get_line() (whether successful or not) or as soon as
 * bl_file_close() is executed.
 */
char *bl_file_get_line(bl_file_t *from, size_t *len) {
  size_t filled;
  int c;

  filled = 0;

  if ((c = fgetc(from->file)) < 0) {
    return NULL;
  }

  while (1) {
    if (filled == from->buf_size) {
      void *p;

      if ((p = realloc(from->buffer, from->buf_size + BUF_UNIT_SIZE)) == NULL) {
        return NULL;
      }

      from->buffer = p;
      from->buf_size += BUF_UNIT_SIZE;
    }

    if (c == '\n') {
      if (filled > 0 && from->buffer[filled - 1] == '\r') {
        filled--;
      }
      break;
    } else {
      from->buffer[filled++] = c;
    }

    if ((c = fgetc(from->file)) < 0) {
      break;
    }
  }

  from->buffer[filled] = '\0';
  *len = filled;

  return from->buffer;
}

#endif

#if defined(HAVE_FLOCK) && defined(LOCK_EX) && defined(LOCK_UN)

int bl_file_lock(int fd) {
  if (flock(fd, LOCK_EX) == -1) {
    return 0;
  } else {
    return 1;
  }
}

int bl_file_unlock(int fd) {
  if (flock(fd, LOCK_UN) == -1) {
    return 0;
  } else {
    return 1;
  }
}

#else

int bl_file_lock(int fd) { return 0; }

int bl_file_unlock(int fd) { return 0; }

#endif

#ifdef F_GETFD

int bl_file_set_cloexec(int fd) {
  int old_flags;

  old_flags = fcntl(fd, F_GETFD);
  if (old_flags == -1) {
    return 0;
  }

  if (!(old_flags & FD_CLOEXEC) && (fcntl(fd, F_SETFD, old_flags | FD_CLOEXEC) == -1)) {
    return 0;
  }
  return 1;
}

int bl_file_unset_cloexec(int fd) {
  int old_flags;

  old_flags = fcntl(fd, F_GETFD);
  if (old_flags == -1) {
    return 0;
  }
  if ((old_flags & FD_CLOEXEC) && (fcntl(fd, F_SETFD, old_flags & (~FD_CLOEXEC)) == -1)) {
    return 0;
  }
  return 1;
}

#else /* F_GETFD */

int bl_file_set_cloexec(int fd) {
  /* do nothing */

  return 0;
}

int bl_file_unset_cloexec(int fd) {
  /* do nothing */

  return 0;
}

#endif

/*
 * /a/b/c  => mkdir /a ; mkdir /a/b
 * /a/b/c/ => mkdir /a ; mkdir /a/b ; mkdir /a/b/c
 * /a => do nothing
 */
int bl_mkdir_for_file(char *file_path, /* Not const. Don't specify read only data. */
                      mode_t dir_mode) {
  char *p;

  p = file_path + 1;
  while (*p) {
    if (*p == '/'
#ifdef USE_WIN32API
        || *p == '\\'
#endif
        ) {
      struct stat s;
      char c;

      c = *p; /* save */

      *p = '\0';
      if (stat(file_path, &s) != 0) {
        if (errno == ENOENT &&
#ifdef USE_WIN32API
            mkdir(file_path) != 0
#else
            mkdir(file_path, dir_mode) != 0
#endif
            ) {
          bl_msg_printf("Failed to mkdir %s\n", file_path);

          *p = c; /* restore */

          return 0;
        }
      }

      *p = c; /* restore */
    }

    p++;
  }

  return 1;
}
