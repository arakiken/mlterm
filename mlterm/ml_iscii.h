/*
 *	$Id$
 */

#ifndef  __ML_ISCII_H__
#define  __ML_ISCII_H__


#include  <kiklib/kik_types.h>		/* u_int/u_char */


typedef struct ml_iscii_lang *  ml_iscii_lang_t ;

typedef struct ml_iscii_keymap *  ml_iscii_keymap_t ;

typedef enum  ml_iscii_lang_type
{
	ISCIILANG_UNKNOWN = -1 ,
	
	ISCIILANG_ASSAMESE = 0 ,
	ISCIILANG_BENGALI ,
	ISCIILANG_GUJARATI ,
	ISCIILANG_HINDI ,
	ISCIILANG_KANNADA ,
	ISCIILANG_MALAYALAM ,
	ISCIILANG_ORIYA ,
	ISCIILANG_PUNJABI ,
	ISCIILANG_ROMAN ,
	ISCIILANG_TAMIL ,
	ISCIILANG_TELUGU ,

	MAX_ISCIILANG ,

} ml_iscii_lang_type_t ;


ml_iscii_lang_type_t  ml_iscii_get_lang( char *  name) ;

char *  ml_iscii_get_lang_name( ml_iscii_lang_type_t  lang) ;

/*
 * lang
 */
 
ml_iscii_lang_t  ml_iscii_lang_new( ml_iscii_lang_type_t  type) ;

int  ml_iscii_lang_delete( ml_iscii_lang_t  lang) ;

char *  ml_iscii_get_font_name( ml_iscii_lang_t  lang , u_int  font_size) ;

u_int  ml_iscii_shape( ml_iscii_lang_t  lang , u_char *  dst , size_t  dst_size , u_char *  src) ;

/*
 * keymap
 */
 
ml_iscii_keymap_t  ml_iscii_keymap_new( int  is_inscript) ;

int  ml_iscii_keymap_delete( ml_iscii_keymap_t  keymap) ;

size_t  ml_convert_ascii_to_iscii( ml_iscii_keymap_t  keymap ,
	u_char *  iscii , size_t  iscii_len , u_char *  ascii , size_t  ascii_len) ;


#endif
