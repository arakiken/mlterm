/*
 *	$Id$
 */

#ifndef  __ML_LOCALE_H__
#define  __ML_LOCALE_H__


int  ml_locale_init(char *  locale) ;

int  ml_locale_final(void) ;

char *  ml_get_locale(void) ;

/*
 * lang/country/codeset are decided as below , not decided only by setlocale().
 * Be careful to use them.
 *
 * <order>
 * setlocale() (=> nl_langinfo(CODESET)) => LC_ALL => LC_CTYPE => LANG
 */

char *  ml_get_lang(void) ;

char *  ml_get_country(void) ;

char *  ml_get_codeset(void) ;


#endif
