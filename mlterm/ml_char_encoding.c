/*
 *	$Id$
 */

#include  "ml_char_encoding.h"

#include  <kiklib/kik_str.h>		/* kik_str_alloca_dup */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>		/* alloca */
#include  <kiklib/kik_locale.h>		/* kik_get_codeset */

#include  <mkf/mkf_iso8859_parser.h>
#include  <mkf/mkf_viscii_parser.h>
#include  <mkf/mkf_iscii_parser.h>
#include  <mkf/mkf_8bit_parser.h>
#include  <mkf/mkf_eucjp_parser.h>
#include  <mkf/mkf_euckr_parser.h>
#include  <mkf/mkf_euccn_parser.h>
#include  <mkf/mkf_euctw_parser.h>
#include  <mkf/mkf_iso2022jp_parser.h>
#include  <mkf/mkf_iso2022kr_parser.h>
#include  <mkf/mkf_iso2022cn_parser.h>
#include  <mkf/mkf_sjis_parser.h>
#include  <mkf/mkf_johab_parser.h>
#include  <mkf/mkf_big5_parser.h>
#include  <mkf/mkf_hz_parser.h>
#include  <mkf/mkf_utf8_parser.h>

#include  <mkf/mkf_iso8859_conv.h>
#include  <mkf/mkf_viscii_conv.h>
#include  <mkf/mkf_iscii_conv.h>
#include  <mkf/mkf_8bit_conv.h>
#include  <mkf/mkf_eucjp_conv.h>
#include  <mkf/mkf_euckr_conv.h>
#include  <mkf/mkf_euccn_conv.h>
#include  <mkf/mkf_euctw_conv.h>
#include  <mkf/mkf_iso2022jp_conv.h>
#include  <mkf/mkf_iso2022kr_conv.h>
#include  <mkf/mkf_iso2022cn_conv.h>
#include  <mkf/mkf_sjis_conv.h>
#include  <mkf/mkf_johab_conv.h>
#include  <mkf/mkf_big5_conv.h>
#include  <mkf/mkf_hz_conv.h>
#include  <mkf/mkf_utf8_conv.h>

#include  <mkf/mkf_iso2022_conv.h>	/* mkf_iso2022_illegal_char */

#include  <mkf/mkf_ucs4_map.h>


typedef struct  encoding_table
{
	ml_char_encoding_t  encoding ;
	char *  name ;
	mkf_parser_t *  (*parser_new)( void) ;
	mkf_conv_t *  (*conv_new)( void) ;

} encoding_table_t ;


/* --- static variables --- */

/*
 * !!! Notice !!!
 * The order should be the same as ml_char_encoding_t in ml_char_encoding.h
 * If the order is changed, x_font_manager.c:usascii_font_cs_table should be
 * also changed.
 */
