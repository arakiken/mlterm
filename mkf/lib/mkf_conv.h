/*
 *	$Id$
 */

#ifndef  __MKF_CONV_H__
#define  __MKF_CONV_H__


#include  "mkf_parser.h"


typedef struct  mkf_conv
{
	void (*init)( struct mkf_conv *) ;
	void (*delete)( struct mkf_conv *) ;
	size_t (*convert)( struct mkf_conv * , u_char * , size_t , mkf_parser_t *) ;
	size_t (*illegal_char)( struct mkf_conv * , u_char * , size_t , int * , mkf_char_t *) ;

} mkf_conv_t ;


#endif
