/*
 *	$Id$
 */

#ifndef  __ML_SB_MODE_H__
#define  __ML_SB_MODE_H__


typedef enum  ml_sb_mode
{
	SB_NONE ,
	SB_LEFT ,
	SB_RIGHT ,

} ml_sb_mode_t ;


ml_sb_mode_t  ml_get_sb_mode( char *  name) ;

char *  ml_get_sb_mode_name( ml_sb_mode_t  mode) ;


#endif
