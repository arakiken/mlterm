/*
 *	$Id$
 */

#include  "x_imagelib.h"

#include  <X11/Xatom.h>			/* XA_PIXMAP */
#include  <kiklib/kik_debug.h>


/* --- global functions --- */

int
x_imagelib_display_opened(
	Display *  display
	)
{
	return  1 ;
}

int
x_imagelib_display_closed(
	Display *  display
	)
{
	return  1 ;
}

Pixmap
x_imagelib_load_file_for_background(
	x_window_t *  win ,
	char *  file_path ,
	x_picture_modifier_t *  pic_mod
	)
{
	return  None ;
}

int
x_imagelib_root_pixmap_available(
	Display *   display
	)
{
	if( XInternAtom( display , "_XROOTPMAP_ID" , True))
	{
		return  1 ;
	}

	return  0 ;
}

Pixmap
x_imagelib_get_transparent_background(
	x_window_t *  win ,
	x_picture_modifier_t *  pic_mod
	)
{
	Atom id ;
	int  x ;
	int  y ;
	int  _x ;
	int  _y ;
	u_int  width ;
	u_int  height ;
	Pixmap  pixmap ;
	Atom act_type ;
	int  act_format ;
	u_long  nitems ;
	u_long  bytes_after ;
	u_char *  prop ;

	if( pic_mod && pic_mod->brightness != 100)
	{
		/*
		 * XXX
		 * image is never modified without Imlib.
		 */
		
		return  None ;
	}
	
	if( ( id = XInternAtom( win->display , "_XROOTPMAP_ID" , True)) == None)
	{
		return  None ;
	}
	
	if( ! x_window_get_visible_geometry( win , &x , &y , &_x , &_y , &width , &height))
	{
		return  None ;
	}
	
	if( XGetWindowProperty( win->display , DefaultRootWindow(win->display) , id , 0 , 1 , False ,
		XA_PIXMAP , &act_type , &act_format , &nitems , &bytes_after , &prop) != Success ||
		prop == NULL)
	{
		return  None ;
	}
	
	if( ! *prop)
	{
		XFree( prop) ;

		return  None ;
	}
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " root pixmap %d found.\n" , *prop) ;
#endif

	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;

	XCopyArea( win->display , (*(Drawable*)prop) , pixmap , win->gc ,
		x , y , width , height , _x , _y) ;

	return  pixmap ;
}

int x_imagelib_load_file(
	Display *  display,
	char *  path,
	u_int32_t **  cardinal,
	Pixmap *  pixmap,
	Pixmap *  mask,
	unsigned int *  width,
	unsigned int *  height
	)
{
	return 0 ;
}
