/*
 *	$Id$
 */

#ifndef  __ML_STR_PARSER_H__
#define  __ML_STR_PARSER_H__


#include  <mkf/mkf_parser.h>
#include  <kiklib/kik_types.h>

#include  "ml_char.h"


mkf_parser_t *  ml_str_parser_new(void) ;

void  ml_str_parser_set_str( mkf_parser_t *  mkf_parser , ml_char_t *  str , u_int  size) ;


#endif
