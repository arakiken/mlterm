/*
 *	$Id$
 */

#ifndef  __ML_STR_H__
#define  __ML_STR_H__


#include  <kiklib/kik_mem.h>	/* alloca */

#include  "ml_char.h"


int  ml_str_init( ml_char_t *  str , u_int  size) ;

ml_char_t *  __ml_str_init( ml_char_t *  str , u_int  size) ;

#define  ml_str_alloca(size) __ml_str_init( alloca(sizeof(ml_char_t) * (size)) , (size) )

ml_char_t *  ml_str_new( u_int  size) ;

int  ml_str_final( ml_char_t *  str , u_int  size) ;

int  ml_str_delete( ml_char_t *  str , u_int  size) ;

int  ml_str_copy( ml_char_t *  dst , ml_char_t *  src , u_int  size) ; 

u_int  ml_str_cols( ml_char_t *  chars , u_int  len) ;

int  ml_str_equal( ml_char_t *  str1 , ml_char_t *  str2 , u_int  len) ;

int  ml_str_bytes_equal( ml_char_t *  str1 , ml_char_t *  str2 , u_int  len) ;

#ifdef  DEBUG

void  ml_str_dump( ml_char_t *  chars , u_int  len) ;

#endif


#endif
