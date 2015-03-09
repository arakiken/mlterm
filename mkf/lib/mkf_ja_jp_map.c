/*
 *	$Id$
 */

#include  <string.h>            /* memcpy */

#include  "mkf_ja_jp_map.h"

#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_jisx0201.h"
#include  "mkf_ucs4_jisx0208.h"
#include  "mkf_ucs4_jisx0212.h"
#include  "mkf_ucs4_jisx0213.h"


/* --- static variables --- */

static mkf_map_ucs4_to_func_t  map_ucs4_to_funcs[] =
{
	mkf_map_ucs4_to_jisx0201_roman ,
	mkf_map_ucs4_to_jisx0201_kata ,
	mkf_map_ucs4_to_jisx0208_1983 ,
	mkf_map_ucs4_to_jisx0212_1990 ,
	mkf_map_ucs4_to_jisx0213_2000_1 ,
	mkf_map_ucs4_to_jisx0213_2000_2 ,

} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_ja_jp(
	mkf_char_t *  jajp ,
	mkf_char_t *  ucs4
	)
{
	return  mkf_map_ucs4_to_with_funcs( jajp , ucs4 , map_ucs4_to_funcs ,
		sizeof( map_ucs4_to_funcs) / sizeof( map_ucs4_to_funcs[0])) ;
}


int
mkf_map_jisx0213_2000_1_to_jisx0208_1983(
	mkf_char_t *  jis2k ,
	mkf_char_t *  jis83
	)
{
	memcpy( jis2k->ch , jis83->ch , 2) ;
	jis2k->size = 2 ;
	jis2k->cs = JISX0213_2000_1 ;
	jis2k->property = jis83->property ;

	return  1 ;
}

int
mkf_map_jisx0208_1983_to_jisx0213_2000_1(
	mkf_char_t *  jis83 ,
	mkf_char_t *  jis2k
	)
{
	/*
	 * XXX
	 * since JISX0213-1 is upper compatible with JISX0208 , some chars of JISX0213-1
	 * must not be mapped to JISX0208.
	 */
	memcpy( jis83->ch , jis2k->ch , 2) ;
	jis83->size = 2 ;
	jis83->cs = JISX0213_2000_1 ;
	jis83->property = jis2k->property ;
	
	return  1 ;
}

int
mkf_map_jisx0213_2000_2_to_jisx0212_1990(
	mkf_char_t *  jis90 ,
	mkf_char_t *  jis2k
	)
{
	return  mkf_map_via_ucs( jis2k , jis90 , JISX0213_2000_2) ;
}

int
mkf_map_jisx0212_1990_to_jisx0213_2000_2(
	mkf_char_t *  jis2k ,
	mkf_char_t *  jis90
	)
{
	return  mkf_map_via_ucs( jis90 , jis2k , JISX0212_1990) ;
}


/*
 * Gaiji characters
 */

/* SJIS_IBM_EXT */
 
int
mkf_map_sjis_ibm_ext_to_jisx0208_1983(
	mkf_char_t *  jis ,
	mkf_char_t *  ibm
	)
{
	return  mkf_map_via_ucs( jis , ibm , JISX0208_1983) ;
}

int
mkf_map_sjis_ibm_ext_to_jisx0212_1990(
	mkf_char_t *  jis ,
	mkf_char_t *  ibm
	)
{
	return  mkf_map_via_ucs( jis , ibm , JISX0212_1990) ;
}

int
mkf_map_sjis_ibm_ext_to_jisx0213_2000(
	mkf_char_t *  jis ,
	mkf_char_t *  ibm
	)
{
	mkf_char_t  ucs4 ;

	if( ! mkf_map_to_ucs4( &ucs4 , ibm))
	{
		return  0 ;
	}

	if( ! mkf_map_ucs4_to_cs( jis , &ucs4 , JISX0213_2000_2) &&
		! mkf_map_ucs4_to_cs( jis , &ucs4 , JISX0213_2000_1))
	{
		return  0 ;
	}

	return  1 ;
}

int
mkf_map_jisx0212_1990_to_sjis_ibm_ext(
	mkf_char_t *  ibm ,
	mkf_char_t *  jis
	)
{
	return  mkf_map_via_ucs( ibm , jis , SJIS_IBM_EXT) ;
}

int
mkf_map_jisx0213_2000_2_to_sjis_ibm_ext(
	mkf_char_t *  ibm ,
	mkf_char_t *  jis
	)
{
	return  mkf_map_via_ucs( ibm , jis , SJIS_IBM_EXT) ;
}


/* NEC EXT */

int
mkf_map_jisx0208_nec_ext_to_jisx0208_1983(
	mkf_char_t *  jis ,
	mkf_char_t *  nec_ext
	)
{
	return  mkf_map_via_ucs( jis , nec_ext , JISX0208_1983) ;
}

