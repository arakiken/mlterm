/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_locale.h"

#include <stdio.h>  /* sprintf */
#include <locale.h> /* setlocale() */

/* for bl_get_codeset_win32() */
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include "bl_langinfo.h" /* bl_langinfo() */
#include "bl_debug.h"
#include "bl_mem.h" /* alloca */
#include "bl_str.h"
#include "bl_util.h" /* BL_MIN */

#if 0
#define __DEBUG
#endif

typedef struct lang_codeset_table {
  char *lang;
  char *codeset;

} lang_codeset_table_t;

typedef struct alias_codeset_table {
  char *codeset;
  char *locale;

  char *alias;

} alias_codeset_table_t;

/* --- static variables --- */

static char *sys_locale = NULL;
static char *sys_lang = NULL;
static char *sys_country = NULL;
static char *sys_codeset = NULL;

/* for sys_lang and sys_country memory */
static char *sys_lang_country = NULL;

#ifndef USE_WIN32API
static lang_codeset_table_t lang_codeset_table[] = {
    {
     "en", "ISO8859-1",
    },
    {
     "da", "ISO8859-1",
    },
    {
     "de", "ISO8859-1",
    },
    {
     "fi", "ISO8859-1",
    },
    {
     "fr", "ISO8859-1",
    },
    {
     "is", "ISO8859-1",
    },
    {
     "it", "ISO8859-1",
    },
    {
     "nl", "ISO8859-1",
    },
    {
     "no", "ISO8859-1",
    },
    {
     "pt", "ISO8859-1",
    },
    {
     "sv", "ISO8859-1",
    },
    {
     "cs", "ISO8859-2",
    },
    {
     "hr", "ISO8859-2",
    },
    {
     "hu", "ISO8859-2",
    },
    {
     "la", "ISO8859-2",
    },
    {
     "lt", "ISO8859-2",
    },
    {
     "pl", "ISO8859-2",
    },
    {
     "sl", "ISO8859-2",
    },
    {
     "el", "ISO8859-7",
    },
    {
     "ru", "KOI8-R",
    },
    {
     "uk", "KOI8-U",
    },
    {
     "vi", "VISCII",
    },
    {
     "th", "TIS-620",
    },
    {
     "ja", "eucJP",
    },
    {
     "ko", "eucKR",
    },
    {
     "zh_CN", "eucCN",
    },
    {
     "zh_TW", "Big5",
    },
    {
     "zh_HK", "Big5HKSCS",
    },

};
#endif

static alias_codeset_table_t alias_codeset_table[] = {
    {
     "EUC", "ja_JP.EUC", "eucJP",
    },
    {
     "EUC", "ko_KR.EUC", "eucKR",
    },
};

/* --- global functions --- */

int bl_locale_init(const char *locale) {
  char *locale_p;
  int result;

  if (sys_locale && locale && strcmp(locale, sys_locale) == 0) {
    return 1;
  }

  if ((locale = setlocale(LC_CTYPE, locale)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " setlocale() failed.\n");
#endif

    if (sys_locale) {
      /* restoring locale info. nothing is changed. */
      setlocale(LC_CTYPE, sys_locale);

      return 0;
    } else if ((locale = getenv("LC_ALL")) == NULL && (locale = getenv("LC_CTYPE")) == NULL &&
               (locale = getenv("LANG")) == NULL) {
      /* nothing is changed */

      return 0;
    }

    result = 0;
  }

  if (sys_locale) {
    free(sys_locale);
    sys_locale = NULL;
  }

  if (sys_lang_country) {
    free(sys_lang_country);
    sys_lang_country = NULL;
  }

  /*
   * If external library calls setlocale(), this 'locale' variable
   * can be free'ed, so strdup() shoule be called.
   */
  sys_locale = strdup(locale);
  result = 1;

  if ((locale_p = sys_lang_country = strdup(locale)) == NULL) {
    sys_locale = NULL;

    return 0;
  }

  if ((sys_lang = bl_str_sep(&locale_p, "_")) == NULL) {
    /* this never happends */

    return 0;
  }

  sys_country = bl_str_sep(&locale_p, ".");

  sys_codeset = bl_langinfo(CODESET);

  if (strcmp(sys_codeset, "") == 0) {
    if (locale_p && *locale_p) {
      sys_codeset = locale_p;
    } else {
      sys_codeset = NULL;
    }
  }

  if (sys_codeset) {
    /*
     * normalizing codeset name.
     */

    int count;

    for (count = 0; count < sizeof(alias_codeset_table) / sizeof(alias_codeset_table[0]); count++) {
      if (strcmp(sys_codeset, alias_codeset_table[count].codeset) == 0 &&
          strcmp(locale, alias_codeset_table[count].locale) == 0) {
        sys_codeset = alias_codeset_table[count].alias;

        break;
      }
    }
  }

#ifdef __DEBUG
  bl_debug_printf("locale setttings -> locale %s lang %s country %s codeset %s\n", sys_locale,
                  sys_lang, sys_country, sys_codeset);
#endif

  return result;
}

