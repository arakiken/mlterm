/*
 *	$Id$
 */

#ifndef  __KIK_LOCALE_H__
#define  __KIK_LOCALE_H__


#include  "kik_config.h"	/* HAVE_WINDOWS_H */


int  kik_locale_init(char *  locale) ;

int  kik_locale_final(void) ;

char *  kik_get_locale(void) ;

/*
 * lang/country/codeset are decided as below , not decided only by setlocale().
 * Be careful to use them.
 *
 * <order>
 * setlocale() (=> nl_langinfo(CODESET)) => LC_ALL => LC_CTYPE => LANG
 */

char *  kik_get_lang(void) ;

char *  kik_get_country(void) ;

#ifdef  USE_WIN32API
#define  kik_get_codeset  kik_get_codeset_win32
#else
char *  kik_get_codeset(void) ;
#endif

#ifdef  HAVE_WINDOWS_H
char *  kik_get_codeset_win32(void) ;
#endif


#endif
