/*
 *	$Id$
 */

#ifndef  __X_MOD_META_MODE_H__
#define  __X_MOD_META_MODE_H__


typedef enum  x_mod_meta_mode
{
	MOD_META_NONE = 0x0 ,
	MOD_META_OUTPUT_ESC ,
	MOD_META_SET_MSB ,
	
	MOD_META_MODE_MAX

} x_mod_meta_mode_t ;


x_mod_meta_mode_t  x_get_mod_meta_mode( char *  name) ;

char *  x_get_mod_meta_mode_name( x_mod_meta_mode_t  mode) ;


#endif
