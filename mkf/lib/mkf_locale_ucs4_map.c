/*
 *	$Id$
 */

#include  "mkf_locale_ucs4_map.h"

#include  <string.h>		/* strncmp */
#include  <kiklib/kik_locale.h>

#include  "mkf_ucs4_map.h"
#include  "mkf_ja_jp_map.h"
#include  "mkf_ko_kr_map.h"
#include  "mkf_ru_map.h"
#include  "mkf_tg_map.h"
#include  "mkf_uk_map.h"
#include  "mkf_viet_map.h"
#include  "mkf_zh_cn_map.h"
#include  "mkf_zh_hk_map.h"
#include  "mkf_zh_tw_map.h"


typedef int (*map_func_t)( mkf_char_t *  , mkf_char_t *) ;

typedef struct  map_ucs4_to_func_table
{
	char *  lang ;
	char *  country ;
	map_func_t  func ;

} map_ucs4_to_func_table_t ;


/* --- static variables --- */

/*
 * XXX
 * in the future , mkf_map_ucs4_to_[locale]_iso2022cs() should be prepared.
 *
 * XXX
 * other languages(especially ISO8859 variants) are not supported yet.
 */
static map_ucs4_to_func_table_t  map_ucs4_to_func_table[] =
{
	{ "ja" , NULL , mkf_map_ucs4_to_ja_jp } ,
	{ "ko" , NULL , mkf_map_ucs4_to_ko_kr } ,
	{ "ru" , NULL , mkf_map_ucs4_to_ru } ,
	{ "tg" , NULL , mkf_map_ucs4_to_tg } ,
	{ "uk" , NULL , mkf_map_ucs4_to_uk } ,
	{ "vi" , NULL , mkf_map_ucs4_to_viet } ,
	{ "zh" , "CN" , mkf_map_ucs4_to_zh_cn } ,
	{ "zh" , "HK" , mkf_map_ucs4_to_zh_hk } ,
	{ "zh" , "TW" , mkf_map_ucs4_to_zh_tw } ,
	{ "zh" , NULL , mkf_map_ucs4_to_zh_tw } ,
} ;


/* --- static functions --- */

static map_func_t
get_map_ucs4_to_func_for_current_locale(void)
{
	size_t  count ;
	char *  lang ;
	char *  country ;
	map_ucs4_to_func_table_t *  tablep ;
	static map_func_t  cached_func ;
	static int  cached ;

	/* Once cached, never changed. NULL is also cached. */
	if( cached)
	{
		return  cached_func ;
	}

	cached = 1 ;

	lang = kik_get_lang() ;
	country = kik_get_country() ;

	for( count = 0 ;
		count < sizeof( map_ucs4_to_func_table) / sizeof( map_ucs4_to_func_table[0]) ;
		count ++)
	{
		tablep = map_ucs4_to_func_table + count ;

		if( ! strcmp( tablep->lang, lang) &&
		    ( ! tablep->country || ! strcmp( tablep->country, country)))
		{
			return  ( cached_func = tablep->func) ;
		}
	}

	return  NULL ;
}


/* --- global functions --- */

int
mkf_map_locale_ucs4_to(
	mkf_char_t *  non_ucs4 ,
	mkf_char_t *  ucs4
	)
{
	map_func_t  func ;

	if( ( func = get_map_ucs4_to_func_for_current_locale()) == NULL ||
		! (*func)( non_ucs4 , ucs4))
	{
		return  mkf_map_ucs4_to( non_ucs4 , ucs4) ;
	}

	return  1 ;
}
