/*
 *	$Id$
 */

#ifndef  __MKF_ISO2022_PARSER_H__
#define  __MKF_ISO2022_PARSER_H__


#include  <kiklib/kik_types.h>	/* size_t */

#include  "mkf_parser.h"


typedef struct  mkf_iso2022_parser
{
	mkf_parser_t  parser ;
	
	mkf_charset_t *  gl ;
	mkf_charset_t *  gr ;
	
	mkf_charset_t  g0 ;
	mkf_charset_t  g1 ;
	mkf_charset_t  g2 ;
	mkf_charset_t  g3 ;

	mkf_charset_t  non_iso2022_cs ;

	int  is_single_shifted ;

	int  (*non_iso2022_is_started)( struct mkf_iso2022_parser *) ;
	int  (*next_non_iso2022_byte)( struct mkf_iso2022_parser * , mkf_char_t *) ;
	
} mkf_iso2022_parser_t ;


mkf_iso2022_parser_t *  mkf_iso2022_parser_new(void) ;

int  mkf_iso2022_parser_init_func( mkf_iso2022_parser_t *  iso2022_parser) ;
	
void  mkf_iso2022_parser_set_str( mkf_parser_t *  parser , u_char *  str , size_t  size) ;

void  mkf_iso2022_parser_delete( mkf_parser_t *  parser) ;

int  mkf_iso2022_parser_next_char( mkf_parser_t *  parser , mkf_char_t *  ch) ;


#endif
