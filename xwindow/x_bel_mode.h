/*
 *	$Id$
 */

#ifndef  __X_BEL_MODE_H__
#define  __X_BEL_MODE_H__


typedef enum  x_bel_mode
{
	BEL_NONE = 0x0 ,
	BEL_SOUND ,
	BEL_VISUAL ,
	
} x_bel_mode_t ;


x_bel_mode_t  x_get_bel_mode( char *  name) ;

char *  x_get_bel_mode_name( x_bel_mode_t  mode) ;


#endif
