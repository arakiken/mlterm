/*
 *	$Id$
 */

#include  "ml_iscii.h"

#include  <string.h>			/* memcpy */
#include  "ml_ctl_loader.h"


/* --- global functions --- */

#ifndef  NO_DYNAMIC_LOAD_CTL

#ifdef  __APPLE__
ml_isciikey_state_t  ml_isciikey_state_new( int) __attribute__((weak)) ;
int  ml_isciikey_state_delete( ml_isciikey_state_t) __attribute__((weak)) ;
size_t  ml_convert_ascii_to_iscii( ml_isciikey_state_t ,
	u_char * , size_t , u_char * , size_t) __attribute__((weak)) ;
#endif

ml_isciikey_state_t
ml_isciikey_state_new(
	int  is_inscript
	)
{
	ml_isciikey_state_t (*func)(int) ;

	if( ! (func = ml_load_ctl_iscii_func( ML_ISCIIKEY_STATE_NEW)))
	{
		return  NULL ;
	}

	return  (*func)( is_inscript) ;
}

int
ml_isciikey_state_delete(
	ml_isciikey_state_t  state
	)
{
	int (*func)( ml_isciikey_state_t) ;

	if( ! (func = ml_load_ctl_iscii_func( ML_ISCIIKEY_STATE_DELETE)))
	{
		return  0 ;
	}

	return  (*func)( state) ;
}

size_t
ml_convert_ascii_to_iscii(
	ml_isciikey_state_t  state ,
	u_char *  iscii ,
	size_t  iscii_len ,
	u_char *  ascii ,
	size_t  ascii_len
	)
{
	int (*func)( ml_isciikey_state_t , u_char * , size_t , u_char * , size_t) ;

	if( ! (func = ml_load_ctl_iscii_func( ML_CONVERT_ASCII_TO_ISCII)))
	{
		return  0 ;
	}

	return  (*func)( state , iscii , iscii_len , ascii , ascii_len) ;
}

#else

#ifdef  USE_IND
#include  "libctl/ml_iscii.c"
#else

/*
 * Dummy functions are necessary for x_im.c
 */

ml_isciikey_state_t
ml_isciikey_state_new(
	int  is_inscript
	)
{
	return  NULL ;
}

int
ml_isciikey_state_delete(
	ml_isciikey_state_t  state
	)
{
	return  0 ;
}

size_t
ml_convert_ascii_to_iscii(
	ml_isciikey_state_t  state ,
	u_char *  iscii ,
	size_t  iscii_len ,
	u_char *  ascii ,
	size_t  ascii_len
	)
{
	return  0 ;
}

#endif
#endif
