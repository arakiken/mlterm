/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_utmp.h"

#include <stdio.h> /* NULL */
#include <utempter.h>
#include "bl_mem.h" /* malloc/free */
#include "bl_str.h" /* strdup */

struct bl_utmp {
  char *tty;
  int fd;
};

/* --- global functions --- */

bl_utmp_t bl_utmp_new(const char *tty, const char *host, int pty_fd) {
  bl_utmp_t utmp;

  if ((utmp = malloc(sizeof(*utmp))) == NULL) {
    return NULL;
  }

  if ((utmp->tty = strdup(tty)) == NULL) {
    free(utmp);
    return NULL;
  }

  utmp->fd = pty_fd;

  addToUtmp(tty, host, pty_fd);

  return utmp;
}

int bl_utmp_destroy(bl_utmp_t utmp) {
  removeLineFromUtmp(utmp->tty, utmp->fd);
  free(utmp->tty);
  free(utmp);
  return 1;
}
