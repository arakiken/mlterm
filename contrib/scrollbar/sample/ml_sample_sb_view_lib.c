/*
 *	$Id$
 */

#include  "ml_sample_sb_view_lib.h"

#include  <stdio.h>


/* --- global functions --- */

Pixmap
ml_get_icon_pixmap(
	ml_sb_view_t *  view ,
	GC  gc_intern ,
	char **  data ,
	unsigned int  width ,
	unsigned int  height
	)
{
	GC  gc ;
	Pixmap  pix ;
	char  cur ;
	int  x ;
	int  y ;
	
	pix = XCreatePixmap( view->display , view->window , width , height ,
		DefaultDepth( view->display , view->screen)) ;

	cur = '\0' ;
	for( y = 0 ; y < height ; y ++)
	{
		for( x = 0 ; x < width ; x ++)
		{
			if( cur != data[y][x])
			{
				if( data[y][x] == ' ')
				{
					XSetForeground( view->display , gc_intern ,
						WhitePixel( view->display , view->screen)) ;
					gc = gc_intern ;
				}
				else if( data[y][x] == '#')
				{
					XSetForeground( view->display , gc_intern ,
						BlackPixel( view->display , view->screen)) ;
					gc = gc_intern ;
				}
				else if( data[y][x] == '%')
				{
					gc = view->gc ;
				}
				else
				{
					continue ;
				}

				cur = data[y][x] ;
			}

			XDrawPoint( view->display , pix , gc , x , y) ; 
		}

		x = 0 ;
	}

	return  pix ;
}
