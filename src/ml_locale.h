/*
 *	update: <2001/11/14(11:14:15)>
 *	$Id$
 */

#ifndef  __ML_LOCALE_H__
#define  __ML_LOCALE_H__


int  ml_locale_init(char *  locale) ;

int  ml_locale_final(void) ;

char *  ml_get_locale(void) ;

char *  ml_get_lang(void) ;

char *  ml_get_country(void) ;

char *  ml_get_codeset(void) ;


#endif
