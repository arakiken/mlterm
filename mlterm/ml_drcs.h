/*
 *	$Id$
 */

#ifndef  __ML_DRCS_H__
#define  __ML_DRCS_H__

#include  <kiklib/kik_types.h>
#include  <mkf/mkf_char.h>


typedef struct ml_drcs_font
{
	mkf_charset_t  cs ;
	char *  glyphs[0x5f] ;

} ml_drcs_font_t ;

typedef struct ml_drcs
{
	ml_drcs_font_t *  fonts ;
	u_int  num_of_fonts ;

} ml_drcs_t ;


#define  ml_drcs_new()  calloc(1,sizeof(ml_drcs_t))

#define  ml_drcs_delete(drcs)  ml_drcs_select(drcs);ml_drcs_final_full();free(drcs);

void  ml_drcs_select( ml_drcs_t *  drcs) ;

ml_drcs_font_t *  ml_drcs_get_font( mkf_charset_t  cs , int  create) ;

char *  ml_drcs_get_glyph( mkf_charset_t  cs , u_char  idx) ;

int  ml_drcs_final( mkf_charset_t  cs) ;

int  ml_drcs_final_full(void) ;

int  ml_drcs_add( ml_drcs_font_t *  font ,
	int  idx , char *  seq , u_int  width , u_int  height) ;

int  ml_convert_drcs_to_unicode_pua( mkf_char_t *  ch) ;

int  ml_convert_unicode_pua_to_drcs( mkf_char_t *  ch) ;


#endif
