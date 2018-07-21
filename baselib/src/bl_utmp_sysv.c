/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/* Sample implementation - who/w  works;
                           finger doesn't (WHY ??)
 */

#include "bl_utmp.h"

#include <stdio.h> /* NULL */
#include <pwd.h>
#include <string.h> /* strncmp */
#include <time.h>   /* time */

#ifdef USE_UTMPX
#include <utmpx.h> /* getut*, setut*, etc */
#else
#include <utmp.h> /* getut*, setut*, etc */
#endif

#include <sys/time.h> /* timeval */
#include <unistd.h>   /* getuid */

#include "bl_util.h" /* BL_MIN */
#include "bl_mem.h"  /* malloc/free */
#include "bl_privilege.h"
#include "bl_debug.h"

#ifdef _PATH_UTMP_UPDATE
#include <errno.h>
#endif

#ifdef USE_UTMPX
#define LINE_WIDTH 32
#else
#define LINE_WIDTH 12
#endif

struct bl_utmp {
  char ut_line[LINE_WIDTH];
  char ut_pos[4];
};

/* --- static functions --- */

static const char *get_pw_name(void) {
  struct passwd *pwent;

  if (((pwent = getpwuid(getuid())) == NULL) || (pwent->pw_name == NULL)) {
    return "?";
  } else {
    return pwent->pw_name;
  }
}

/* --- global functions --- */

bl_utmp_t bl_utmp_new(const char *tty, const char *host, int pty_fd) {
#ifdef USE_UTMPX
  struct utmpx ut;
#else
  struct utmp ut;
#endif
  bl_utmp_t utmp;
  const char *pw_name;
  char *tty_num;
  struct timeval timenow;

  gettimeofday(&timenow, NULL);

  if ((utmp = calloc(1, sizeof(*utmp))) == NULL) {
    return NULL;
  }

  memset(&ut, 0, sizeof(ut));

  if ((strncmp(tty, "/dev/", 5)) == 0) {
    /* skip /dev/ prefix */
    tty += 5;
  }

  if ((strncmp(tty, "pts", 3) != 0) && (strncmp(tty, "pty", 3) != 0) &&
      (strncmp(tty, "tty", 3) != 0)) {
    free(utmp);

    return NULL;
  }

  if ((tty_num = strchr(tty, '/'))) {
    tty_num++;
  } else if ((strncmp(tty + 1, "typ", 3)) == 0) {
    /* /dev/ttypN or /dev/ptypN */
    tty_num = tty + 4;
  } else {
    free(utmp);

    return NULL;
  }

  pw_name = get_pw_name();
  strncpy(ut.ut_user, pw_name, BL_MIN(sizeof(ut.ut_user), strlen(pw_name)));
  memcpy(ut.ut_id, tty_num, BL_MIN(sizeof(ut.ut_id), strlen(tty_num)));
  memcpy(ut.ut_line, tty, BL_MIN(sizeof(ut.ut_line), strlen(tty)));

  ut.ut_pid = getpid();
  ut.ut_type = USER_PROCESS;

#ifdef USE_UTMPX
  ut.ut_tv.tv_sec = timenow.tv_sec;
  ut.ut_tv.tv_usec = timenow.tv_usec;
#else
  ut.ut_time = time(NULL);
#endif

#ifdef USE_UTMPX
  memcpy(ut.ut_host, host, BL_MIN(sizeof(ut.ut_host), strlen(host)));
#if !defined(__FreeBSD__) && !defined(__APPLE__) && !defined(__CYGWIN__) && !defined(__MSYS__)
  ut.ut_session = getsid(0);
#endif
#endif

  memcpy(utmp->ut_line, tty, BL_MIN(sizeof(utmp->ut_line), strlen(tty)));
  memcpy(utmp->ut_pos, tty_num, BL_MIN(sizeof(utmp->ut_pos), strlen(tty_num)));

  bl_priv_restore_euid(); /* useless? */
  bl_priv_restore_egid();

/* reset the input stream to the beginning of the file */
#ifdef USE_UTMPX
  setutxent();
#else
  setutent();
#endif

/* insert new entry */
#ifdef USE_UTMPX
  if (!pututxline(&ut)) {
#ifdef _PATH_UTMP_UPDATE
    /* NetBSD calls waitpid() for utmp_update which is executed as root(SUID)
     * and fails(ECHILD). */
    if (errno != ECHILD)
#endif
    {
      free(utmp);
      utmp = NULL;
    }
  }
#else
  /* pututline doesn't return value in XPG2 and SVID2 */
  pututline(&ut);
#endif

#ifdef DEBUG
  if (!utmp) {
    bl_debug_printf(BL_DEBUG_TAG "pututline failed. EUID %d EGID %d => ", geteuid(), getegid());
    perror(NULL);
  }
#endif

#ifdef USE_UTMPX
  endutxent();
#else
  endutent();
#endif

  bl_priv_change_euid(getuid());
  bl_priv_change_egid(getgid());

  return utmp;
}

int bl_utmp_delete(bl_utmp_t utmp) {
#ifdef USE_UTMPX
  struct utmpx ut;
#else
  struct utmp ut;
#endif
  const char *pw_name;

  memset(&ut, 0, sizeof(ut));

  pw_name = get_pw_name();
  strncpy(ut.ut_user, pw_name, BL_MIN(sizeof(ut.ut_user), strlen(pw_name)));
  memcpy(ut.ut_id, utmp->ut_pos, BL_MIN(sizeof(ut.ut_id), sizeof(utmp->ut_pos)));
  memcpy(ut.ut_line, utmp->ut_line, BL_MIN(sizeof(ut.ut_line), sizeof(utmp->ut_line)));

  ut.ut_pid = getpid();
  ut.ut_type = DEAD_PROCESS;

#ifdef USE_UTMPX
  ut.ut_tv.tv_sec = 0;
  ut.ut_tv.tv_usec = 0;
#else
  ut.ut_time = 0;
#endif

  bl_priv_restore_euid();
  bl_priv_restore_egid();

/* reset the input stream to the beginning of the file */
#ifdef USE_UTMPX
  setutxent();
  pututxline(&ut);
  endutxent();
#else
  setutent();
  pututline(&ut);
  endutent();
#endif

  bl_priv_change_euid(getuid());
  bl_priv_change_egid(getgid());

  free(utmp);

  return 1;
}