int
mkf_map_jisx0208_nec_ext_to_jisx0212_1990(
	mkf_char_t *  jis ,
	mkf_char_t *  nec_ext
	)
{
	return  mkf_map_via_ucs( jis , nec_ext , JISX0212_1990) ;
}

int
mkf_map_jisx0208_nec_ext_to_jisx0213_2000(
	mkf_char_t *  jis ,
	mkf_char_t *  nec_ext
	)
{
	mkf_char_t  ucs4 ;

	if( ! mkf_map_to_ucs4( &ucs4 , nec_ext))
	{
		return  0 ;
	}

	if( ! mkf_map_ucs4_to_cs( jis , &ucs4 , JISX0213_2000_2) &&
		! mkf_map_ucs4_to_cs( jis , &ucs4 , JISX0213_2000_1))
	{
		return  0 ;
	}

	return  1 ;
}

int
mkf_map_jisx0212_1990_to_jisx0208_nec_ext(
	mkf_char_t *  nec_ext ,
	mkf_char_t *  jis
	)
{
	return  mkf_map_via_ucs( nec_ext , jis , JISC6226_1978_NEC_EXT) ;
}

int
mkf_map_jisx0213_2000_2_to_jisx0208_nec_ext(
	mkf_char_t *  nec_ext ,
	mkf_char_t *  jis
	)
{
	return  mkf_map_via_ucs( nec_ext , jis , JISC6226_1978_NEC_EXT) ;
}


/* NEC IBM EXT */

int
mkf_map_jisx0208_necibm_ext_to_jisx0208_1983(
	mkf_char_t *  jis ,
	mkf_char_t *  necibm
	)
{
	return  mkf_map_via_ucs( jis , necibm , JISX0208_1983) ;
}

int
mkf_map_jisx0208_necibm_ext_to_jisx0212_1990(
	mkf_char_t *  jis ,
	mkf_char_t *  necibm
	)
{
	return  mkf_map_via_ucs( jis , necibm , JISX0212_1990) ;
}

int
mkf_map_jisx0208_necibm_ext_to_jisx0213_2000(
	mkf_char_t *  jis ,
	mkf_char_t *  necibm
	)
{
	mkf_char_t  ucs4 ;

	if( ! mkf_map_to_ucs4( &ucs4 , necibm))
	{
		return  0 ;
	}

	if( ! mkf_map_ucs4_to_cs( jis , &ucs4 , JISX0213_2000_2) &&
		! mkf_map_ucs4_to_cs( jis , &ucs4 , JISX0213_2000_1))
	{
		return  0 ;
	}

	return  1 ;
}

int
mkf_map_jisx0212_1990_to_jisx0208_necibm_ext(
	mkf_char_t *  necibm ,
	mkf_char_t *  jis
	)
{
	return  mkf_map_via_ucs( necibm , jis , JISC6226_1978_NECIBM_EXT) ;
}

int
mkf_map_jisx0213_2000_2_to_jisx0208_necibm_ext(
	mkf_char_t *  necibm ,
	mkf_char_t *  jis
	)
{
	return  mkf_map_via_ucs( necibm , jis , JISC6226_1978_NECIBM_EXT) ;
}


/* MAC EXT */

int
mkf_map_jisx0208_mac_ext_to_jisx0208_1983(
	mkf_char_t *  jis ,
	mkf_char_t *  mac
	)
{
	return  mkf_map_via_ucs( jis , mac , JISX0208_1983) ;
}

int
mkf_map_jisx0208_mac_ext_to_jisx0212_1990(
	mkf_char_t *  jis ,
	mkf_char_t *  mac
	)
{
	return  mkf_map_via_ucs( jis , mac , JISX0212_1990) ;
}

int
mkf_map_jisx0208_mac_ext_to_jisx0213_2000(
	mkf_char_t *  jis ,
	mkf_char_t *  mac
	)
{
	mkf_char_t  ucs4 ;

	if( ! mkf_map_to_ucs4( &ucs4 , mac))
	{
		return  0 ;
	}

	if( ! mkf_map_ucs4_to_cs( jis , &ucs4 , JISX0213_2000_2) &&
		! mkf_map_ucs4_to_cs( jis , &ucs4 , JISX0213_2000_1))
	{
		return  0 ;
	}

	return  1 ;
}

int
mkf_map_jisx0212_1990_to_jisx0208_mac_ext(
	mkf_char_t *  mac ,
	mkf_char_t *  jis
	)
{
	return  mkf_map_via_ucs( mac , jis , JISX0208_1983_MAC_EXT) ;
}

int
mkf_map_jisx0213_2000_2_to_jisx0208_mac_ext(
	mkf_char_t *  mac ,
	mkf_char_t *  jis
	)
{
	return  mkf_map_via_ucs( mac , jis , JISX0208_1983_MAC_EXT) ;
}
