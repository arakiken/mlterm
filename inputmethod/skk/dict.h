/*
 *	$Id$
 */

#ifndef  __DICT_H__
#define  __DICT_H__


#include  <kiklib/kik_types.h>
#include  <mkf/mkf_conv.h>


void  dict_final(void) ;

char *  dict_search( mkf_char_t *  mkf_caption , u_int  caption_len ,
		mkf_conv_t *  conv) ;

int  dict_add( char *  caption , char *  word , mkf_parser_t *  parser) ;

void  dict_set_global( char *  dict) ;


#endif
