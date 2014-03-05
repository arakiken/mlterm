/*
 *	$Id$
 */

/*
 * Don't include directly this header.
 * Include kik_def.h (which wraps POSIX and kik_config.h macros) or kik_types.h
 * (which wraps POSIX and kik_config.h types).
 */

#ifndef  __KIK_CONFIG_H__
#define  __KIK_CONFIG_H__

#define  HAVE_GNU_SOURCE

#undef  HAVE_LANGINFO_H

#define  HAVE_DL_H

#undef  HAVE_DLFCN_H

#undef  HAVE_WINDOWS_H

#define  HAVE_ERRNO_H

#undef  WORDS_BIGENDIAN

#define  HAVE_STRSEP

#define  HAVE_FGETLN

#define  HAVE_BASENAME

#define  HAVE_ALLOCA

#define  HAVE_ALLOCA_H

#undef  HAVE_STROPTS_H

#undef  HAVE_SYS_STROPTS_H

#undef  HAVE_ISASTREAM

#define  HAVE_SETUTENT

#define  HAVE_SETEUID

#define  HAVE_SETEGID

#define  HAVE_GETEUID

#define  HAVE_SETSID

#define  HAVE_GETUID

#define  HAVE_GETGID

#define  HAVE_RECVMSG

#define  HAVE_SETPGID

#define  HAVE_SOCKETPAIR

#define  HAVE_SNPRINTF

#undef  CONCATABLE_FUNCTION

#undef  DLFCN_NONE

#define  HAVE_USLEEP

#define  HAVE_SETENV

#define  HAVE_UNSETENV

#define  HAVE_FLOCK

#undef  HAVE_POSIX_OPENPT

#undef  USE_WIN32API

#define  HAVE_STDINT_H

#define  REMOVE_FUNCS_MLTERM_UNUSE

#define  CALLOC_CHECK_OVERFLOW

#undef  inline

#undef  const

#undef  u_char

#undef  u_short

#undef  u_int

#undef  u_long

#undef  u_int8_t

#undef  u_int8_t

#undef  u_int16_t

#undef  u_int32_t

#undef  u_int64_t

#undef  int8_t

#undef  int8_t

#undef  int16_t

#undef  int32_t

#undef  int64_t

#undef  ssize_t

#undef  socklen_t

#undef  mode_t

#undef  pid_t

#undef  uid_t

#undef  gid_t

#undef  off_t

#undef  size_t

#endif
