/*
 *	$Id$
 */

#ifndef  __X_SB_MODE_H__
#define  __X_SB_MODE_H__


typedef enum  x_sb_mode
{
	SB_NONE ,
	SB_LEFT ,
	SB_RIGHT ,

} x_sb_mode_t ;


x_sb_mode_t  x_get_sb_mode( char *  name) ;

char *  x_get_sb_mode_name( x_sb_mode_t  mode) ;


#endif
