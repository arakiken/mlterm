/*
 *	$Id$
 */

#ifndef  __ML_ISCII_H__
#define  __ML_ISCII_H__


#include  <kiklib/kik_types.h>		/* u_int/u_char */


typedef struct ml_isciikey_state *  ml_isciikey_state_t ;

typedef struct ml_iscii_state *  ml_iscii_state_t ;


ml_isciikey_state_t  ml_isciikey_state_new( int  is_inscript) ;

int  ml_isciikey_state_delete( ml_isciikey_state_t  state) ;

size_t  ml_convert_ascii_to_iscii( ml_isciikey_state_t  state ,
	u_char *  iscii , size_t  iscii_len , u_char *  ascii , size_t  ascii_len) ;


#endif
