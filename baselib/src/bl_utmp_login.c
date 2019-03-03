/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_utmp.h"

#include <stdio.h> /* NULL */
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h> /* getuid */
#include <string.h> /* strncmp */
#include <time.h>   /* time */
#if 1
/* *BSD has utmp.h anyway though login/logout aren't defined in it */
#include <utmp.h> /* login/logout(glibc2) you have to link libutil. */
#endif
#if 0
/* glibc(linux) doesn't have util.h */
#include <util.h> /* login/logout(*BSD) you have to link libutil. */
#endif

#include "bl_util.h" /* BL_MIN */
#include "bl_mem.h"  /* malloc/free */
#include "bl_def.h"  /* HAVE_SETUTENT */
#include "bl_privilege.h"

struct bl_utmp {
  char ut_line[UT_LINESIZE];
};

/* --- global functions --- */

bl_utmp_t bl_utmp_new(const char *tty, const char *host, int pty_fd) {
  bl_utmp_t utmp;
  struct utmp ut;
  struct passwd *pwent;
  char *pw_name;

  if ((utmp = malloc(sizeof(*utmp))) == NULL) {
    return NULL;
  }

/* unnecessary ? */
#if 0
#ifdef HAVE_SETUTENT
  setutent();
#endif
#endif

  memset(&ut, 0, sizeof(ut));

  if ((pwent = getpwuid(getuid())) == NULL || pwent->pw_name == NULL) {
    pw_name = "?";
  } else {
    pw_name = pwent->pw_name;
  }

  /*
   * user name field is named ut_name in *BSD and is ut_user in glibc2 ,
   * but glibc2 also defines ut_name as an alias of ut_user for backward
   * compatibility.
   */
  strncpy(ut.ut_name, pw_name, BL_MIN(sizeof(ut.ut_name) - 2, strlen(pw_name)));
  ut.ut_name[sizeof(ut.ut_name) - 1] = 0;

  if (strncmp(tty, "/dev/", 5) == 0) {
    /* skip /dev/ prefix */
    tty += 5;
  }

  if (strncmp(tty, "pts", 3) != 0 && strncmp(tty, "pty", 3) != 0 && strncmp(tty, "tty", 3) != 0) {
    free(utmp);

    return NULL;
  }

#ifndef HAVE_SETUTENT
  /* ut.ut_line must be filled before login() on bsd. */
  memcpy(ut.ut_line, tty, BL_MIN(sizeof(ut.ut_line), strlen(tty)));
#endif

  ut.ut_time = time(NULL);
  memcpy(ut.ut_host, host, BL_MIN(sizeof(ut.ut_host), strlen(host)));
  bl_priv_restore_euid(); /* useless? */
  bl_priv_restore_egid();

  /* login does not give us error information... */
  login(&ut);

  bl_priv_change_euid(getuid());
  bl_priv_change_egid(getgid());
  memcpy(utmp->ut_line, ut.ut_line, sizeof(utmp->ut_line));

  return utmp;
}

void bl_utmp_destroy(bl_utmp_t utmp) {
  bl_priv_restore_euid();
  bl_priv_restore_egid();

  logout(utmp->ut_line);
  logwtmp(utmp->ut_line, "", "");

  bl_priv_change_euid(getuid());
  bl_priv_change_egid(getgid());

/* unnecessary ? */
#if 0
#ifdef HAVE_SETUTENT
  endutent();
#endif
#endif

  free(utmp);
}
