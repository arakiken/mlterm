/*
 *	$Id$
 */

#ifndef  __ML_ISCII_H__
#define  __ML_ISCII_H__


#include  <kiklib/kik_types.h>


typedef struct ml_iscii_state *  ml_iscii_state_t ;

typedef enum  ml_iscii_lang
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

} ml_iscii_lang_t ;

typedef enum  ml_iscii_keyb
{
	ISCIIKEYB_UNKNOWN = -1 ,
	
	ISCIIKEYB_NONE = 0 ,
	ISCIIKEYB_INSCRIPT ,
	ISCIIKEYB_IITKEYB ,

	MAX_ISCIIKEYB ,

} ml_iscii_keyb_t ;


ml_iscii_lang_t  ml_iscii_get_lang( char *  name) ;


ml_iscii_state_t  ml_iscii_new(void) ;

int  ml_iscii_delete( ml_iscii_state_t  iscii_state) ;

int  ml_iscii_select_lang( ml_iscii_state_t  iscii_state , ml_iscii_lang_t  lang) ;

char *  ml_iscii_get_font_name( ml_iscii_state_t  iscii_state , u_int  font_size) ;

u_int  ml_iscii_shape( ml_iscii_state_t  iscii_state , u_char *  dst , size_t  dst_size , u_char *  src) ;

int  ml_iscii_select_keyb( ml_iscii_state_t  iscii_state , ml_iscii_keyb_t  keyb) ;

ml_iscii_keyb_t  ml_iscii_current_keyb( ml_iscii_state_t  iscii_state) ;

size_t  ml_convert_ascii_to_iscii( ml_iscii_state_t  iscii_state ,
	u_char *  iscii , size_t  iscii_len , u_char *  ascii , size_t  ascii_len) ;


#endif
