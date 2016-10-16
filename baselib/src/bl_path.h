/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_PATH_H__
#define __BL_PATH_H__

#include <ctype.h> /* isalpha */

#include "bl_types.h"
#include "bl_def.h"

/* PathIsRelative() is not used to avoid link shlwapi.lib */
#define IS_RELATIVE_PATH_DOS(path) \
  (isalpha((path)[0]) ? ((path)[1] != ':' || (path)[2] != '\\') : ((path)[0] != '\\'))
#define IS_RELATIVE_PATH_UNIX(path) (*(path) != '/')

#ifdef USE_WIN32API
#define IS_RELATIVE_PATH(path) IS_RELATIVE_PATH_DOS(path)
#else
#define IS_RELATIVE_PATH(path) IS_RELATIVE_PATH_UNIX(path)
#endif

/* XXX win32 basename() works strangely if cp932 characters are pssed. */
#if defined(HAVE_BASENAME) && !defined(USE_WIN32API)

#include <libgen.h>

#define bl_basename(path) basename(path)

#else

#define bl_basename(path) __bl_basename(path)

char *__bl_basename(char *path);

#endif

#ifndef REMOVE_FUNCS_MLTERM_UNUSE

int bl_path_cleanname(char *cleaned_path, size_t size, const char *path);

#endif

int bl_parse_uri(char **proto, char **user, char **host, char **port, char **path, char **aux,
                 char *seq);

char *bl_get_home_dir(void);

#if defined(__CYGWIN__) || defined(__MSYS__)

int bl_conv_to_win32_path(const char *path, char *winpath, size_t len);

int bl_conv_to_posix_path(const char *winpath, char *path, size_t len);

#endif

#endif
