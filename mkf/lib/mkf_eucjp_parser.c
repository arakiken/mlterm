/*
 *	update: <2001/11/26(02:41:44)>
 *	$Id$
 */
 
#include  "mkf_eucjp_parser.h"

#include  <stdio.h>		/* NULL */

#include  "mkf_iso2022_parser.h"


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static void
eucjp_parser_init(
	mkf_parser_t *  parser
	)
{
	mkf_iso2022_parser_t *  iso2022_parser ;

	mkf_parser_init( parser) ;

	iso2022_parser = (mkf_iso2022_parser_t*) parser ;

	iso2022_parser->g0 = US_ASCII ;
	iso2022_parser->g1 = JISX0208_1983 ;
	iso2022_parser->g2 = JISX0201_KATA ;
	iso2022_parser->g3 = JISX0212_1990 ;

	iso2022_parser->gl = &iso2022_parser->g0 ;
	iso2022_parser->gr = &iso2022_parser->g1 ;

	iso2022_parser->is_single_shifted = 0 ;
}

static void
eucjisx0213_parser_init(
	mkf_parser_t *  parser
	)
{
	mkf_iso2022_parser_t *  iso2022_parser ;

	mkf_parser_init( parser) ;

	iso2022_parser = (mkf_iso2022_parser_t*) parser ;
	
	iso2022_parser->g0 = US_ASCII ;
	iso2022_parser->g1 = JISX0213_2000_1 ;
	iso2022_parser->g2 = JISX0201_KATA ;
	iso2022_parser->g3 = JISX0213_2000_2 ;
	
	iso2022_parser->gl = &iso2022_parser->g0 ;
	iso2022_parser->gr = &iso2022_parser->g1 ;
	
	iso2022_parser->is_single_shifted = 0 ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_eucjp_parser_new(void)
{
	mkf_iso2022_parser_t *  iso2022_parser ;

	if( ( iso2022_parser = mkf_iso2022_parser_new()) == NULL)
	{
		return  NULL ;
	}

	/* override */
	iso2022_parser->parser.init = eucjp_parser_init ;

	eucjp_parser_init( (mkf_parser_t*)iso2022_parser) ;

	return  (mkf_parser_t*) iso2022_parser ;
}

mkf_parser_t *
mkf_eucjisx0213_parser_new(void)
{
	mkf_iso2022_parser_t *  iso2022_parser ;

	if( ( iso2022_parser = mkf_iso2022_parser_new()) == NULL)
	{
		return  NULL ;
	}

	eucjisx0213_parser_init( (mkf_parser_t*)iso2022_parser) ;

	/* overwrite */
	iso2022_parser->parser.init = eucjisx0213_parser_init ;

	return  (mkf_parser_t*) iso2022_parser ;
}
