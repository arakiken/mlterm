/*
 *	$Id$
 */

#ifndef  __ML_BEL_MODE_H__
#define  __ML_BEL_MODE_H__


typedef enum  ml_bel_mode
{
	BEL_NONE = 0x0 ,
	BEL_SOUND ,
	BEL_VISUAL ,
	
} ml_bel_mode_t ;


ml_bel_mode_t  ml_get_bel_mode( char *  name) ;

char *  ml_get_bel_mode_name( ml_bel_mode_t  mode) ;


#endif
