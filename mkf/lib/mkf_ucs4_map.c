/*
 *	$Id$
 */

#include  "mkf_ucs4_map.h"

#include  <string.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_iso8859.h"
#include  "mkf_ucs4_viscii.h"
#include  "mkf_ucs4_koi8.h"
#include  "mkf_ucs4_iscii.h"
#include  "mkf_ucs4_georgian_ps.h"
#include  "mkf_ucs4_cp125x.h"
#include  "mkf_ucs4_jisx0201.h"
#include  "mkf_ucs4_jisx0208.h"
#include  "mkf_ucs4_jisx0212.h"
#include  "mkf_ucs4_jisx0213.h"
#include  "mkf_ucs4_ksc5601.h"
#include  "mkf_ucs4_uhc.h"
#include  "mkf_ucs4_johab.h"
#include  "mkf_ucs4_gb2312.h"
#include  "mkf_ucs4_gbk.h"
#include  "mkf_ucs4_big5.h"
#include  "mkf_ucs4_cns11643.h"


typedef struct  map
{
	mkf_charset_t  cs ;
	int  (*map_ucs4_to)( mkf_char_t * , u_int32_t) ;
	int  (*map_to_ucs4)( mkf_char_t * , u_int16_t) ;
	
} map_t ;


/* --- static variables --- */

