/*
 *	update: <2001/11/26(02:43:13)>
 *	$Id$
 */

#include  "mkf_iso2022kr_parser.h"

#include  <stdio.h>		/* NULL */
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_parser.h"


/* --- static functions --- */

static void
iso2022kr_parser_init(
	mkf_parser_t *  parser
	)
{
	mkf_iso2022_parser_t *  iso2022_parser ;

	mkf_parser_init( parser) ;

	iso2022_parser = (mkf_iso2022_parser_t*) parser ;

	iso2022_parser->g0 = US_ASCII ;
#if  0
	iso2022_parser->g1 = KSC5601_1987 ;
#else
	iso2022_parser->g1 = UNKNOWN_CS ;
#endif
	iso2022_parser->g2 = UNKNOWN_CS ;
	iso2022_parser->g3 = UNKNOWN_CS ;

	iso2022_parser->gl = &iso2022_parser->g0 ;
	iso2022_parser->gr = NULL ;
	
	iso2022_parser->is_single_shifted = 0 ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_iso2022kr_parser_new(void)
{
	mkf_iso2022_parser_t *  iso2022_parser ;

	if( ( iso2022_parser = mkf_iso2022_parser_new()) == NULL)
	{
		return  NULL ;
	}

	iso2022kr_parser_init( ( mkf_parser_t*) iso2022_parser) ;

	/* override */
	iso2022_parser->parser.init = iso2022kr_parser_init ;

	return  (mkf_parser_t*) iso2022_parser ;
}
