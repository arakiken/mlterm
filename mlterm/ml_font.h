/*
 *	$Id$
 */

#ifndef  __ML_FONT_H__
#define  __ML_FONT_H__


#include  <mkf/mkf_charset.h>


#define  NORMAL_FONT_OF(cs)  (IS_BIWIDTH_CS(cs) ? (cs) | FONT_BIWIDTH : (cs))
#define  FONT_CS(font)  ((font) & MAX_CHARSET)
#define  FONT_STYLE_INDEX(font)  ((((font) & (FONT_BOLD|FONT_ITALIC)) >> 13) - 1)

typedef enum ml_font
{
	/* 0x00 - MAX_CHARSET(0x2ff) is reserved for mkf_charset_t */

	/* 0x1000 is reserved for unicode half or full width tag */
	FONT_BIWIDTH = 0x1000u ,	/* (default) half width */

	/* 0x2000 is reserved for font thickness */
	FONT_BOLD    = 0x2000u ,	/* (default) medium */

	/* for font slant */
	FONT_ITALIC  = 0x4000u ,	/* (default) roman */

#if  0
	/* font width */
	FONT_SEMICONDENSED	/* (default) normal */
#endif

} ml_font_t ;


#endif
