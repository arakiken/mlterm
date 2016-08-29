/*
 *	$Id$
 */

#include "bl_time.h"

#ifndef REMOVE_FUNCS_MLTERM_UNUSE

#include <string.h> /* strncmp()/memset() */
#include <stdio.h>
#include <ctype.h> /* isdigit() */

#include "bl_debug.h"
#include "bl_mem.h" /* alloca() */

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static char* abbrev_wdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static char* wdays[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                        "Thursday", "Friday", "Saturday"};

static char* abbrev_months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static char* months[] = {"January", "Febrary", "March",     "April",   "May",      "June",
                         "July",    "August",  "September", "October", "November", "December"};

/* --- static functions --- */

/*
 * XXX
 * this should be placed in bl_str.
 */
static int strntoi(const char* str, size_t size) {
  int i = 0;
  char* format = NULL;

  /*
   * size should be 4 digits , or 0 - 999
   */
  if (size < 0 || 999 < size) {
    return 0;
  }

  if ((format = alloca(4 + 2)) == NULL) {
    /* the same spec as atoi() */

    return 0;
  }
  sprintf(format, "%%%dd", size);

  sscanf(str, format, &i);

  return i;
}

/* --- global functions --- */

/*
 * this function converts the string date represented by the following format to
 *time_t.
 * the format is described by the characters below.
 *
 * supported format.
 * %Y  year  1900-
 * %m  month 0-11
 * %d  mday  1-31
 * %H  hour  0-23
 * %S  second 0-61
 *
 * If you want to do the opposition of this function , use strftime().
 *
 * XXX
 * these format characters are conformed to str{p|f}time() format charcters and
 *are
 * extended in some points e.g.) the num of length can be inserted between '%'
 * and a format character.
 */
time_t bl_time_string_date_to_time_t(const char* format, const char* date) {
  struct tm tm_info;
  char* date_dup = NULL;
  char* date_p = NULL;
  const char* format_p = NULL;

  if ((date_dup = alloca(strlen(date) + 1)) == NULL) {
    return -1;
  }
  strcpy(date_dup, date);

  format_p = format;
  date_p = date_dup;

  /*
   * we set default value.
   */
  memset(&tm_info, 0, sizeof(struct tm));
  tm_info.tm_mday = 1;   /* [1-31] */
  tm_info.tm_wday = 0;   /* ignored by mktime() */
  tm_info.tm_yday = 0;   /* ignored by mktime() */
  tm_info.tm_isdst = -1; /* we dont presume summer time */
#if 0
  tm_info.tm_gmtoff = 0; /* offset from UTC in seconds */
  tm_info.tm_zone = "UTC";
#endif

  while (*format_p && *date_p) {
    if (*format_p == '%') {
      int size = 0;

      format_p++;
      if (!*format_p) {
        /* strange format. */

        return -1;
      } else if (*format_p == '%') {
        if (*date_p != '%') {
          /* strange format. */

          return -1;
        }

        format_p++;
        date_p++;

        continue;
      }

      if (isdigit(*format_p)) {
        size = strntoi(format_p, 1);

        format_p++;
      } else {
        size = 1;
      }

      if (*format_p == 'Y') {
        if (size != 4) {
          /* we don't support before 1900. */

          return -1;
        }
        tm_info.tm_year = strntoi(date_p, size) - 1900;

        date_p += size;
      } else if (*format_p == 'm') {
        if (size != 1 && size != 2) {
          return -1;
        }

        tm_info.tm_mon = strntoi(date_p, size) - 1;

        date_p += size;

#ifdef __DEBUG
        bl_debug_printf("mon %d\n", tm_info.tm_mon);
#endif
      } else if (*format_p == 'd') {
        if (size != 1 && size != 2) {
          return -1;
        }

        tm_info.tm_mday = strntoi(date_p, size);

        date_p += size;

#ifdef __DEBUG
        bl_debug_printf("day %d\n", tm_info.tm_mday);
#endif
      } else if (*format_p == 'H') {
        if (size != 1 && size != 2) {
          return -1;
        }

        tm_info.tm_hour = strntoi(date_p, size);

        date_p += size;

#ifdef __DEBUG
        bl_debug_printf("hour %d\n", tm_info.tm_hour);
#endif
      } else if (*format_p == 'M') {
        if (size != 1 && size != 2) {
          return -1;
        }

        tm_info.tm_min = strntoi(date_p, size);

        date_p += size;

#ifdef __DEBUG
        bl_debug_printf("min %d\n", tm_info.tm_min);
#endif
      } else if (*format_p == 'S') {
        if (size != 1 && size != 2) {
          return -1;
        }

        tm_info.tm_sec = strntoi(date_p, size);

        date_p += size;

#ifdef __DEBUG
        bl_debug_printf("sec %d\n", tm_info.tm_sec);
#endif
      } else {
        return -1;
      }

      format_p++;
    } else {
      date_p++;
      format_p++;
    }
  }

  if (*date_p != '\0' || *format_p != '\0') {
    return -1;
  } else {
    /* if fails , mktime returns -1. */

    return mktime(&tm_info);
  }
}

struct tm* bl_time_string_date_to_tm(struct tm* tm_info, const char* format, const char* date) {
  time_t time = 0;

  if ((time = bl_time_string_date_to_time_t(format, date)) == -1) {
    return NULL;
  }

  return memcpy(tm_info, localtime(&time), sizeof(struct tm));
}

/*
 * "Sun","Sunday" -> 0.
 *
 * XXX
 * we don't intend to support TIME locale , which means locale wdays for example
 *"ÆüÍË"
 * will not be parsed.
 */
int bl_time_string_wday_to_int(const char* wday) {
  int count = 0;

  for (count = 0; count < 7; count++) {
    if (strcmp(wday, wdays[count]) == 0 || strcmp(wday, abbrev_wdays[count]) == 0) {
      return count;
    }
  }

  return -1;
}

/*
 * 0 -> "Sun"
 */
char* bl_time_int_wday_to_abbrev_string(int wday) {
  if (0 <= wday && wday < 6) {
    return abbrev_wdays[wday];
  } else {
    return NULL;
  }
}

/*
 * 0 -> "Sunday"
 */
char* bl_time_int_wday_to_string(int wday) {
  if (0 <= wday && wday < 6) {
    return wdays[wday];
  } else {
    return NULL;
  }
}

/*
 * "Jan","January" -> 0
 */
int bl_time_string_month_to_int(const char* month) {
  int count = 0;

  for (count = 0; count < 12; count++) {
    if (strcmp(month, months[count]) == 0 || strcmp(month, abbrev_months[count]) == 0) {
      return count;
    }
  }

  return -1;
}

/*
 * 0 -> "January"
 */
char* bl_time_int_month_to_string(int month) {
  if (0 <= month && month < 6) {
    return months[month];
  } else {
    return NULL;
  }
}

/*
 * 0 -> "Jan"
 */
char* bl_time_int_month_to_abbrev_string(int month) {
  if (0 <= month && month < 6) {
    return abbrev_months[month];
  } else {
    return NULL;
  }
}

#endif /* REMOVE_FUNCS_MLTERM_UNUSE */
