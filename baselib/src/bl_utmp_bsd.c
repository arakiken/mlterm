/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_utmp.h"

#include <stdio.h>  /* sscanf */
#include <string.h> /* strncpy */
#include <fcntl.h>  /* open */
#include <sys/types.h>
#include <unistd.h>   /* write/close/getuid/getgid */
#include <pwd.h>      /* getpwuid */
#include <sys/stat.h> /* stat */
#include <errno.h>
#include <utmp.h>

#include "bl_types.h" /* off_t */
#include "bl_util.h"  /* BL_MIN */
#include "bl_mem.h"   /* malloc/free */
#include "bl_privilege.h"

/*
 * These four macros are directly used in this file.
 * They appeared at least in 4.4BSD-Lite according to NetBSD cvs repository.
 */
#ifndef UT_LINESIZE
#error "UT_LINESIZE is not defined."
#endif

#ifndef _PATH_UTMP
#error "_PATH_UTMP is not defined."
#endif

#ifndef _PATH_WTMP
#error "_PATH_WTMP is not defined."
#endif

#ifndef _PATH_LASTLOG
#error "_PATH_LASTLOG is not defined."
#endif

struct bl_utmp {
  char ut_line[UT_LINESIZE];
  int ut_pos;
};

/* --- static functions --- */

static int write_utmp(struct utmp *ut, int pos) {
  int fd;

  if ((fd = open(_PATH_UTMP, O_WRONLY)) == -1 ||
      lseek(fd, (off_t)(pos * sizeof(*ut)), SEEK_SET) == -1) {
    return 0;
  }

  write(fd, ut, sizeof(*ut));
  close(fd);

  return 1;
}

static int write_wtmp(struct utmp *ut, int pos) {
  int fd;
  int locked;
  int retry;
  struct flock lck; /* fcntl locking scheme */
  struct stat st;
  int result;

  if ((fd = open(_PATH_WTMP, O_WRONLY | O_APPEND, 0)) < 0) {
    return 0;
  }

  lck.l_whence = SEEK_END; /* lock from current eof */
  lck.l_len = 0;           /* end at "largest possible eof" */
  lck.l_start = 0;
  lck.l_type = F_WRLCK; /* write lock */

  locked = 0;
  for (retry = 0; retry < 10; retry++) {
    /*
     * lock with F_SETLK.
     * F_SETLKW would cause a deadlock.
     */
    if (fcntl(fd, F_SETLK, &lck) != -1) {
      locked = 1;
      break;
    } else if (errno != EAGAIN && errno != EACCES) {
      break;
    }
  }

  if (!locked) {
    close(fd);

    return 0;
  }

  result = 0;

  if (fstat(fd, &st) == 0) {
    if (write(fd, ut, sizeof(*ut)) != sizeof(*ut)) {
      /* removing bad writes */
      ftruncate(fd, st.st_size);
    } else {
      result = 1;
    }
  }

  lck.l_type = F_UNLCK;
  fcntl(fd, F_SETLK, &lck);

  close(fd);

  return result;
}

static int write_lastlog(struct lastlog *ll, uid_t uid) {
  int fd;
  int result;

  result = 0;

  if ((fd = open(_PATH_LASTLOG, O_RDWR)) != -1) {
    if (lseek(fd, (off_t)(uid * sizeof(*ll)), SEEK_SET) != -1) {
      write(fd, ll, sizeof(*ll));

      result = 1;
    }

    close(fd);
  }

  return result;
}

/* --- global functions --- */

bl_utmp_t bl_utmp_new(const char *tty, const char *host, int pty_fd /* not used */
                      ) {
  bl_utmp_t utmp;
  struct utmp ut;
  struct lastlog ll;
  FILE *fp;
  struct passwd *pwent;
  char *pw_name;
  char buf[256];
  char buf2[256];

  if ((utmp = malloc(sizeof(*utmp))) == NULL) {
    return NULL;
  }

  memset(&ut, 0, sizeof(ut));
  memset(&ll, 0, sizeof(ll));

  ll.ll_time = ut.ut_time = time(NULL);

  memcpy(ut.ut_host, host, BL_MIN(sizeof(ut.ut_host), strlen(host)));
  memcpy(ll.ll_host, host, BL_MIN(sizeof(ll.ll_host), strlen(host)));

  if ((pwent = getpwuid(getuid())) == NULL || pwent->pw_name == NULL) {
    pw_name = "?";
  } else {
    pw_name = pwent->pw_name;
  }

  strncpy(ut.ut_name, pw_name, BL_MIN(sizeof(ut.ut_name), strlen(pw_name)));

  if (strncmp(tty, "/dev/", 5) == 0) {
    /* skip /dev/ prefix */
    tty += 5;
  }

  if (strncmp(tty, "pty", 3) != 0 && strncmp(tty, "tty", 3) != 0) {
    goto error;
  }

  memcpy(ut.ut_line, tty, BL_MIN(sizeof(ut.ut_line), strlen(tty)));
  memcpy(ll.ll_line, tty, BL_MIN(sizeof(ll.ll_line), strlen(tty)));

  memcpy(utmp->ut_line, ut.ut_line, sizeof(ut.ut_line));

  if ((fp = fopen("/etc/ttys", "r")) == NULL && (fp = fopen("/etc/ttytab", "r")) == NULL) {
    goto error;
  }

  utmp->ut_pos = 1;
  while (1) {
    if (!fgets(buf, sizeof(buf), fp)) {
      goto error;
    }

    if (*buf != '#' && sscanf(buf, "%s", buf2) == 1) {
      if (strcmp(tty, buf2) == 0) {
        break;
      }
    }

    utmp->ut_pos++;
  }

  fclose(fp);

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  if (!write_utmp(&ut, utmp->ut_pos)) {
    goto error;
  }

  write_wtmp(&ut, utmp->ut_pos);

  write_lastlog(&ll, pwent->pw_uid);

  bl_priv_change_euid(getuid());
  bl_priv_change_egid(getgid());

  return utmp;

error:
  bl_priv_change_euid(getuid());
  bl_priv_change_egid(getgid());

  free(utmp);

  return NULL;
}

void bl_utmp_destroy(bl_utmp_t utmp) {
  struct utmp ut;

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  memset(&ut, 0, sizeof(ut));

  write_utmp(&ut, utmp->ut_pos);

  ut.ut_time = time(NULL);
  memcpy(ut.ut_line, utmp->ut_line, sizeof(utmp->ut_line));
  memset(ut.ut_name, 0, sizeof(ut.ut_name));
  memset(ut.ut_host, 0, sizeof(ut.ut_host));

  write_wtmp(&ut, utmp->ut_pos);

  bl_priv_change_euid(getuid());
  bl_priv_change_egid(getgid());

  free(utmp);
}
