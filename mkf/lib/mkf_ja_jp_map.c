/*
 *	$Id$
 */

#include  "mkf_ja_jp_map.h"

#include  "table/mkf_sjis_ext.table"
#include  "table/mkf_jisx0208_ext.table"

#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_usascii.h"
#include  "mkf_ucs4_jisx0201.h"
#include  "mkf_ucs4_jisx0208.h"
#include  "mkf_ucs4_jisx0212.h"
#include  "mkf_ucs4_jisx0213.h"


static mkf_map_ucs4_to_func_t  map_ucs4_to_funcs[] =
{
	mkf_map_ucs4_to_us_ascii ,
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
	 * JISX0213-1 is upper compatible with JISX0208 , some chars of JISX0213
	 * cannot be mapped to JISX0208.
	 */
	memcpy( jis83->ch , jis2k->ch , 2) ;
	jis83->size = 2 ;
	jis83->cs = JISX0213_2000_1 ;
	jis83->property = jis2k->property ;
	
	return  1 ;
}

int
mkf_map_jisx0213_2000_2_to_jisx0212_1990(
	mkf_char_t *  jis2k ,
	mkf_char_t *  jis90
	)
{
	return  0 ;
}

int
mkf_map_jisx0212_1990_to_jisx0213_2000_2(
	mkf_char_t *  jis90 ,
	mkf_char_t *  jis2k
	)
{
	return  0 ;
}

int
mkf_map_ibm_ext_to_jisx0208_1983(
	mkf_char_t *  jis ,
	mkf_char_t *  ibm
	)
{
	if( ibm->ch[0] == 0xfa)
	{
		if( ibm->ch[1] == 0x54)
		{
			memcpy( jis->ch , "\x22\x4c" , 2) ;
		}
		else if( ibm->ch[1] == 0x5b)
		{
			memcpy( jis->ch , "\x22\x68" , 2) ;
		}
		else if( ibm->ch[1] == 0xd0)
		{
			/*
			 * this character(SUBARU) glyph is the same as JISX0208_1983 0x5a65.
			 * but YFI , it is not the same glyph as JISC6226_1978 0x5a65.
			 */

			memcpy( jis->ch , "\x5a\x65" , 2) ;
		}
		else
		{
			return  0 ;
		}
	}
	else
	{
		return  0 ;
	}
	
	jis->size = 2 ;
	jis->cs = JISX0208_1983 ;

	return  1 ;
}

int
mkf_map_ibm_ext_to_jisx0212_1990(
	mkf_char_t *  jis ,
	mkf_char_t *  ibm
	)
{
	u_int16_t  c ;

	if( ibm->ch[0] == 0xfa)
	{
		if( 0x59 <= ibm->ch[1] && ibm->ch[1] <= 0xaf)
		{
			if( ( c = sjis_ibm_ext_to_jisx0212_1990_table1[ ibm->ch[1] - 0x59]) == 0x0)
			{
				return  0 ;
			}
		}
		else
		{
			return  0 ;
		}
	}
	else if( ibm->ch[0] == 0xfb)
	{
		if( 0x60 <= ibm->ch[1] && ibm->ch[1] <= 0xfb)
		{
			if( ( c = sjis_ibm_ext_to_jisx0212_1990_table2[ ibm->ch[1] - 0x60]) == 0x0)
			{
				return  0 ;
			}
		}
		else
		{
			return  0 ;
		}
	}
	else if( ibm->ch[0] == 0xfc)
	{
		if( 0x40 <= ibm->ch[1] && ibm->ch[1] <= 0x4a)
		{
			if( ( c = sjis_ibm_ext_to_jisx0212_1990_table3[ ibm->ch[1] - 0x40]) == 0x0)
			{
				return  0 ;
			}
		}
		else
		{
			return  0 ;
		}
	}
	else
	{
		return  0 ;
	}

	mkf_int_to_bytes( jis->ch , 2 , c) ;
	jis->size = 2 ;
	jis->cs = JISX0212_1990 ;
	
	return  1 ;
}

int
mkf_map_jisx0212_1990_to_ibm_ext(
	mkf_char_t *  ibm ,
	mkf_char_t *  jis
	)
{
	return  0 ;
}

int
mkf_map_nec_ext_to_jisx0208_1983(
	mkf_char_t *  jis ,
	mkf_char_t *  nec_ext
	)
{
	u_int16_t  c ;

	if( nec_ext->cs != JISC6226_1978_NEC_EXT
		|| nec_ext->ch[0] != 0x2d || nec_ext->ch[1] < 0x70 || nec_ext->ch[1] > 0x7c )
	{
		return  0 ;
	}

	if( ( c = nec_ext_to_jisx0208_1983_table[ nec_ext->ch[1] - 0x70]) == 0x0)
	{
		return  0 ;
	}

	mkf_int_to_bytes( jis->ch , 2 , c) ;
	jis->size = 2 ;
	jis->cs = JISX0208_1983 ;

	return  1 ;
}