static map_t  map_table[] =
{
	{ ISO8859_1_R , mkf_map_ucs4_to_iso8859_1_r , mkf_map_iso8859_1_r_to_ucs4 } ,
	{ ISO8859_2_R , mkf_map_ucs4_to_iso8859_2_r , mkf_map_iso8859_2_r_to_ucs4 } ,
	{ ISO8859_3_R , mkf_map_ucs4_to_iso8859_3_r , mkf_map_iso8859_3_r_to_ucs4 } ,
	{ ISO8859_4_R , mkf_map_ucs4_to_iso8859_4_r , mkf_map_iso8859_4_r_to_ucs4 } ,
	{ ISO8859_5_R , mkf_map_ucs4_to_iso8859_5_r , mkf_map_iso8859_5_r_to_ucs4 } ,
	{ ISO8859_6_R , mkf_map_ucs4_to_iso8859_6_r , mkf_map_iso8859_6_r_to_ucs4 } ,
	{ ISO8859_7_R , mkf_map_ucs4_to_iso8859_7_r , mkf_map_iso8859_7_r_to_ucs4 } ,
	{ ISO8859_8_R , mkf_map_ucs4_to_iso8859_8_r , mkf_map_iso8859_8_r_to_ucs4 } ,
	{ ISO8859_9_R , mkf_map_ucs4_to_iso8859_9_r , mkf_map_iso8859_9_r_to_ucs4 } ,
	{ ISO8859_10_R , mkf_map_ucs4_to_iso8859_10_r , mkf_map_iso8859_10_r_to_ucs4 } ,
	{ TIS620_2533 , mkf_map_ucs4_to_tis620_2533 , mkf_map_tis620_2533_to_ucs4 } ,
	{ ISO8859_13_R , mkf_map_ucs4_to_iso8859_13_r , mkf_map_iso8859_13_r_to_ucs4 } ,
	{ ISO8859_14_R , mkf_map_ucs4_to_iso8859_14_r , mkf_map_iso8859_14_r_to_ucs4 } ,
	{ ISO8859_15_R , mkf_map_ucs4_to_iso8859_15_r , mkf_map_iso8859_15_r_to_ucs4 } ,
	{ ISO8859_16_R , mkf_map_ucs4_to_iso8859_16_r , mkf_map_iso8859_16_r_to_ucs4 } ,
	{ TCVN5712_3_1993 , mkf_map_ucs4_to_tcvn5712_3_1993 , mkf_map_tcvn5712_3_1993_to_ucs4 } ,
	
	{ VISCII , mkf_map_ucs4_to_viscii , mkf_map_viscii_to_ucs4 } ,
	{ KOI8_R , mkf_map_ucs4_to_koi8_r , mkf_map_koi8_r_to_ucs4 } ,
	{ KOI8_U , mkf_map_ucs4_to_koi8_u , mkf_map_koi8_u_to_ucs4 } ,
	{ ISCII_ASSAMESE , mkf_map_ucs4_to_iscii , mkf_map_iscii_assamese_to_ucs4 } ,
	{ ISCII_BENGALI , mkf_map_ucs4_to_iscii , mkf_map_iscii_bengali_to_ucs4 } ,
	{ ISCII_GUJARATI , mkf_map_ucs4_to_iscii , mkf_map_iscii_gujarati_to_ucs4 } ,
	{ ISCII_HINDI , mkf_map_ucs4_to_iscii , mkf_map_iscii_hindi_to_ucs4 } ,
	{ ISCII_KANNADA , mkf_map_ucs4_to_iscii , mkf_map_iscii_kannada_to_ucs4 } ,
	{ ISCII_MALAYALAM , mkf_map_ucs4_to_iscii , mkf_map_iscii_malayalam_to_ucs4 } ,
	{ ISCII_ORIYA , mkf_map_ucs4_to_iscii , mkf_map_iscii_oriya_to_ucs4 } ,
	{ ISCII_PUNJABI , mkf_map_ucs4_to_iscii , mkf_map_iscii_punjabi_to_ucs4 } ,
	{ ISCII_ROMAN , mkf_map_ucs4_to_iscii , mkf_map_iscii_roman_to_ucs4 } ,
	{ ISCII_TAMIL , mkf_map_ucs4_to_iscii , mkf_map_iscii_tamil_to_ucs4 } ,
	{ ISCII_TELUGU , mkf_map_ucs4_to_iscii , mkf_map_iscii_telugu_to_ucs4 } ,
	{ KOI8_T , mkf_map_ucs4_to_koi8_t , mkf_map_koi8_t_to_ucs4 } ,
	{ GEORGIAN_PS , mkf_map_ucs4_to_georgian_ps , mkf_map_georgian_ps_to_ucs4 } ,
	{ CP1250 , mkf_map_ucs4_to_cp1250 , mkf_map_cp1250_to_ucs4 } ,
	{ CP1251 , mkf_map_ucs4_to_cp1251 , mkf_map_cp1251_to_ucs4 } ,
	{ CP1252 , mkf_map_ucs4_to_cp1252 , mkf_map_cp1252_to_ucs4 } ,
	{ CP1253 , mkf_map_ucs4_to_cp1253 , mkf_map_cp1253_to_ucs4 } ,
	{ CP1254 , mkf_map_ucs4_to_cp1254 , mkf_map_cp1254_to_ucs4 } ,
	{ CP1255 , mkf_map_ucs4_to_cp1255 , mkf_map_cp1255_to_ucs4 } ,
	{ CP1256 , mkf_map_ucs4_to_cp1256 , mkf_map_cp1256_to_ucs4 } ,
	{ CP1257 , mkf_map_ucs4_to_cp1257 , mkf_map_cp1257_to_ucs4 } ,
	{ CP1258 , mkf_map_ucs4_to_cp1258 , mkf_map_cp1258_to_ucs4 } ,
	{ CP874 , mkf_map_ucs4_to_cp874 , mkf_map_cp874_to_ucs4 } ,
	
	{ JISX0201_ROMAN , mkf_map_ucs4_to_jisx0201_roman , mkf_map_jisx0201_roman_to_ucs4 } ,
	{ JISX0201_KATA , mkf_map_ucs4_to_jisx0201_kata , mkf_map_jisx0201_kata_to_ucs4 } ,
	{ JISX0208_1983 , mkf_map_ucs4_to_jisx0208_1983 , mkf_map_jisx0208_1983_to_ucs4 } ,
	{ JISX0212_1990 , mkf_map_ucs4_to_jisx0212_1990 , mkf_map_jisx0212_1990_to_ucs4 } ,
	{ JISX0213_2000_1 , mkf_map_ucs4_to_jisx0213_2000_1 , mkf_map_jisx0213_2000_1_to_ucs4 } ,
	{ JISX0213_2000_2 , mkf_map_ucs4_to_jisx0213_2000_2 , mkf_map_jisx0213_2000_2_to_ucs4 } ,
	{ JISC6226_1978_NEC_EXT , mkf_map_ucs4_to_jisx0208_nec_ext , mkf_map_jisx0208_nec_ext_to_ucs4 } ,
	{ JISC6226_1978_NECIBM_EXT , mkf_map_ucs4_to_jisx0208_necibm_ext ,
		mkf_map_jisx0208_necibm_ext_to_ucs4 } ,
	{ SJIS_IBM_EXT , mkf_map_ucs4_to_sjis_ibm_ext , mkf_map_sjis_ibm_ext_to_ucs4 } ,
	
	{ GB2312_80 , mkf_map_ucs4_to_gb2312_80 , mkf_map_gb2312_80_to_ucs4 } ,
	{ GBK , mkf_map_ucs4_to_gbk , mkf_map_gbk_to_ucs4 } ,
	
	{ CNS11643_1992_1 , mkf_map_ucs4_to_cns11643_1992_1 , mkf_map_cns11643_1992_1_to_ucs4 } ,
	{ CNS11643_1992_2 , mkf_map_ucs4_to_cns11643_1992_2 , mkf_map_cns11643_1992_2_to_ucs4 } ,
	{ CNS11643_1992_3 , mkf_map_ucs4_to_cns11643_1992_3 , mkf_map_cns11643_1992_3_to_ucs4 } ,
	{ BIG5 , mkf_map_ucs4_to_big5 , mkf_map_big5_to_ucs4 } ,
	{ HKSCS , mkf_map_ucs4_to_hkscs , mkf_map_hkscs_to_ucs4 } ,

	{ KSC5601_1987 , mkf_map_ucs4_to_ksc5601_1987 , mkf_map_ksc5601_1987_to_ucs4 } ,
	{ UHC , mkf_map_ucs4_to_uhc , mkf_map_uhc_to_ucs4 } ,
	{ JOHAB , mkf_map_ucs4_to_johab , mkf_map_johab_to_ucs4 } ,
	
} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_cs(
	mkf_char_t *  non_ucs ,
	mkf_char_t *  ucs4 ,
	mkf_charset_t  cs
	)
{
	u_int32_t  ucs4_code ;
	map_t *  map ;
	static map_t *  cached_map ;

#ifdef  DEBUG
	if( ucs4->cs != ISO10646_UCS4_1)
	{
		kik_debug_printf( KIK_DEBUG_TAG " ucs4 is not ucs4.\n") ;

		return  0 ;
	}
#endif

	ucs4_code = mkf_char_to_int( ucs4) ;

	if( ! ( map = cached_map) || map->cs != cs)
	{
		size_t  count ;

		for( count = 0 ; count < sizeof( map_table) / sizeof( map_t) ; count++)
		{
			if( map_table[count].cs == cs)
			{
				cached_map = map = &map_table[count] ;

				goto  found ;
			}
		}

	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %x cs is not supported to map to ucs4.\n" , cs) ;
	#endif

		return  0 ;
	}

found:
	if( (*map->map_ucs4_to)( non_ucs , ucs4_code))
	{
		return  1 ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" UCS4 char(0x%.2x%.2x%.2x%.2x) is not supported to %x cs.\n" ,
			ucs4->ch[0] , ucs4->ch[1] , ucs4->ch[2] , ucs4->ch[3] , cs) ;
	#endif

		return  0 ;
	}
}

