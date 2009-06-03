/*
 *	$Id$
 */

#include  "kik_langinfo.h"


#ifdef  USE_BUILTIN_LANGINFO


#ifdef  USE_WIN32API

#include  <windows.h>

typedef struct cp_cs_table
{
	int  codepage ;
	char *  codeset ;
	
} cp_cs_table_t ;

static cp_cs_table_t  cp_cs_table[] =
{
	{ 1252 , "ISO8859-1" , } ,	/* ANSI_CHARSET */
	{ 1251 , "CP1251" , } ,		/* RUSSIAN_CHARSET */
	{ 1250 , "CP1250" , } ,		/* EE_CHARSET */
	{ 1253 , "CP1253" , } ,		/* GREEK_CHARSET */
	{ 1254 , "CP1254" , } ,		/* TURKISH_CHARSET */
	{ 1257 , "CP1257" , } ,		/* BALTIC_CHARSET */
	{ 1255 , "CP1255" , } ,		/* HEBREW_CHARSET */
	{ 1256 , "CP1256" , } ,		/* ARABIC_CHARSET */
	{ 932 ,	"SJIS" , } ,		/* SHIFTJIS_CHARSET */
	{ 949 ,	"UHC" , } ,		/* HANGEUL_CHARSET */
	{ 936 ,	"EUCCN" , } ,		/* GB2313_CHARSET */
	{ 950 ,	"BIG5" , } ,		/* CHINESEBIG5_CHARSET */
} ;

#endif	/* USE_WIN32API */


/* --- global functions --- */

char *
__kik_langinfo(
	nl_item  item
	)
{
#ifdef  USE_WIN32API
	if( item == CODESET)
	{
		int  count ;
		int  codepage ;

		codepage = GetACP() ;

		for( count = 0 ; count < sizeof( cp_cs_table) / sizeof( cp_cs_table_t) ; count ++)
		{
			if( cp_cs_table[count].codepage == codepage)
			{
				return  cp_cs_table[count].codeset ;
			}
		}
	}
#endif /* USE_WIN32API */

	return  "" ;
}


#endif /* USE_BUILTIN_LANGINFO */