int
mkf_map_nec_ext_to_jisx0212_1990(
	mkf_char_t *  jis ,
	mkf_char_t *  nec_ext
	)
{
	u_int16_t  c ;
	
	if( nec_ext->cs != JISC6226_1978_NEC_EXT || nec_ext->ch[0] != 0x2d)
	{
		return  0 ;
	}

	if( nec_ext->ch[1] == 0x62)
	{
		c = 0x2271 ;
	}
	else if( 0x70 <= nec_ext->ch[1] && nec_ext->ch[1] <= 0x7c)
	{
		if( ( c = nec_ext_to_jisx0212_1990_table[ nec_ext->ch[1] - 0x70]) == 0x0)
		{
			return  0 ;
		}
	}
	else
	{
		return  0 ;
	}

	mkf_int_to_bytes( jis->ch , 2 , c) ;
	jis->size = 2 ;
	jis->cs = JISX0212_1990 ;

	return  1 ;
}

int
mkf_map_jisx0212_1990_to_nec_ext(
	mkf_char_t *  nec_ext ,
	mkf_char_t *  jis
	)
{
	return  0 ;
}

int
mkf_map_necibm_ext_to_jisx0208_1983(
	mkf_char_t *  jis ,
	mkf_char_t *  ibm
	)
{
	if( ibm->cs != JISC6226_1978_NECIBM_EXT)
	{
		return  0 ;
	}

	if( ibm->ch[0] == 0x7c && ibm->ch[1] == 0x7b)
	{
		/* SJIS 0xeef9 (¢Ì) */
		
		memcpy( jis->ch , "\x22\x4c" , 2) ;
		jis->size = 2 ;
		jis->cs = JISX0208_1983 ;
		
		return  1 ;
	}

	return  0 ;
}

int
mkf_map_necibm_ext_to_jisx0212_1990(
	mkf_char_t *  jis ,
	mkf_char_t *  ibm
	)
{
	u_int16_t  c ;

	if( ibm->cs != JISC6226_1978_NECIBM_EXT)
	{
		return  0 ;
	}
	
	if( ibm->ch[ 0] == 0x79)
	{
		if( 0x21 <= ibm->ch[1] && ibm->ch[1] <= 0x7d)
		{
			if( ( c = necibm_ext_to_jisx0212_1990_table1[ ibm->ch[1] - 0x21]) == 0x0)
			{
				return  0 ;
			}
		}
		else
		{
			return  0 ;
		}
	}
	else if( ibm->ch[0] == 0x7a)
	{
		if( 0x21 <= ibm->ch[1] && ibm->ch[1] <= 0x7e)
		{
			if( ( c = necibm_ext_to_jisx0212_1990_table2[ ibm->ch[1] - 0x21]) == 0x0)
			{
				return  0 ;
			}
		}
		else
		{
			return  0 ;
		}
	}
	else if( ibm->ch[0] == 0x7b)
	{
		if( 0x21 <= ibm->ch[1] && ibm->ch[1] <= 0x7e)
		{
			if( ( c = necibm_ext_to_jisx0212_1990_table3[ ibm->ch[1] - 0x21]) == 0x0)
			{
				return  0 ;
			}
		}
		else
		{
			return  0 ;
		}
	}
	else if( ibm->ch[0] == 0x7c)
	{
		if( 0x21 <= ibm->ch[1] && ibm->ch[1] <= 0x6d)
		{
			if( ( c = necibm_ext_to_jisx0212_1990_table4[ ibm->ch[1] - 0x21]) == 0x0)
			{
				return  0 ;
			}
		}
		else
		{
			return  0 ;
		}
	}
	else
	{
		return  0 ;
	}

	mkf_int_to_bytes( jis->ch , 2 , c) ;
	jis->size = 2 ;
	jis->cs = JISX0212_1990 ;
	
	return  1 ;
}

int
mkf_map_jisx0212_1990_to_necibm_ext(
	mkf_char_t *  ibm ,
	mkf_char_t *  jis
	)
{
	return  0 ;
}


int
mkf_map_mac_ext_to_jisx0208_1983(
	mkf_char_t *  jis ,
	mkf_char_t *  mac
	)
{
	return  0 ;
}

int
mkf_map_mac_ext_to_jisx0212_1990(
	mkf_char_t *  jis ,
	mkf_char_t *  mac
	)
{
	return  0 ;
}

int
mkf_map_mac_ext_to_jisx0213_2000(
	mkf_char_t *  jis ,
	mkf_char_t *  mac
	)
{
	return  0 ;
}

int
mkf_map_jisx0212_1990_to_mac_ext(
	mkf_char_t *  mac ,
	mkf_char_t *  jis
	)
{
	return  0 ;
}
