/*
 *	$Id$
 */

#ifndef  __DICT_H__
#define  __DICT_H__


#include  <kiklib/kik_types.h>
#include  <mkf/mkf_conv.h>


#define  MAX_CAPTION_LEN  64


void  dict_final(void) ;

u_int  dict_completion( mkf_char_t *  caption , u_int  caption_len , void **  aux , int  step) ;

void  dict_completion_finish( void *  aux) ;

u_int  dict_completion_reset_and_finish( mkf_char_t *  caption , void *  aux) ;

char *  dict_search( mkf_char_t *  mkf_caption , u_int  caption_len ,
		mkf_conv_t *  conv) ;

int  dict_add( char *  caption , char *  word , mkf_parser_t *  parser) ;

void  dict_set_global( char *  dict) ;


#endif