int bl_locale_final(void) {
  if (sys_locale) {
    free(sys_locale);
    sys_locale = NULL;
  }

  if (sys_lang_country) {
    free(sys_lang_country);
    sys_lang_country = NULL;
  }

  return 1;
}

char *bl_get_locale(void) {
  if (sys_locale) {
    return sys_locale;
  } else {
    return "C";
  }
}

char *bl_get_lang(void) {
  if (sys_lang) {
    return sys_lang;
  } else {
    return "en";
  }
}

char *bl_get_country(void) {
  if (sys_country) {
    return sys_country;
  } else {
    return "US";
  }
}

#ifndef USE_WIN32API

char *bl_get_codeset(void) {
  if (sys_codeset) {
    return sys_codeset;
  } else if (sys_lang) {
    int count;
    char *lang;
    u_int lang_len;

    lang_len = strlen(sys_lang) + 1;
    if (sys_country) {
      /* "+ 1" is for '_' */
      lang_len += strlen(sys_country) + 1;
    }

    if ((lang = alloca(lang_len)) == NULL) {
      return "ISO8859-1";
    }

    if (sys_country) {
      sprintf(lang, "%s_%s", sys_lang, sys_country);
    } else {
      sprintf(lang, "%s", sys_lang);
    }

#ifdef __DEBUG
    bl_debug_printf("lang -> %s\n", lang);
#endif

    for (count = 0; count < sizeof(lang_codeset_table) / sizeof(lang_codeset_table[0]); count++) {
      if (strncmp(lang, lang_codeset_table[count].lang,
                  /* lang_len *- 1* is excluing NULL */
                  BL_MIN(lang_len - 1, strlen(lang_codeset_table[count].lang))) == 0) {
        return lang_codeset_table[count].codeset;
      }
    }
  }

  return "ISO8859-1";
}

#endif /* USE_WIN32API */

#ifdef HAVE_WINDOWS_H

typedef struct cp_cs_table {
  int codepage;
  char *codeset;

} cp_cs_table_t;

static cp_cs_table_t cp_cs_table[] = {
    {
     1250, "CP1250",
    }, /* EastEurope_CHARSET */
    {
     1251, "CP1251",
    }, /* RUSSIAN_CHARSET */
    {
     1252, "CP1252",
    }, /* ANSI_CHARSET */
    {
     1253, "CP1253",
    }, /* GREEK_CHARSET */
    {
     1254, "CP1254",
    }, /* TURKISH_CHARSET */
    {
     1255, "CP1255",
    }, /* HEBREW_CHARSET */
    {
     1256, "CP1256",
    }, /* ARABIC_CHARSET */
    {
     1257, "CP1257",
    }, /* BALTIC_CHARSET */
    {
     1258, "CP1258",
    }, /* VIETNAMESE_CHARSET */
    {
     874, "ISO8859-11",
    }, /* THAI_CHARSET XXX CP874 is extended from ISO8859-11 */
    {
     932, "SJIS",
    }, /* SHIFTJIS_CHARSET */
    {
     936, "GBK",
    }, /* GB2313_CHARSET */
    {
     949, "UHC",
    }, /* HANGEUL_CHARSET */
    {
     950, "BIG5",
    }, /* CHINESEBIG5_CHARSET */
};

char *bl_get_codeset_win32(void) {
  int count;
  int codepage;

  codepage = GetACP();

  for (count = 0; count < sizeof(cp_cs_table) / sizeof(cp_cs_table_t); count++) {
    if (cp_cs_table[count].codepage == codepage) {
      return cp_cs_table[count].codeset;
    }
  }

  return "ISO8859-1";
}

#endif /* HAVE_WINDOWS_H */
