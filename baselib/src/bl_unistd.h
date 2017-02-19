/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_UNISTD_H__
#define __BL_UNISTD_H__

#include "bl_def.h"
#include "bl_types.h"

#ifdef HAVE_USLEEP

#include <unistd.h>

#define bl_usleep(microsec) usleep(microsec)

#else

#define bl_usleep(microsec) __bl_usleep(microsec)

int __bl_usleep(u_int microseconds);

#endif

#ifdef HAVE_SETENV

#include <stdlib.h>

#define bl_setenv(name, value, overwrite) setenv(name, value, overwrite)

#else /* HAVE_SETENV */

#ifdef USE_WIN32API

#define bl_setenv(name, value, overwrite) SetEnvironmentVariableA(name, value)

#else /* USE_WIN32API */

#define bl_setenv __bl_setenv

int __bl_setenv(const char *name, const char *value, int overwrite);

#endif /* USE_WIN32API */

#endif /* HAVE_SETENV */

#ifdef HAVE_UNSETENV

#include <stdlib.h>

#define bl_unsetenv(name) unsetenv(name)

#else /* HAVE_SETENV */

#ifdef USE_WIN32API

#define bl_unsetenv(name) SetEnvironmentVariableA(name, NULL)

#else /* USE_WIN32API */

#define bl_unsetenv(name) bl_setenv(name, "", 1);

#endif /* USE_WIN32API */

#endif /* HAVE_UNSETENV */

#ifdef HAVE_GETUID

#include <unistd.h>

#define bl_getuid getuid

#else

#define bl_getuid __bl_getuid

uid_t __bl_getuid(void);

#endif

#ifdef HAVE_GETGID

#include <unistd.h>

#define bl_getgid getgid

#else

#define bl_getgid __bl_getgid

gid_t __bl_getgid(void);

#endif

/* XXX vt_pty_unix.c which uses bl_killpg() has already included it. */
/* #include  <signal.h> */

#ifdef HAVE_KILLPG

#define bl_killpg(pid, sig) killpg(pid, sig)

#else

#define bl_killpg(pid, sig) kill(-pid, sig)

#endif

#endif
