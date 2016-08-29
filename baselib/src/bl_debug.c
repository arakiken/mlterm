/*
 *	$Id$
 */

#include "bl_debug.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h> /* strlen */
#include <unistd.h> /* getpid */
#include <time.h>   /* time/ctime */
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "bl_mem.h"     /* alloca */
#include "bl_util.h"    /* DIGIT_STR_LEN */
#include "bl_conf_io.h" /* bl_get_user_rc_path */

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static char *log_file_path;

/* --- static functions --- */

static FILE *open_msg_file(void) {
  FILE *fp;

  if (log_file_path && (fp = fopen(log_file_path, "a+"))) {
    char ch;
    time_t tm;
    char *time_str;

    if (fseek(fp, -1, SEEK_END) == 0) {
      if (fread(&ch, 1, 1, fp) == 1 && ch != '\n') {
        fseek(fp, 0, SEEK_SET);

        return fp;
      }

      fseek(fp, 0, SEEK_SET);
    }

    tm = time(NULL);
    time_str = ctime(&tm);
    time_str[19] = '\0';
    time_str += 4;
    fprintf(fp, "%s[%d] ", time_str, getpid());

    return fp;
  }

  return stderr;
}

static void close_msg_file(FILE *fp) {
  if (fp != stderr) {
    fclose(fp);
  } else {
#ifdef USE_WIN32API
    fflush(fp);
#endif
  }
}

static int debug_printf(const char *prefix, const char *format, va_list arg_list) {
  size_t prefix_len;
  int ret;
  FILE *fp;

  if ((prefix_len = strlen(prefix)) > 0) {
    char *new_format;

    if ((new_format = alloca(prefix_len + strlen(format) + 1)) == NULL) {
      /* error */

      return 0;
    }

    sprintf(new_format, "%s%s", prefix, format);
    format = new_format;
  }

  fp = open_msg_file();
  ret = vfprintf(fp, format, arg_list);
  close_msg_file(fp);

  return ret;
}

/* --- global functions --- */

/*
 * this is usually used between #ifdef __DEBUG ... #endif
 */
int bl_debug_printf(const char *format, ...) {
  va_list arg_list;

  va_start(arg_list, format);

  return debug_printf("DEBUG: ", format, arg_list);
}

/*
 * this is usually used between #ifdef DEBUG ... #endif
 */
int bl_warn_printf(const char *format, ...) {
  va_list arg_list;

  va_start(arg_list, format);

  return debug_printf("WARN: ", format, arg_list);
}

/*
 * this is usually used without #ifdef ... #endif
 */
int bl_error_printf(const char *format, ...) {
  va_list arg_list;
  char *prefix;
  int ret;

  va_start(arg_list, format);

#ifdef HAVE_ERRNO_H
  if (errno != 0) {
    char *error;

    error = strerror(errno);

    if (!(prefix = alloca(6 + strlen(error) + 3 + 1))) {
      ret = 0;
      goto end;
    }

    sprintf(prefix, "ERROR(%s): ", error);
  } else
#endif
  {
    prefix = "ERROR: ";
  }

  ret = debug_printf(prefix, format, arg_list);

end:
  va_end(arg_list);

  return ret;
}

/*
 * for noticing message.
 */
int bl_msg_printf(const char *format, ...) {
  va_list arg_list;

  va_start(arg_list, format);

  return debug_printf("", format, arg_list);
}

int bl_set_msg_log_file_name(const char *name) {
  char *p;

  free(log_file_path);

  if (name && *name && (p = alloca(strlen(name) + DIGIT_STR_LEN(pid_t) + 5))) {
    log_file_path = bl_get_user_rc_path(name);
  } else {
    log_file_path = NULL;
  }

  return 1;
}
