/*
 *	$Id$
 */

#ifndef  __ML_FONT_H__
#define  __ML_FONT_H__


#include  <mkf/mkf_charset.h>


#define  DEFAULT_FONT(cs)  (cs)
#define  FONT_CS(font)  ((font) & MAX_CHARSET)


typedef enum ml_font
{
	/* 0x00 - MAX_CHARSET(0x7ff) is reserved for mkf_charset_t */

	/* 0x1000 is reserved for unicode half or full width tag */
	FONT_BIWIDTH = 0x1000u ,	/* (default) half width */

	/* 0x2000 is reserved for font thickness */
	FONT_BOLD    = 0x2000u ,	/* (default) medium */

#if  0
	/* not defined in VT100 */
	
	/* for font slant */
	FONT_ITALIC ,		/* (default) roman */
	
	/* font width */
	FONT_SEMICONDENSED ,	/* (default) normal */
#endif

} ml_font_t ;


#endif
