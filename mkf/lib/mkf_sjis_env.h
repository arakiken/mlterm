/*
 *	$Id$
 */

#ifndef  __MKF_SJIS_ENV_H__
#define  __MKF_SJIS_ENV_H__


typedef enum  mkf_sjis_type_t
{
	APPLE_CS ,
	MICROSOFT_CS ,
	MS_CS_WITH_NECDOS_9_12	/* chars(9 - 12 section) not included in MS charset. */

} mkf_sjis_type_t ;


void mkf_set_sjis_input_type( mkf_sjis_type_t  type) ;

void mkf_set_sjis_output_type( mkf_sjis_type_t  type) ;

mkf_sjis_type_t  mkf_get_sjis_input_type(void) ;

mkf_sjis_type_t  mkf_get_sjis_output_type(void) ;


#endif
