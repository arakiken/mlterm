/*
 *	$Id$
 */

#ifndef  __ML_MOD_META_MODE_H__
#define  __ML_MOD_META_MODE_H__


typedef enum  ml_mod_meta_mode
{
	MOD_META_NONE = 0x0 ,
	MOD_META_OUTPUT_ESC ,
	MOD_META_SET_MSB ,
	
} ml_mod_meta_mode_t ;


ml_mod_meta_mode_t  ml_get_mod_meta_mode( char *  name) ;

char *  ml_get_mod_meta_mode_name( ml_mod_meta_mode_t  mode) ;


#endif
