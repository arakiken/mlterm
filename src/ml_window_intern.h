/*
 *	$Id$
 */

#ifndef  __ML_WINDOW_INTERN_H__
#define  __ML_WINDOW_INTERN_H__


#ifdef  ANTI_ALIAS

#define  XFT_COLOR(win,color)  ((win)->color_table[(color)])
#define  COLOR_PIXEL(win,color)  (XFT_COLOR(win,color)->pixel)

#else

#define  COLOR_PIXEL(win,color)  ((win)->color_table[(color)]->pixel)

#endif

#define  FG_COLOR_PIXEL(win)  COLOR_PIXEL(win,MLC_FG_COLOR)
#define  BG_COLOR_PIXEL(win)  COLOR_PIXEL(win,MLC_BG_COLOR)
#define  UNFADE_FG_COLOR_PIXEL(win) \
	( (win)->orig_fg_xcolor ? (win)->orig_fg_xcolor->pixel : FG_COLOR_PIXEL(win) )
#define  UNFADE_BG_COLOR_PIXEL(win)  \
	( (win)->orig_bg_xcolor ? (win)->orig_bg_xcolor->pixel : BG_COLOR_PIXEL(win) )


#endif
