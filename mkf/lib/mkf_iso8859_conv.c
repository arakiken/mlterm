/*
 *	$Id$
 */

#include  "mkf_iso8859_conv.h"

#include  <stdio.h>		/* NULL */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_conv.h"
#include  "mkf_iso2022_intern.h"
#include  "mkf_viet_map.h"
#include  "mkf_ru_map.h"
#include  "mkf_ucs4_iso8859.h"
#include  "mkf_ucs4_map.h"


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch ,
	mkf_charset_t  gr_cs
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( mkf_map_ucs4_to_cs( &c , ch , gr_cs))
		{
			*ch = c ;

			return ;
		}
	}

	mkf_iso2022_remap_unsupported_charset( ch) ;
}

static size_t
convert_to_iso8859(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;
	size_t  filled_size ;
	mkf_char_t  ch ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;
	
	filled_size = 0 ;
	while( mkf_parser_next_char( parser , &ch))
	{
		remap_unsupported_charset( &ch , iso2022_conv->g1) ;

		if( ch.cs == US_ASCII)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_full_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = ch.ch[0] ;
			filled_size ++ ;
		}
		else if( ch.cs == iso2022_conv->g1)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_full_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = SET_MSB(ch.ch[0]) ;
			filled_size ++ ;
		}
		else if( conv->illegal_char)
		{
			size_t  size ;
			int  is_full ;
			
			size = (*conv->illegal_char)( conv , dst , dst_size - filled_size , &is_full , &ch) ;
			if( is_full)
			{
				mkf_parser_full_reset( parser) ;

				return  filled_size ;
			}

			dst += size ;
			filled_size += size ;
		}
	}

	return  filled_size ;
}

static void
conv_init_intern(
	mkf_conv_t *  conv ,
	mkf_charset_t  g1
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;
	
	iso2022_conv->gl = &iso2022_conv->g0 ;
	iso2022_conv->gr = &iso2022_conv->g1 ;
	iso2022_conv->g0 = US_ASCII ;
	iso2022_conv->g1 = g1 ;
	iso2022_conv->g2 = UNKNOWN_CS ;
	iso2022_conv->g3 = UNKNOWN_CS ;
}

static void
conv_init_iso8859_1(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_1_R) ;
}

static void
conv_init_iso8859_2(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_2_R) ;
}

static void
conv_init_iso8859_3(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_3_R) ;
}

static void
conv_init_iso8859_4(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_4_R) ;
}

static void
conv_init_iso8859_5(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_5_R) ;
}

static void
conv_init_iso8859_6(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_6_R) ;
}

static void
conv_init_iso8859_7(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_7_R) ;
}

static void
conv_init_iso8859_8(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_8_R) ;
}

static void
conv_init_iso8859_9(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_9_R) ;
}

static void
conv_init_iso8859_10(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_10_R) ;
}

static void
conv_init_tis620_2533(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , TIS620_2533) ;
}

static void
conv_init_iso8859_13(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_13_R) ;
}

static void
conv_init_iso8859_14(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_14_R) ;
}

static void
conv_init_iso8859_15(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_15_R) ;
}

static void
conv_init_iso8859_16(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , ISO8859_16_R) ;
}

static void
conv_init_tcvn5712_3_1993(
	mkf_conv_t *  conv
	)
{
	conv_init_intern( conv , TCVN5712_3_1993) ;
}

static void
conv_delete(
	mkf_conv_t *  conv
	)
{
	free( conv) ;
}

static mkf_conv_t *
iso8859_conv_new(
	void (*init)( mkf_conv_t *)
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	(*init)( (mkf_conv_t*) iso2022_conv) ;

	iso2022_conv->conv.convert = convert_to_iso8859 ;
	iso2022_conv->conv.init = init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;

	return  (mkf_conv_t*) iso2022_conv ;
}


/* --- global functions --- */

mkf_conv_t *
mkf_iso8859_1_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_1) ;
}

mkf_conv_t *
mkf_iso8859_2_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_2) ;
}

mkf_conv_t *
mkf_iso8859_3_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_3) ;
}

mkf_conv_t *
mkf_iso8859_4_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_4) ;
}

mkf_conv_t *
mkf_iso8859_5_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_5) ;
}

mkf_conv_t *
mkf_iso8859_6_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_6) ;
}

mkf_conv_t *
mkf_iso8859_7_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_7) ;
}

mkf_conv_t *
mkf_iso8859_8_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_8) ;
}

mkf_conv_t *
mkf_iso8859_9_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_9) ;
}

mkf_conv_t *
mkf_iso8859_10_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_10) ;
}

mkf_conv_t *
mkf_tis620_2533_conv_new(void)
{
	return  iso8859_conv_new( conv_init_tis620_2533) ;
}

mkf_conv_t *
mkf_iso8859_13_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_13) ;
}

mkf_conv_t *
mkf_iso8859_14_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_14) ;
}

mkf_conv_t *
mkf_iso8859_15_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_15) ;
}

mkf_conv_t *
mkf_iso8859_16_conv_new(void)
{
	return  iso8859_conv_new( conv_init_iso8859_16) ;
}

mkf_conv_t *
mkf_tcvn5712_3_1993_conv_new(void)
{
	return  iso8859_conv_new( conv_init_tcvn5712_3_1993) ;
}
