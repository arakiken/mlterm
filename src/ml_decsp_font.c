/*
 *	$Id$
 */

#include  "ml_decsp_font.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_mem.h>	/* malloc */


/* --- global functions --- */

ml_decsp_font_t *
ml_decsp_font_new(
	Display *  display ,
	u_int  width ,
	u_int  height ,
	u_int  height_to_baseline
	)
{
	ml_decsp_font_t *  font ;
	char  gray_bits[] = { 0x11 , 0x44 } ;
	Window  win ; 
	u_int  glyph_width ;
	u_int  glyph_height ;
	GC  gc ;
	Pixmap  gray ;
	XPoint  pts[4] ;
	int  counter ;

	if( ( font = malloc( sizeof( ml_decsp_font_t))) == NULL)
	{
		return  NULL ;
	}

	font->width = width ;
	font->height = height ;
	font->height_to_baseline = height_to_baseline ;

	glyph_width = width - 1 ;
	glyph_height = height - 1 ;

	win = DefaultRootWindow( display) ;
	
	gray = XCreateBitmapFromData( display , win , gray_bits , 8 , 2) ;
	gc = XCreateGC( display , gray , 0 , NULL) ;

	XSetForeground( display , gc , 0) ;
	XSetFillStyle( display , gc , FillSolid) ;

	memset( font->glyphs , 0 , sizeof( font->glyphs)) ;
	
	for( counter = 1 ; counter < sizeof( font->glyphs) / sizeof( font->glyphs[0]) ; counter ++)
	{
		/*
		 * Glyph map
		 *
		 * None , Used , Used , None , None , None , None , None ,
		 * None , None , None , Used , Used , Used , Used , Used ,
		 * Used , Used , Used , Used , Used , Used , Used , Used ,
		 * Used , Used , None , None , None , None , Used , None ,
		 */
		if( counter <= 0x02 || ( 0x0b <= counter && counter <= 0x19) || counter == 0x1e)
		{
			font->glyphs[counter] = XCreatePixmap( display , win , width , height , 1) ;
			XFillRectangle( display , font->glyphs[counter] , gc , 0 , 0 , width , height) ;
		}
	}

	XSetForeground( display , gc , 1) ;

	pts[0].x = glyph_width / 2 ;
	pts[0].y = 0 ;
	pts[1].x = 0 ;
	pts[1].y = glyph_height / 2 ;
	pts[2].x = glyph_width / 2 ;
	pts[2].y = glyph_height ;
	pts[3].x = glyph_width ;
	pts[3].y = glyph_height / 2 ;
	XFillPolygon( display , font->glyphs[0x01] , gc , pts , 4 , Nonconvex , CoordModeOrigin) ;

	XSetFillStyle( display , gc , FillStippled) ;
	XSetStipple( display , gc , gray) ;
	XFillRectangle( display , font->glyphs[0x02] , gc , 0 , 0 , width , height) ;

	XSetFillStyle( display , gc , FillSolid) ;
	XDrawLine( display , font->glyphs[0x0b] , gc ,
		0 , glyph_height / 2 , glyph_width / 2 , glyph_height / 2) ;
	XDrawLine( display , font->glyphs[0x0b] , gc ,
		glyph_width / 2 , 0 , glyph_width / 2 , glyph_height / 2) ;

	XDrawLine( display , font->glyphs[0x0c] , gc ,
		0 , glyph_height / 2 , glyph_width / 2 , glyph_height / 2) ;
	XDrawLine( display , font->glyphs[0x0c] , gc ,
		glyph_width / 2 , glyph_height / 2 , glyph_width / 2 , glyph_height) ;

	XDrawLine( display , font->glyphs[0x0d] , gc ,
		glyph_width / 2 , glyph_height / 2 , glyph_width , glyph_height / 2) ;
	XDrawLine( display , font->glyphs[0x0d] , gc ,
		glyph_width / 2 , glyph_height / 2 , glyph_width / 2 , glyph_height) ;

	XDrawLine( display , font->glyphs[0x0e] , gc ,
		glyph_width / 2 , glyph_height / 2 , glyph_width , glyph_height / 2) ;
	XDrawLine( display , font->glyphs[0x0e] , gc ,
		glyph_width / 2 , 0 , glyph_width / 2 , glyph_height / 2) ;

	XDrawLine( display , font->glyphs[0x0f] , gc ,
		0 , glyph_height / 2 , glyph_width , glyph_height / 2) ;
	XDrawLine( display , font->glyphs[0x0f] , gc ,
		glyph_width / 2 , 0 , glyph_width / 2 , glyph_height) ;

	XDrawLine( display , font->glyphs[0x10] , gc ,
		0 , 0 , glyph_width , 0) ;

	XDrawLine( display , font->glyphs[0x11] , gc ,
		0 , glyph_height / 4 , glyph_width , glyph_height / 4) ;

	XDrawLine( display , font->glyphs[0x12] , gc ,
		0 , glyph_height / 2 , glyph_width , glyph_height / 2) ;

	XDrawLine( display , font->glyphs[0x13] , gc ,
		0 , glyph_height * 3 / 4 , glyph_width , glyph_height * 3 / 4) ;

	XDrawLine( display , font->glyphs[0x14] , gc ,
		0 , glyph_height , glyph_width , glyph_height) ;

	XDrawLine( display , font->glyphs[0x15] , gc ,
		glyph_width / 2 , glyph_height / 2 , glyph_width , glyph_height / 2) ;
	XDrawLine( display , font->glyphs[0x15] , gc ,
		glyph_width / 2 , 0 , glyph_width / 2 , glyph_height) ;

	XDrawLine( display , font->glyphs[0x16] , gc ,
		0 , glyph_height / 2 , glyph_width / 2 , glyph_height / 2) ;
	XDrawLine( display , font->glyphs[0x16] , gc ,
		glyph_width / 2 , 0 , glyph_width / 2 , glyph_height) ;

	XDrawLine( display , font->glyphs[0x17] , gc ,
		0 , glyph_height / 2 , glyph_width , glyph_height / 2) ;
	XDrawLine( display , font->glyphs[0x17] , gc ,
		glyph_width / 2 , 0 , glyph_width / 2 , glyph_height / 2) ;

	XDrawLine( display , font->glyphs[0x18] , gc ,
		0 , glyph_height / 2 , glyph_width , glyph_height / 2) ;
	XDrawLine( display , font->glyphs[0x18] , gc ,
		glyph_width / 2 , glyph_height / 2 , glyph_width / 2 , glyph_height) ;

	XDrawLine( display , font->glyphs[0x19] , gc ,
		glyph_width / 2 , 0 , glyph_width / 2 , glyph_height) ;

	XDrawLine( display , font->glyphs[0x1e] , gc ,
		glyph_width / 2 - 1 , glyph_height / 2 , glyph_width / 2 + 1 , glyph_height / 2) ;
	XDrawLine( display , font->glyphs[0x1e] , gc ,
		glyph_width / 2 , glyph_height / 2 - 1 , glyph_width / 2 , glyph_height / 2 + 1) ;

	XFreePixmap( display , gray) ;
	XFreeGC( display , gc) ;

	return  font ;
}

int
ml_decsp_font_delete(
	ml_decsp_font_t *  font ,
	Display *  display
	)
{
	int  counter ;

	for( counter = 0 ; counter < sizeof( font->glyphs) / sizeof( font->glyphs[0]) ; counter ++)
	{
		if( font->glyphs[counter])
		{
			XFreePixmap( display , font->glyphs[counter]) ;
		}
	}

	free( font) ;

	return  1 ;
}

int
ml_decsp_font_draw_string(
	ml_decsp_font_t *  font ,
	Display *  display ,
	Drawable  drawable ,
	GC  gc ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	int  counter ;

	for( counter = 0 ; counter < len ; counter ++)
	{
		if( /* 0x00 <= *str && */ *str < 0x20 && font->glyphs[*str])
		{
			XCopyPlane( display , font->glyphs[*str] , drawable , gc ,
				0 , 0 , font->width , font->height ,
				x , y - font->height_to_baseline , 1) ;
		}

		x += font->width ;
		str ++ ;
	}

	return  1 ;
}