static encoding_table_t  encoding_table[] =
{
	{ ML_ISO8859_1 , "ISO88591" , mkf_iso8859_1_parser_new , mkf_iso8859_1_conv_new , } ,
	{ ML_ISO8859_2 , "ISO88592" , mkf_iso8859_2_parser_new , mkf_iso8859_2_conv_new , } ,
	{ ML_ISO8859_3 , "ISO88593" , mkf_iso8859_3_parser_new , mkf_iso8859_3_conv_new , } ,
	{ ML_ISO8859_4 , "ISO88594" , mkf_iso8859_4_parser_new , mkf_iso8859_4_conv_new , } ,
	{ ML_ISO8859_5 , "ISO88595" , mkf_iso8859_5_parser_new , mkf_iso8859_5_conv_new , } ,
	{ ML_ISO8859_6 , "ISO88596" , mkf_iso8859_6_parser_new , mkf_iso8859_6_conv_new , } ,
	{ ML_ISO8859_7 , "ISO88597" , mkf_iso8859_7_parser_new , mkf_iso8859_7_conv_new , } ,
	{ ML_ISO8859_8 , "ISO88598" , mkf_iso8859_8_parser_new , mkf_iso8859_8_conv_new , } ,
	{ ML_ISO8859_9 , "ISO88599" , mkf_iso8859_9_parser_new , mkf_iso8859_9_conv_new , } ,
	{ ML_ISO8859_10 , "ISO885910" , mkf_iso8859_10_parser_new , mkf_iso8859_10_conv_new , } ,
	{ ML_TIS620 , "ISO885911" , mkf_tis620_2533_parser_new , mkf_tis620_2533_conv_new , } ,
	{ ML_ISO8859_13 , "ISO885913" , mkf_iso8859_13_parser_new , mkf_iso8859_13_conv_new , } ,
	{ ML_ISO8859_14 , "ISO885914" , mkf_iso8859_14_parser_new , mkf_iso8859_14_conv_new , } ,
	{ ML_ISO8859_15 , "ISO885915" , mkf_iso8859_15_parser_new , mkf_iso8859_15_conv_new , } ,
	{ ML_ISO8859_16 , "ISO885916" , mkf_iso8859_16_parser_new , mkf_iso8859_16_conv_new , } ,
	{ ML_TCVN5712 , "TCVN5712" , mkf_tcvn5712_3_1993_parser_new , mkf_tcvn5712_3_1993_conv_new , } ,

	{ ML_ISCII , "ISCII" , mkf_iscii_parser_new , mkf_iscii_conv_new , } ,	
	{ ML_VISCII , "VISCII" , mkf_viscii_parser_new , mkf_viscii_conv_new , } ,
	{ ML_KOI8_R , "KOI8R" , mkf_koi8_r_parser_new , mkf_koi8_r_conv_new , } ,
	{ ML_KOI8_U , "KOI8U" , mkf_koi8_u_parser_new , mkf_koi8_u_conv_new , } ,
	{ ML_KOI8_T , "KOI8T" , mkf_koi8_t_parser_new , mkf_koi8_t_conv_new , } ,
	{ ML_GEORGIAN_PS , "GEORGIANPS" , mkf_georgian_ps_parser_new , mkf_georgian_ps_conv_new , } ,
	{ ML_CP1250 , "CP1250" , mkf_cp1250_parser_new , mkf_cp1250_conv_new , } ,
	{ ML_CP1251 , "CP1251" , mkf_cp1251_parser_new , mkf_cp1251_conv_new , } ,
	{ ML_CP1252 , "CP1252" , mkf_cp1252_parser_new , mkf_cp1252_conv_new , } ,
	{ ML_CP1253 , "CP1253" , mkf_cp1253_parser_new , mkf_cp1253_conv_new , } ,
	{ ML_CP1254 , "CP1254" , mkf_cp1254_parser_new , mkf_cp1254_conv_new , } ,
	{ ML_CP1255 , "CP1255" , mkf_cp1255_parser_new , mkf_cp1255_conv_new , } ,
	{ ML_CP1256 , "CP1256" , mkf_cp1256_parser_new , mkf_cp1256_conv_new , } ,
	{ ML_CP1257 , "CP1257" , mkf_cp1257_parser_new , mkf_cp1257_conv_new , } ,
	{ ML_CP1258 , "CP1258" , mkf_cp1258_parser_new , mkf_cp1258_conv_new , } ,

	{ ML_UTF8 , "UTF8" , mkf_utf8_parser_new , mkf_utf8_conv_new , } ,
	
	{ ML_EUCJP , "EUCJP" , mkf_eucjp_parser_new , mkf_eucjp_conv_new , } ,
	{ ML_EUCJISX0213 , "EUCJISX0213" , mkf_eucjisx0213_parser_new , mkf_eucjisx0213_conv_new , } ,
	{ ML_ISO2022JP , "ISO2022JP" , mkf_iso2022jp_7_parser_new , mkf_iso2022jp_7_conv_new , } ,
	{ ML_ISO2022JP2 , "ISO2022JP2" , mkf_iso2022jp2_parser_new , mkf_iso2022jp2_conv_new , } ,
	{ ML_ISO2022JP3 , "ISO2022JP3" , mkf_iso2022jp3_parser_new , mkf_iso2022jp3_conv_new , } ,
	{ ML_SJIS , "SJIS" , mkf_sjis_parser_new , mkf_sjis_conv_new , } ,
	{ ML_SJISX0213 , "SJISX0213" , mkf_sjisx0213_parser_new , mkf_sjisx0213_conv_new , } ,

	{ ML_EUCKR , "EUCKR"  , mkf_euckr_parser_new , mkf_euckr_conv_new , } ,
	{ ML_UHC , "UHC" , mkf_uhc_parser_new , mkf_uhc_conv_new , } ,
	{ ML_JOHAB , "JOHAB" , mkf_johab_parser_new , mkf_johab_conv_new , } ,
	{ ML_ISO2022KR , "ISO2022KR" , mkf_iso2022kr_parser_new , mkf_iso2022kr_conv_new , } ,
	
	{ ML_BIG5 , "BIG5" , mkf_big5_parser_new , mkf_big5_conv_new , } ,
	{ ML_EUCTW , "EUCTW" , mkf_euctw_parser_new , mkf_euctw_conv_new , } ,

	{ ML_BIG5HKSCS , "BIG5HKSCS" , mkf_big5hkscs_parser_new , mkf_big5hkscs_conv_new , } ,
	
	/* not listed in IANA. GB2312 is usually used instead. */
	{ ML_EUCCN , "EUCCN" , mkf_euccn_parser_new , mkf_euccn_conv_new , } ,
	{ ML_GBK , "GBK" , mkf_gbk_parser_new , mkf_gbk_conv_new , } ,
	{ ML_GB18030 , "GB18030" , mkf_gb18030_2000_parser_new , mkf_gb18030_2000_conv_new , } ,
	{ ML_HZ , "HZ" , mkf_hz_parser_new , mkf_hz_conv_new , } ,

	{ ML_ISO2022CN , "ISO2022CN" , mkf_iso2022cn_parser_new , mkf_iso2022cn_conv_new , } ,
	
	/*
	 * alternative names.
	 * these are not used in ml_{parser|conv}_new , so parser_new/parser_conv members are
	 * not necessary.
	 */

	{ ML_TIS620 , "TIS620" , } ,
	
#if  0
	/* XXX necessary ? */
	{ ML_EUCJP , "EXTENDEDUNIXCODEPACKEDFORMATFORJAPANESE" , } , /* MIME */
	{ ML_EUCJP , "CSEUCPKDFMTJAPANESE" , } ,	/* MIME */
#endif
	{ ML_EUCJP , "UJIS" } ,
	{ ML_SJIS , "SHIFTJIS" , } ,	/* MIME */
	
	{ ML_EUCKR , "KSC56011987" , } ,	/* for IIS error page(IIS bug?) */
	
	{ ML_EUCCN , "GB2312" , } ,
	
	{ ML_HZ , "HZGB2312" , } ,
} ;

