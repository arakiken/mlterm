/*
 *	update: <2001/11/26(02:41:54)>
 *	$Id$
 */
 
#include  "mkf_euckr_parser.h"

#include  <stdio.h>		/* NULL */
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_parser.h"


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static int
uhc_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	if( parser->is_eos)
	{
		return  0 ;
	}
	
	mkf_parser_mark( parser) ;

	if( /* 0x00 <= *parser->str && */ *parser->str <= 0x80)
	{
		ch->ch[0] = *parser->str ;
		ch->cs = US_ASCII ;
		ch->size = 1 ;
	}
	else
	{
		ch->ch[0] = *parser->str ;

		if( mkf_parser_increment( parser) == 0)
		{
			mkf_parser_reset( parser) ;

			return  0 ;
		}

		ch->ch[1] = *parser->str ;
		ch->size = 2 ;
		ch->cs = UHC ;

	}

	ch->property = 0 ;
		
	mkf_parser_increment( parser) ;

	return  1 ;
}

static void
euckr_parser_init_intern(
	mkf_parser_t *  parser ,
	mkf_charset_t  g1_cs
	)
{
	mkf_iso2022_parser_t *  iso2022_parser ;

	mkf_parser_init( parser) ;

	iso2022_parser = (mkf_iso2022_parser_t*) parser ;

	iso2022_parser->g0 = US_ASCII ;
	iso2022_parser->g1 = g1_cs ;
	iso2022_parser->g2 = UNKNOWN_CS ;
	iso2022_parser->g3 = UNKNOWN_CS ;

	iso2022_parser->gl = &iso2022_parser->g0 ;
	iso2022_parser->gr = &iso2022_parser->g1 ;

	iso2022_parser->is_single_shifted = 0 ;
}

static void
euckr_parser_init(
	mkf_parser_t *  parser
	)
{
	euckr_parser_init_intern( parser , KSC5601_1987) ;
}

static void
uhc_parser_init(
	mkf_parser_t *  parser
	)
{
	euckr_parser_init_intern( parser , UHC) ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_euckr_parser_new(void)
{
	mkf_iso2022_parser_t *  iso2022_parser ;

	if( ( iso2022_parser = mkf_iso2022_parser_new()) == NULL)
	{
		return  NULL ;
	}

	euckr_parser_init( (mkf_parser_t*) iso2022_parser) ;

	/* override */
	iso2022_parser->parser.init = euckr_parser_init ;

	return  (mkf_parser_t*) iso2022_parser ;
}

mkf_parser_t *
mkf_uhc_parser_new(void)
{
	mkf_iso2022_parser_t *  iso2022_parser ;

	if( ( iso2022_parser = mkf_iso2022_parser_new()) == NULL)
	{
		return  NULL ;
	}

	uhc_parser_init( (mkf_parser_t*) iso2022_parser) ;

	/* override */
	iso2022_parser->parser.init = uhc_parser_init ;
	iso2022_parser->parser.next_char = uhc_parser_next_char ;

	return  (mkf_parser_t*) iso2022_parser ;
}
