/*
 *	$Id$
 */

#include  "mkf_locale_ucs4_map.h"

#include  <string.h>		/* strncmp */
#include  <kiklib/kik_locale.h>
#include  <kiklib/kik_util.h>	/* K_MIN */

#include  "mkf_ucs4_map.h"
#include  "mkf_ja_jp_map.h"
#include  "mkf_zh_tw_map.h"
#include  "mkf_zh_cn_map.h"
#include  "mkf_zh_hk_map.h"
#include  "mkf_ko_kr_map.h"
#include  "mkf_viet_map.h"
#include  "mkf_ru_map.h"


typedef int (*map_func_t)( mkf_char_t *  , mkf_char_t *) ;

typedef struct  map_ucs4_to_func_table
{
	char *  locale ;
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
	{ "ja" , mkf_map_ucs4_to_ja_jp } ,
	{ "ko" , mkf_map_ucs4_to_ko_kr } ,
	{ "ru" , mkf_map_ucs4_to_ru } ,
	{ "vi" , mkf_map_ucs4_to_viet } ,
	{ "zh_CN" , mkf_map_ucs4_to_zh_cn } ,
	{ "zh_TW" , mkf_map_ucs4_to_zh_tw } ,
	{ "zh_HK" , mkf_map_ucs4_to_zh_hk } ,
	{ "zh" , mkf_map_ucs4_to_zh_cn } ,
} ;


/* --- static functions --- */

static map_func_t
get_map_ucs4_to_func_for_current_locale(void)
{
	int  counter ;
	char *  locale ;

	locale = kik_get_locale() ;

	for( counter = 0 ;
		counter < sizeof( map_ucs4_to_func_table) / sizeof( map_ucs4_to_func_table[0]) ;
		counter ++)
	{
		if( strncmp( map_ucs4_to_func_table[counter].locale , locale ,
				K_MIN( strlen( map_ucs4_to_func_table[counter].locale) ,
					strlen( locale))) == 0)
		{
			return  map_ucs4_to_func_table[counter].func ;
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
		(*func)( non_ucs4 , ucs4) == 0)
	{
		return  mkf_map_ucs4_to( non_ucs4 , ucs4) ;
	}

	return  1 ;
}