/*
 * MSB of these charsets are not set , but must be set manually for X font.
 */
static mkf_charset_t  msb_set_cs_table[] =
{
	JISX0201_KATA ,
	ISO8859_1_R ,
	ISO8859_2_R ,
	ISO8859_3_R ,
	ISO8859_4_R ,
	ISO8859_5_R ,
	ISO8859_6_R ,
	ISO8859_7_R ,
	ISO8859_8_R ,
	ISO8859_9_R ,
	ISO8859_10_R ,
	TIS620_2533 ,
	ISO8859_13_R ,
	ISO8859_14_R ,
	ISO8859_15_R ,
	ISO8859_16_R ,
	TCVN5712_3_1993 ,

} ;

static void (*iso2022kr_conv_init)( mkf_conv_t *) ;
static void (*iso2022kr_parser_init)( mkf_parser_t *) ;

static int  use_cp932_ucs_for_xft = 0 ;


/* --- static functions --- */

static void
ovrd_iso2022kr_conv_init(
	mkf_conv_t *  conv
	)
{
	u_char  buf[5] ;
	mkf_parser_t *  parser ;
	
	(*iso2022kr_conv_init)( conv) ;

	if( ( parser = mkf_iso2022kr_parser_new()) == NULL)
	{
		return ;
	}
	
	/* designating KSC5601 to G1 */
	(*parser->set_str)( parser , "\x1b$)Ca" , 5) ;
	
	/* this returns sequence of designating KSC5601 to G1 */
	(*conv->convert)( conv , buf , sizeof(buf) , parser) ;

	(*parser->delete)( parser) ;
}

