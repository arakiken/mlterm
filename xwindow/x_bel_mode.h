/*
 *	$Id$
 */

#ifndef  __X_BEL_MODE_H__
#define  __X_BEL_MODE_H__


typedef enum  x_bel_mode
{
	BEL_NONE = 0x0 ,
	BEL_SOUND = 0x1 ,
	BEL_VISUAL = 0x2 ,
	/* BEL_SOUND|BEL_VISUAL */

	BEL_MODE_MAX = 0x4
	
} x_bel_mode_t ;


x_bel_mode_t  x_get_bel_mode_by_name( char *  name) ;

char *  x_get_bel_mode_name( x_bel_mode_t  mode) ;


#endif