int
mkf_map_ucs4_to_with_funcs(
	mkf_char_t *  non_ucs ,
	mkf_char_t *  ucs4 ,
	mkf_map_ucs4_to_func_t *  map_ucs4_to_funcs ,
	size_t  list_size
	)
{
	size_t  count ;
	u_int32_t  ucs4_code ;

#ifdef  DEBUG
	if( ucs4->cs != ISO10646_UCS4_1)
	{
		kik_debug_printf( KIK_DEBUG_TAG " ucs4 is not ucs4.\n") ;

		return  0 ;
	}
#endif

	ucs4_code = mkf_char_to_int( ucs4) ;
	
	for( count = 0 ; count < list_size ; count ++)
	{
		if( (*map_ucs4_to_funcs[count])( non_ucs , ucs4_code))
		{
			return  1 ;
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " UCS4 char(0x%.2x%.2x%.2x%.2x) is not supported.\n" ,
		ucs4->ch[0] , ucs4->ch[1] , ucs4->ch[2] , ucs4->ch[3]) ;
#endif

	return  0 ;
}

/*
 * using the default order of the mapping table.
 */
int
mkf_map_ucs4_to(
	mkf_char_t *  non_ucs ,
	mkf_char_t *  ucs4
	)
{
	size_t  count ;
	u_int32_t  ucs4_code ;
	map_t *  map ;
	static map_t *  cached_map ;

#ifdef  DEBUG
	if( ucs4->cs != ISO10646_UCS4_1)
	{
		kik_debug_printf( KIK_DEBUG_TAG " ucs4 is not ucs4.\n") ;

		return  0 ;
	}
#endif

	ucs4_code = mkf_char_to_int( ucs4) ;

	if( ( map = cached_map) && (*map->map_ucs4_to)( non_ucs , ucs4_code))
	{
		return  1 ;
	}
	
	for( count = 0 ; count < sizeof( map_table) / sizeof( map_table[0]) ; count++)
	{
		if( (*map_table[count].map_ucs4_to)( non_ucs , ucs4_code))
		{
			mkf_charset_t  cs ;

			cs = map_table[count].cs ;

			/*
			 * Don't cache the map functions of JISX0213_2000_1 and
			 * non ISO2022 cs (GBK etc), in order not to map the
			 * following chars automatically to JISX0213_2000_1,
			 * GBK etc if a ucs4 character is mapped to the one of
			 * JISX0213_2000_1, GBK etc which doesn't exist in
			 * JISX0208, GB2312 etc.
			 */
			if( ! IS_NON_ISO2022(cs) && cs != JISX0213_2000_1)
			{
				cached_map = &map_table[count] ;
			}

			return  1 ;
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " UCS4 char(0x%.2x%.2x%.2x%.2x) is not supported.\n" ,
		ucs4->ch[0] , ucs4->ch[1] , ucs4->ch[2] , ucs4->ch[3]) ;
#endif

	return  0 ;
}

/*
 * using the default order of the mapping table.
 */
int
mkf_map_ucs4_to_iso2022cs(
	mkf_char_t *  non_ucs ,
	mkf_char_t *  ucs4
	)
{
	size_t  count ;
	u_int32_t  ucs4_code ;
	map_t *  map ;
	static map_t *  cached_map ;

#ifdef  DEBUG
	if( ucs4->cs != ISO10646_UCS4_1)
	{
		kik_debug_printf( KIK_DEBUG_TAG " ucs4 is not ucs4.\n") ;

		return  0 ;
	}
#endif

	ucs4_code = mkf_char_to_int( ucs4) ;

	if( ( map = cached_map) && (*map->map_ucs4_to)( non_ucs , ucs4_code))
	{
		return  1 ;
	}

	for( count = 0 ; count < sizeof( map_table) / sizeof( map_table[0]) ; count ++)
	{
		if( IS_CS_BASED_ON_ISO2022(map_table[count].cs))
		{
			if( (*map_table[count].map_ucs4_to)( non_ucs , ucs4_code))
			{
				cached_map = &map_table[count] ;

				return  1 ;
			}
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " UCS4 char(0x%.2x%.2x%.2x%.2x) is not supported.\n" ,
		ucs4->ch[0] , ucs4->ch[1] , ucs4->ch[2] , ucs4->ch[3]) ;
#endif

	return  0 ;
}

int
mkf_map_to_ucs4(
	mkf_char_t *  ucs4 ,
	mkf_char_t *  non_ucs
	)
{
	u_int32_t  code ;
	map_t *  map ;
	static map_t *  cached_map ;

	if( non_ucs->cs == ISO10646_UCS4_1)
	{
		*ucs4 = *non_ucs ;

		return  1 ;
	}
	
	code = mkf_char_to_int( non_ucs) ;

	if( ! ( map = cached_map) || map->cs != non_ucs->cs)
	{
		size_t  count ;

		for( count = 0 ; count < sizeof( map_table) / sizeof( map_t) ; count ++)
		{
			if( map_table[count].cs == non_ucs->cs)
			{
				cached_map = map = &map_table[count] ;

				goto  found ;
			}
		}

	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" %x cs is not supported to map to ucs4.\n" , non_ucs->cs) ;
	#endif

		return  0 ;
	}

found:
	if( (*map->map_to_ucs4)( ucs4 , code))
	{
		return  1 ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" this cs(%x) (code %x) cannot be mapped to UCS4.\n" ,
			non_ucs->cs , code) ;
	#endif

		return  0 ;
	}
}

int
mkf_map_via_ucs(
	mkf_char_t *  dst ,
	mkf_char_t *  src ,
	mkf_charset_t  cs
	)
{
	mkf_char_t  ucs4 ;

	if( ! mkf_map_to_ucs4( &ucs4 , src) || ! mkf_map_ucs4_to_cs( dst , &ucs4 , cs))
	{
		return  0 ;
	}

	return  1 ;
}
