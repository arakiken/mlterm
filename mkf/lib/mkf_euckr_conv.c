/*
 *	$Id$
 */

#include  "mkf_euckr_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_conv.h"
#include  "mkf_iso2022_intern.h"
#include  "mkf_ucs4_map.h"
#include  "mkf_ko_kr_map.h"


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch ,
	int  is_uhc
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( mkf_map_ucs4_to_ko_kr( &c , ch))
		{
			*ch = c ;
		}
	}
	
	if( is_uhc)
	{
		if( ch->cs == ISO10646_UCS4_1)
		{
			return ;
		}

		if( ch->cs == JOHAB)
		{
			if( ! mkf_map_johab_to_uhc( &c , ch))
			{
				return ;
			}

			*ch = c ;
		}

		if( mkf_map_ksc5601_1987_to_uhc( &c , ch))
		{
			*ch = c ;
		}
	}
	else
	{
		mkf_iso2022_remap_unsupported_charset( ch) ;
	}
}

static size_t
convert_to_euckr_intern(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser ,
	int   is_uhc
	)
{
	size_t  filled_size ;
	mkf_char_t  ch ;

	filled_size = 0 ;
	while( mkf_parser_next_char( parser , &ch))
	{
		remap_unsupported_charset( &ch , is_uhc) ;
		
		if( ch.cs == US_ASCII)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;
			
				return  filled_size ;
			}

			*(dst ++) = *ch.ch ;

			filled_size ++ ;
		}
		else if( (! is_uhc) && ch.cs == KSC5601_1987)
		{
			if( filled_size + 1 >= dst_size)
			{
				mkf_parser_reset( parser) ;
			
				return  filled_size ;
			}
			
			*(dst ++) = MAP_TO_GR( ch.ch[0]) ;
			*(dst ++) = MAP_TO_GR( ch.ch[1]) ;

			filled_size += 2 ;
		}
		else if( is_uhc && ch.cs == UHC)
		{
			if( filled_size + 1 >= dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = ch.ch[0] ;
			*(dst ++) = ch.ch[1] ;
			
			filled_size += 2 ;
		}
		else if( conv->illegal_char)
		{
			size_t  size ;
			int  is_full ;
			
			size = (*conv->illegal_char)( conv , dst , dst_size - filled_size , &is_full , &ch) ;
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
convert_to_euckr(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_euckr_intern( conv , dst , dst_size , parser , 0) ;
}

static size_t
convert_to_uhc(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_euckr_intern( conv , dst , dst_size , parser , 1) ;
}

static void
euckr_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = ( mkf_iso2022_conv_t*) conv ;

	iso2022_conv->gl = &iso2022_conv->g0 ;
	iso2022_conv->gr = &iso2022_conv->g1 ;
	iso2022_conv->g0 = US_ASCII ;
	iso2022_conv->g1 = KSC5601_1987 ;
	iso2022_conv->g2 = UNKNOWN_CS ;
	iso2022_conv->g3 = UNKNOWN_CS ;
}

static void
uhc_conv_init(
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


/* --- global functions --- */

mkf_conv_t *
mkf_euckr_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	euckr_conv_init( ( mkf_conv_t*) iso2022_conv) ;

	iso2022_conv->conv.convert = convert_to_euckr ;
	iso2022_conv->conv.init = euckr_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;

	return  (mkf_conv_t*) iso2022_conv ;
}

mkf_conv_t *
mkf_uhc_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_uhc ;
	conv->init = uhc_conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}