static void
ovrd_iso2022kr_parser_init(
	mkf_parser_t *  parser
	)
{
	u_char  buf[5] ;
	mkf_conv_t *  conv ;
	
	(*iso2022kr_parser_init)( parser) ;

	if( ( conv = mkf_iso2022kr_conv_new()) == NULL)
	{
		return ;
	}
	
	/* designating KSC5601 to G1 */
	(*parser->set_str)( parser , "\x1b$)Ca" , 5) ;
	
	/* this returns sequence of designating KSC5601 to G1 */
	(*conv->convert)( conv , buf , sizeof(buf) , parser) ;

	(*conv->delete)( conv) ;
}


/* --- global functions --- */

char *
ml_get_char_encoding_name(
	ml_char_encoding_t  encoding
	)
{
	if( encoding < 0 || MAX_CHAR_ENCODINGS <= encoding)
	{
		return  "ISO88591" ;
	}
	else
	{
		return  encoding_table[encoding].name ;
	}
}

ml_char_encoding_t
ml_get_char_encoding(
	char *  name		/* '_' and '-' are ignored. */
	)
{
	int  count ;
	char *  _name ;
	char *  encoding ;
	char *  p ;

	/*
	 * duplicating name so as not to destroy its memory.
	 */
	if( ( _name = kik_str_alloca_dup( name)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif

		return  1 ;
	}

	if( ( encoding = alloca( strlen( name) + 1)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif

		return  1 ;
	}
	encoding[0] = '\0' ;

	/*
	 * removing '-' and '_' from name.
	 */
	while( ( p = kik_str_sep( &_name , "-_")) != NULL)
	{
		strcat( encoding , p) ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " encoding -> %s.\n" , encoding) ;
#endif

	if( strcasecmp( encoding , "auto") == 0)
	{
		return  ml_get_char_encoding( kik_get_codeset()) ;
	}
	
	for( count = 0 ; count < sizeof( encoding_table) / sizeof( encoding_table_t) ;
		count ++)
	{
		if( strcasecmp( encoding , encoding_table[count].name) == 0)
		{
			return  encoding_table[count].encoding ;
		}
	}

	return  ML_UNKNOWN_ENCODING ;
}

mkf_parser_t *
ml_parser_new(
	ml_char_encoding_t  encoding
	)
{
	mkf_parser_t *  parser ;
	
	if( encoding < 0 || MAX_CHAR_ENCODINGS <= encoding ||
		encoding_table[encoding].encoding != encoding)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %d is illegal encoding.\n" , encoding) ;
	#endif
	
		return  NULL ;
	}

	if( ( parser = (*encoding_table[encoding].parser_new)()) == NULL)
	{
		return  NULL ;
	}

	if( encoding == ML_ISO2022KR)
	{
		/* overriding init method */
		
		iso2022kr_parser_init = parser->init ;
		parser->init = ovrd_iso2022kr_parser_init ;

		(*parser->init)( parser) ;
	}

	return  parser ;
}

mkf_conv_t *
ml_conv_new(
	ml_char_encoding_t  encoding
	)
{
	mkf_conv_t *  conv ;
	
	if( encoding < 0 || MAX_CHAR_ENCODINGS <= encoding ||
		encoding_table[encoding].encoding != encoding)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %d is illegal encoding.\n" , encoding) ;
	#endif
	
		return  NULL ;
	}

	if( ( conv = (*encoding_table[encoding].conv_new)()) == NULL)
	{
		return  NULL ;
	}

	if( IS_ENCODING_BASED_ON_ISO2022(encoding))
	{
		conv->illegal_char = mkf_iso2022_illegal_char ;

		if( encoding == ML_ISO2022KR)
		{
			/* overriding init method */

			iso2022kr_conv_init = conv->init ;
			conv->init = ovrd_iso2022kr_conv_init ;

			(*conv->init)( conv) ;
		}
	}
	
	return  conv ;
}

