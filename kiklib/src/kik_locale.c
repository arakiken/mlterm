/*
 *	$Id$
 */

#include  "kik_locale.h"

#include  <stdio.h>		/* sprintf */
#include  <locale.h>		/* setlocale() */

#include  "kik_langinfo.h"	/* kik_langinfo() */
#include  "kik_debug.h"
#include  "kik_mem.h"		/* alloca */
#include  "kik_str.h"
#include  "kik_util.h"		/* K_MIN */


#if  0
#define  __DEBUG
#endif


typedef struct  lang_codeset_table
{
	char *  lang ;
	char *  codeset ;

} lang_codeset_table_t ;

typedef struct  alias_codeset_table
{
	char *  codeset ;
	char *  locale ;
	
	char *  alias ;

} alias_codeset_table_t ;


/* --- static variables --- */

static char *  sys_locale = NULL ;
static char *  sys_lang = NULL ;
static char *  sys_country = NULL ;
static char *  sys_codeset = NULL ;

/* for sys_lang and sys_country memory */
static char *  sys_lang_country = NULL ;

static lang_codeset_table_t  lang_codeset_table[] =
{
	{ "en" , "ISO8859-1" , } ,
	{ "da" , "ISO8859-1" , } ,
	{ "de" , "ISO8859-1" , } ,
	{ "fi" , "ISO8859-1" , } ,
	{ "fr" , "ISO8859-1" , } ,
	{ "is" , "ISO8859-1" , } ,
	{ "it" , "ISO8859-1" , } ,
	{ "nl" , "ISO8859-1" , } ,
	{ "no" , "ISO8859-1" , } ,
	{ "pt" , "ISO8859-1" , } ,
	{ "sv" , "ISO8859-1" , } ,
	{ "cs" , "ISO8859-2" , } ,
	{ "hr" , "ISO8859-2" , } ,
	{ "hu" , "ISO8859-2" , } ,
	{ "la" , "ISO8859-2" , } ,
	{ "lt" , "ISO8859-2" , } ,
	{ "pl" , "ISO8859-2" , } ,
	{ "sl" , "ISO8859-2" , } ,
	{ "el" , "ISO8859-7" , } ,
	{ "ru" , "KOI8-R" , } ,
	{ "uk" , "KOI8-U" , } ,
	{ "vi" , "VISCII" , } ,
	{ "th" , "TIS-620" , } ,
	{ "ja" , "eucJP" , } ,
	{ "ko" , "eucKR" , } ,
	{ "zh_CN" , "eucCN" , } ,
	{ "zh_TW" , "Big5" , } ,
	{ "zh_HK" , "Big5HKSCS" , } ,
	
} ;

static alias_codeset_table_t  alias_codeset_table[] =
{
	{ "EUC" , "ja_JP.EUC" , "eucJP" , } ,
	{ "EUC" , "ko_KR.EUC" , "eucKR" , } ,
} ;


/* --- global functions --- */

int
kik_locale_init(
	char *  locale
	)
{
	char *  locale_p ;
	int  result ;

	if( sys_locale && strcmp( locale , sys_locale) == 0)
	{
		return  1 ;
	}
	
	if( sys_lang_country)
	{
		free( sys_lang_country) ;
		sys_lang_country = NULL ;
	}
	
	if( ( locale = setlocale( LC_CTYPE , locale)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " setlocale() failed.\n") ;
	#endif

		result = 0 ;

		if( sys_locale)
		{
			/* restoring locale info. nothing is changed. */
			
			setlocale( LC_CTYPE , sys_locale) ;
			
			return  0 ;
		}
		else
		{
			/* sys_locale is NULL */
			
			if( ( locale = getenv( "LC_ALL")) == NULL &&
				( locale = getenv( "LC_CTYPE")) == NULL &&
				( locale = getenv( "LANG")) == NULL)
			{
				/* nothing is changed */
				
				return  0 ;
			}
		}
	}
	else
	{
		sys_locale = locale ;
		result = 1 ;
	}

	if( ( locale_p = sys_lang_country = strdup( locale)) == NULL)
	{
		sys_locale = NULL ;
		
		return  0 ;
	}
	
	if( ( sys_lang = kik_str_sep( &locale_p , "_")) == NULL)
	{
		/* this never happends */
		
		return  0 ;
	}
	
	sys_country = kik_str_sep( &locale_p , ".") ;

	sys_codeset = kik_langinfo( CODESET) ;
	if( strcmp( sys_codeset , "") == 0)
	{
		if( locale_p && *locale_p)
		{
			sys_codeset = locale_p ;
		}
		else
		{
			sys_codeset = NULL ;
		}
	}

	if( sys_codeset)
	{
		/*
		 * normalizing codeset name.
		 */
		 
		int  counter ;
		
		for( counter = 0 ;
			counter < sizeof( alias_codeset_table) / sizeof( alias_codeset_table[0]) ;
			counter ++)
		{
			if( strcmp( sys_codeset , alias_codeset_table[counter].codeset) == 0 &&
				strcmp( locale , alias_codeset_table[counter].locale) == 0)
			{
				sys_codeset = alias_codeset_table[counter].alias ;
				
				break ;
			}
		}
	}

#ifdef  __DEBUG
	kik_debug_printf( "locale setttings -> locale %s lang %s country %s codeset %s\n" ,
		sys_locale , sys_lang , sys_country , sys_codeset) ;
#endif

	return  result ;
}

int
kik_locale_final(void)
{
	if( sys_lang_country)
	{
		free( sys_lang_country) ;
		sys_lang_country = NULL ;
	}

	return  1 ;
}

char *
kik_get_locale(void)
{
	if( sys_locale)
	{
		return  sys_locale ;
	}
	else
	{
		return  "C" ;
	}
}

char *
kik_get_lang(void)
{
	if( sys_lang)
	{
		return  sys_lang ;
	}
	else
	{
		return  "en" ;
	}
}

char *
kik_get_country(void)
{
	if( sys_country)
	{
		return  sys_country ;
	}
	else
	{
		return  "US" ;
	}
}

char *
kik_get_codeset(void)
{
	if( sys_codeset)
	{
		return  sys_codeset ;
	}
	else if( sys_lang)
	{
		int  counter ;
		char *  lang ;
		u_int  lang_len ;

		lang_len = strlen( sys_lang) + 1 ;
		if( sys_country)
		{
			/* "+ 1" is for '_' */
			lang_len += strlen( sys_country) + 1 ;
		}
		
		if( ( lang = alloca( lang_len)) == NULL)
		{
			return  "ISO8859-1" ;
		}

		if( sys_country)
		{
			sprintf( lang , "%s_%s" , sys_lang , sys_country) ;
		}
		else
		{
			sprintf( lang , "%s" , sys_lang) ;
		}

	#ifdef  __DEBUG
		kik_debug_printf( "lang -> %s\n" , lang) ;
	#endif

		for( counter = 0 ;
			counter < sizeof( lang_codeset_table) / sizeof( lang_codeset_table[0]) ;
			counter ++)
		{
			if( strncmp( lang , lang_codeset_table[counter].lang ,
				/* lang_len *- 1* is excluing NULL */
				K_MIN(lang_len - 1,strlen(lang_codeset_table[counter].lang))) == 0)
			{
				return  lang_codeset_table[counter].codeset ;
			}
		}
	}
	
	return  "ISO8859-1" ;
}
