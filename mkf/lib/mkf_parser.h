/*
 *	$Id$
 */

#ifndef  __MKF_PARSER_H__
#define  __MKF_PARSER_H__


#include  <kiklib/kik_types.h>	/* size_t */

#include  "mkf_char.h"


typedef struct  mkf_parser
{
	/* private */
	u_char *  str ;
	size_t  marked_left ;
	size_t  left ;

	/* public */
	int  is_eos ;

	/* public */
	void (*init)( struct mkf_parser *) ;
	void (*set_str)( struct mkf_parser * , u_char *  str , size_t  size) ;
	void (*delete)( struct mkf_parser *) ;
	int (*next_char)( struct mkf_parser * , mkf_char_t *) ;
	
} mkf_parser_t ;


#define  mkf_parser_increment( parser)  __mkf_parser_increment( ( mkf_parser_t*) parser)

#define  mkf_parser_n_increment( parser , n)  __mkf_parser_n_increment( ( mkf_parser_t*) parser , n)

#define  mkf_parser_reset( parser)  __mkf_parser_reset( ( mkf_parser_t*) parser)

#define  mkf_parser_full_reset( parser)  __mkf_parser_full_reset( ( mkf_parser_t*) parser)

#define  mkf_parser_mark( parser)  __mkf_parser_mark( ( mkf_parser_t*) parser)


void  mkf_parser_init( mkf_parser_t *  parser) ;

size_t  __mkf_parser_increment( mkf_parser_t *  parser) ;

size_t  __mkf_parser_n_increment( mkf_parser_t *  parser , size_t  n) ;

void  __mkf_parser_mark( mkf_parser_t *  parser) ;

void  __mkf_parser_reset( mkf_parser_t *  parser) ;

void  __mkf_parser_full_reset( mkf_parser_t *  parser) ;

int  mkf_parser_next_char( mkf_parser_t *  parser , mkf_char_t *  ch) ;


#endif
