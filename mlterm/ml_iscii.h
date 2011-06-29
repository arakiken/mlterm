/*
 *	$Id$
 */

#ifndef  __ML_ISCII_H__
#define  __ML_ISCII_H__


#include  <kiklib/kik_types.h>		/* u_int/u_char */
#include  <mkf/mkf_charset.h>		/* mkf_iscii_lang_t */

#include  "ml_char.h"


typedef struct ml_isciikey_state *  ml_isciikey_state_t ;

typedef struct  ml_iscii_state
{
	u_int8_t *  num_of_chars_array ;
	u_int16_t  size ;

	int8_t  has_iscii ;

}  ml_iscii_state_t ;


/*
 * lang
 */

#ifdef  USE_IND

u_int  ml_iscii_shape( mkf_charset_t  cs , u_char *  dst , size_t  dst_size , u_char *  src) ;

ml_iscii_state_t *  ml_iscii_new( void) ;

int  ml_iscii_copy( ml_iscii_state_t *  dst , ml_iscii_state_t *  src) ;

int  ml_iscii_delete( ml_iscii_state_t *  state) ;

int  ml_iscii_reset( ml_iscii_state_t *  state) ;

int  ml_iscii( ml_iscii_state_t *  state , ml_char_t *  src , u_int  src_len) ;

#else

#define  ml_iscii_shape( lang , dst , dst_size , src)  (0)

#endif

/*
 * dummy symbol is necessary for x_im.c even if USE_IND isn't defined.
 */

ml_isciikey_state_t  ml_isciikey_state_new( int  is_inscript) ;

int  ml_isciikey_state_delete( ml_isciikey_state_t  state) ;

size_t  ml_convert_ascii_to_iscii( ml_isciikey_state_t  state ,
	u_char *  iscii , size_t  iscii_len , u_char *  ascii , size_t  ascii_len) ;


#endif
