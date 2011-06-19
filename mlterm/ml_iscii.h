/*
 *	$Id$
 */

#ifndef  __ML_ISCII_H__
#define  __ML_ISCII_H__


#include  <kiklib/kik_types.h>		/* u_int/u_char */
#include  <mkf/mkf_charset.h>		/* mkf_iscii_lang_t */


typedef struct ml_iscii_lang *  ml_iscii_lang_t ;

typedef struct ml_iscii_keymap *  ml_iscii_keymap_t ;


mkf_iscii_lang_t  ml_iscii_get_lang( char *  name) ;

char *  ml_iscii_get_lang_name( mkf_iscii_lang_t  lang) ;

/*
 * lang
 */

#ifdef  USE_IND

ml_iscii_lang_t  ml_iscii_lang_new( mkf_iscii_lang_t  type) ;

int  ml_iscii_lang_delete( ml_iscii_lang_t  lang) ;

char *  ml_iscii_get_font_name( ml_iscii_lang_t  lang , u_int  font_size) ;

u_int  ml_iscii_shape( ml_iscii_lang_t  lang , u_char *  dst , size_t  dst_size , u_char *  src) ;

#else

#define  ml_iscii_lang_new( type)  (NULL)

#define  ml_iscii_lang_delete( lang)  (0)

#define  ml_iscii_get_font_name( lang , font_size)  (NULL)

#define  ml_iscii_shape( lang , dst , dst_size , src)  (0)

#endif

/*
 * keymap
 * (dummy symbols are necessary for x_im.c even if USE_IND isn't defined.)
 */
 
ml_iscii_keymap_t  ml_iscii_keymap_new( int  is_inscript) ;

int  ml_iscii_keymap_delete( ml_iscii_keymap_t  keymap) ;

size_t  ml_convert_ascii_to_iscii( ml_iscii_keymap_t  keymap ,
	u_char *  iscii , size_t  iscii_len , u_char *  ascii , size_t  ascii_len) ;


#endif
