/*
 *	$Id$
 */

#include  "mkf_8bit_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ru_map.h"
#include  "mkf_ucs4_map.h"


#if  0
#define  __DEBUG
#endif


typedef struct  mkf_iscii_conv
{
	mkf_conv_t  conv ;
	mkf_charset_t  cs ;

} mkf_iscii_conv_t ;


/* --- static functions --- */

static int
map_direct(
	mkf_char_t *  dst ,
	mkf_char_t *  src ,
	mkf_charset_t  to_cs
	)
{
	if( src->cs == KOI8_U && to_cs == KOI8_R)
	{
		return  mkf_map_koi8_u_to_koi8_r( dst , src) ;
	}
	else if( src->cs == KOI8_R && to_cs == KOI8_U)
	{
		return  mkf_map_koi8_r_to_koi8_u( dst , src) ;
	}
	else if( src->cs == ISO10646_UCS4_1 &&
	         src->ch[0] == 0 && src->ch[1] == 0 && src->ch[2] == 0 && src->ch[3] <= 0x7f)
	{
		dst->cs = US_ASCII ;
		dst->size = 1 ;
		dst->property = 0 ;
		dst->ch[0] = src->ch[3] ;

		return  1 ;
	}

	return  0 ;
}

static void
remap_unsupported_charset(
	mkf_char_t *  ch ,
	mkf_charset_t  to_cs
	)
{
	mkf_char_t  c ;

	if( ch->cs == to_cs)
	{
		/* do nothing */
	}
	else if( map_direct( &c , ch , to_cs))
	{
		*ch = c ;
	}
	else if( mkf_map_via_ucs( &c , ch , to_cs))
	{
		*ch = c ;
	}

	if( to_cs == VISCII && ch->cs == US_ASCII)
	{
		if( ch->ch[0] == 0x02 || ch->ch[0] == 0x05 || ch->ch[0] == 0x06 ||
			ch->ch[0] == 0x14 || ch->ch[0] == 0x19 || ch->ch[0] == 0x1e)
		{
			ch->cs = VISCII ;
		}
	}
}

static size_t
convert_to_intern(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser ,
	mkf_charset_t  to_cs
	)
{
	size_t  filled_size ;
	mkf_char_t  ch ;

	filled_size = 0 ;
	while( mkf_parser_next_char( parser , &ch))
	{
		remap_unsupported_charset( &ch , to_cs) ;

		if( to_cs == ch.cs || ch.cs == US_ASCII)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = ch.ch[0] ;

			filled_size ++ ;
		}
		else if( conv->illegal_char)
		{
			size_t  size ;
			int  is_full ;
			
			size = (*conv->illegal_char)( conv , dst , dst_size - filled_size ,
					&is_full , &ch) ;
			if( is_full)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			dst += size ;
			filled_size += size ;
		}
	}

	return  filled_size ;
}

static size_t
convert_to_koi8_r(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , KOI8_R) ;
}

static size_t
convert_to_koi8_u(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , KOI8_U) ;
}

static size_t
convert_to_koi8_t(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , KOI8_T) ;
}

static size_t
convert_to_georgian_ps(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , GEORGIAN_PS) ;
}

static size_t
convert_to_cp1250(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , CP1250) ;
}

static size_t
convert_to_cp1251(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , CP1251) ;
}

static size_t
convert_to_cp1252(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , CP1252) ;
}

static size_t
convert_to_cp1253(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , CP1253) ;
}

static size_t
convert_to_cp1254(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , CP1254) ;
}

static size_t
convert_to_cp1255(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , CP1255) ;
}

static size_t
convert_to_cp1256(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , CP1256) ;
}

static size_t
convert_to_cp1257(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , CP1257) ;
}

static size_t
convert_to_cp1258(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , CP1258) ;
}

static size_t
convert_to_cp874(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , CP874) ;
}

static size_t
convert_to_viscii(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , VISCII) ;
}

static size_t
convert_to_iscii(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser ,
			((mkf_iscii_conv_t*)conv)->cs) ;
}

static void
conv_init(
	mkf_conv_t *  conv
	)
{
}

static void
conv_delete(
	mkf_conv_t *  conv
	)
{
	free( conv) ;
}

static mkf_conv_t *
iscii_conv_new(
	mkf_charset_t  cs
	)
{
	mkf_iscii_conv_t *  iscii_conv ;

	if( ( iscii_conv = malloc( sizeof( mkf_iscii_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iscii_conv->conv.convert = convert_to_iscii ;
	iscii_conv->conv.init = conv_init ;
	iscii_conv->conv.delete = conv_delete ;
	iscii_conv->conv.illegal_char = NULL ;
	iscii_conv->cs = cs ;

	return  &iscii_conv->conv ;
}


/* --- global functions --- */

mkf_conv_t *
mkf_koi8_r_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_koi8_r ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_koi8_u_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_koi8_u ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_koi8_t_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_koi8_t ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_georgian_ps_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_georgian_ps ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_cp1250_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_cp1250 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_cp1251_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_cp1251 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_cp1252_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_cp1252 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_cp1253_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_cp1253 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_cp1254_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_cp1254 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_cp1255_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_cp1255 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_cp1256_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_cp1256 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_cp1257_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_cp1257 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_cp1258_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_cp1258 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_cp874_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_cp874 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_viscii_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_viscii ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_iscii_assamese_conv_new(void)
{
	return  iscii_conv_new( ISCII_ASSAMESE) ;
}

mkf_conv_t *
mkf_iscii_bengali_conv_new(void)
{
	return  iscii_conv_new( ISCII_BENGALI) ;
}

mkf_conv_t *
mkf_iscii_gujarati_conv_new(void)
{
	return  iscii_conv_new( ISCII_GUJARATI) ;
}

mkf_conv_t *
mkf_iscii_hindi_conv_new(void)
{
	return  iscii_conv_new( ISCII_HINDI) ;
}

mkf_conv_t *
mkf_iscii_kannada_conv_new(void)
{
	return  iscii_conv_new( ISCII_KANNADA) ;
}

mkf_conv_t *
mkf_iscii_malayalam_conv_new(void)
{
	return  iscii_conv_new( ISCII_MALAYALAM) ;
}

mkf_conv_t *
mkf_iscii_oriya_conv_new(void)
{
	return  iscii_conv_new( ISCII_ORIYA) ;
}

mkf_conv_t *
mkf_iscii_punjabi_conv_new(void)
{
	return  iscii_conv_new( ISCII_PUNJABI) ;
}

mkf_conv_t *
mkf_iscii_roman_conv_new(void)
{
	return  iscii_conv_new( ISCII_ROMAN) ;
}

mkf_conv_t *
mkf_iscii_tamil_conv_new(void)
{
	return  iscii_conv_new( ISCII_TAMIL) ;
}

mkf_conv_t *
mkf_iscii_telugu_conv_new(void)
{
	return  iscii_conv_new( ISCII_TELUGU) ;
}