int
ml_is_msb_set(
	mkf_charset_t  cs
	)
{
	int  count ;

	for( count = 0 ; count < sizeof( msb_set_cs_table) / sizeof( msb_set_cs_table[0]) ; count ++)
	{
		if( msb_set_cs_table[count] == cs)
		{
			return  1 ;
		}
	}

	return  0 ;
}

/*
 * used for general ucs4 conversion.
 * (not used for now)
 */
int
ml_convert_to_ucs4(
	u_char *  ucs4_bytes ,
	u_char *  src_bytes ,
	size_t  src_size ,
	mkf_charset_t  cs
	)
{
	if( cs == ISO10646_UCS4_1)
	{
		memcpy( ucs4_bytes , src_bytes , 4) ;
	}
	else
	{
		mkf_char_t  non_ucs ;
		mkf_char_t  ucs4 ;

		if( ml_is_msb_set( cs))
		{
			int  count ;

			for( count = 0 ; count < src_size ; count ++)
			{
				non_ucs.ch[count] = src_bytes[count] & 0x7f ;
			}
		}
		else
		{
			memcpy( non_ucs.ch , src_bytes , src_size) ;
		}
		
		non_ucs.size = src_size ;
		non_ucs.property = 0 ;
		non_ucs.cs = cs ;

		if( mkf_map_to_ucs4( &ucs4 , &non_ucs))
		{
			memcpy( ucs4_bytes , ucs4.ch , 4) ;
		}
		else
		{
			return  0 ;
		}
	}

	return  1 ;
}

int
ml_use_cp932_ucs_for_xft(void)
{
	use_cp932_ucs_for_xft = 1 ;

	return  1 ;
}

/*
 * used only for xft
 */
int
ml_convert_to_xft_ucs4(
	u_char *  ucs4_bytes ,
	u_char *  src_bytes ,
	size_t  src_size ,
	mkf_charset_t  cs	/* US_ASCII and ISO8859_1_R is not accepted */
	)
{
	if( cs == US_ASCII || cs == ISO8859_1_R)
	{
		return  0 ;
	}
	else if( use_cp932_ucs_for_xft && cs == JISX0208_1983)
	{
		u_int16_t  code ;

		code = mkf_bytes_to_int( src_bytes , src_size) ;

		if( code == 0x2140)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0x3c ;
		}
		else if( code == 0x2141)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0x5e ;
		}
		else if( code == 0x2142)
		{
			ucs4_bytes[2] = 0x22 ;
			ucs4_bytes[3] = 0x25 ;
		}
		else if( code == 0x215d)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0x0d ;
		}
		else if( code == 0x2171)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0xe0 ;
		}
		else if( code == 0x2172)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0xe1 ;
		}
		else if( code == 0x224c)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0xe2 ;
		}
		else
		{
			goto  no_diff ;
		}
		
		ucs4_bytes[0] = 0x0 ;
		ucs4_bytes[1] = 0x0 ;
		
		return  1 ;
	}

no_diff:
	return  ml_convert_to_ucs4( ucs4_bytes , src_bytes , src_size , cs) ;
}
