/*
 *	update: <2001/11/16(07:20:23)>
 *	$Id$
 */

#ifndef  __ML_WINDOW_INTERN_H__
#define  __ML_WINDOW_INTERN_H__


#define  MAX_CHILDS  2


#ifdef  ANTI_ALIAS

#define  XFT_COLOR(win,color)  ((win)->color_table[(color)])
#define  COLOR_PIXEL(win,color)  (XFT_COLOR(win,color)->pixel)

#else

#define  COLOR_PIXEL(win,color)  ((win)->color_table[(color)]->pixel)

#endif

#define  FG_COLOR_PIXEL(win)  COLOR_PIXEL(win,MLC_FG_COLOR)
#define  BG_COLOR_PIXEL(win)  COLOR_PIXEL(win,MLC_BG_COLOR)


#endif
