/*
 *	$Id$
 */

#ifndef __BL_LOCALE_H__
#define __BL_LOCALE_H__

#include "bl_def.h" /* HAVE_WINDOWS_H */

int bl_locale_init(const char *locale);

int bl_locale_final(void);

char *bl_get_locale(void);

/*
 * lang/country/codeset are decided as below , not decided only by setlocale().
 * Be careful to use them.
 *
 * <order>
 * setlocale() (=> nl_langinfo(CODESET)) => LC_ALL => LC_CTYPE => LANG
 */

char *bl_get_lang(void);

char *bl_get_country(void);

#ifdef USE_WIN32API
#define bl_get_codeset bl_get_codeset_win32
#else
char *bl_get_codeset(void);
#endif

#ifdef HAVE_WINDOWS_H
char *bl_get_codeset_win32(void);
#endif

#endif
