/*
 *	update: <2001/6/28(03:05:25)>
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

	int  is_single_shifted ;
	
} mkf_iso2022_parser_t ;


mkf_iso2022_parser_t *  mkf_iso2022_parser_new(void) ;

void  mkf_iso2022_parser_set_str( mkf_parser_t *  parser , u_char *  str , size_t  size) ;

void  mkf_iso2022_parser_delete( mkf_parser_t *  parser) ;

int  mkf_iso2022_parser_next_char( mkf_parser_t *  parser , mkf_char_t *  ch) ;


#endif
