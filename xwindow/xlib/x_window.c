/*
 *	$Id$
 */

/*
 * Functions designed and implemented by Minami Hirokazu(minami@mistfall.net) are:
 * - XDND support
 * - Extended Window Manager Hint(Icon) support
 */

#include  "../x_window.h"

#include  <stdlib.h>		/* abs */
#include  <string.h>		/* memset/memcpy */
#include  <X11/Xutil.h>		/* for XSizeHints */
#include  <X11/Xatom.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* realloc/free */
#include  <kiklib/kik_util.h>	/* K_MAX */
#include  <kiklib/kik_unistd.h>	/* kik_usleep */

#include  "../x_xic.h"
#include  "../x_picture.h"
#ifndef  DISABLE_XDND
#include  "../x_dnd.h"
#endif
#include  "../x_type_loader.h"

#include  "x_display.h"		/* x_display_get_cursor */
#include  "x_decsp_font.h"


/*
 * Atom macros.
 * Not cached because Atom may differ on each display
 */

#define  XA_CLIPBOARD(display)  (XInternAtom(display , "CLIPBOARD" , False))
#define  XA_COMPOUND_TEXT(display)  (XInternAtom(display , "COMPOUND_TEXT" , False))
#define  XA_TARGETS(display)  (XInternAtom(display , "TARGETS" , False))
#ifdef  DEBUG
#define  XA_MULTIPLE(display)  (XInternAtom(display , "MULTIPLE" , False))
#endif
#define  XA_TEXT(display)  (XInternAtom( display , "TEXT" , False))
#define  XA_UTF8_STRING(display)  (XInternAtom(display , "UTF8_STRING" , False))
#define  XA_BMP(display)  (XInternAtom( display , "image/bmp" , False))
#define  XA_NONE(display)  (XInternAtom(display , "NONE" , False))
#define  XA_SELECTION(display) (XInternAtom(display , "MLTERM_SELECTION" , False))
#define  XA_DELETE_WINDOW(display) (XInternAtom(display , "WM_DELETE_WINDOW" , False))
#define  XA_TAKE_FOCUS(display) (XInternAtom(display , "WM_TAKE_FOCUS" , False))
#define  XA_INCR(display) (XInternAtom(display, "INCR", False))
#define  XA_XROOTPMAP_ID(display) (XInternAtom(display, "_XROOTPMAP_ID", False))
#define  XA_XSETROOT_ID(display) (XInternAtom(display, "_XSETROOT_ID" , False))
#define  XA_WM_CLIENT_LEADER(display) (XInternAtom(display , "WM_CLIENT_LEADER" , False))

/*
 * Extended Window Manager Hint support
 */
#define  XA_NET_WM_ICON(display) (XInternAtom(display, "_NET_WM_ICON", False))


/*
 * Motif Window Manager Hint (for borderless window)
 */
#define  XA_MWM_INFO(display) (XInternAtom(display, "_MOTIF_WM_INFO", True))
#define  XA_MWM_HINTS(display) (XInternAtom(display, "_MOTIF_WM_HINTS", True))

#define  IS_INHERIT_TRANSPARENT(win) \
		( use_inherit_transparent && x_picture_modifier_is_normal( (win)->pic_mod))

/* win->width is not multiples of (win)->width_inc if window is maximized. */
#define  RIGHT_MARGIN(win) \
	((win)->width_inc ? ((win)->width - (win)->min_width) % (win)->width_inc : 0)
#define  BOTTOM_MARGIN(win) \
	((win)->height_inc ? ((win)->height - (win)->min_height) % (win)->height_inc : 0)


typedef struct {
	u_int32_t  flags ;
	u_int32_t  functions ;
	u_int32_t  decorations ;
	int32_t  inputMode ;
	u_int32_t  status ;
} MWMHints_t ;

#define MWM_HINTS_DECORATIONS   (1L << 1)


#define  MAX_CLICK  3			/* max is triple click */


#define  restore_fg_color(win)  x_gc_set_fg_color((win)->gc,(win)->fg_color.pixel)
#define  restore_bg_color(win)  x_gc_set_bg_color((win)->gc,(win)->bg_color.pixel)


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  click_interval = 250 ;	/* millisecond, same as xterm. */
/* ParentRelative isn't used for transparency by default */
static int  use_inherit_transparent = 0 ;
static int  use_clipboard = 1 ;
static int  use_urgent_bell = 0 ;

static struct
{
	u_int8_t  h_type[2] ;
	u_int8_t  h_size[4] ;
	u_int8_t  h_res1[2] ;
	u_int8_t  h_res2[2] ;
	u_int8_t  h_offbits[4] ;

	u_int8_t  i_size[4] ;
	u_int8_t  i_width[4] ;
	u_int8_t  i_height[4] ;
	u_int8_t  i_planes[2] ;
	u_int8_t  i_bitcount[2] ;
	u_int8_t  i_compression[4] ;
	u_int8_t  i_sizeimage[4] ;
	u_int8_t  i_xpelspermeter[4] ;
	u_int8_t  i_ypelspermeter[4] ;
	u_int8_t  i_clrused[4] ;
	u_int8_t  i_clrimportant[4] ;

	u_char  data[1] ;

} *  sel_bmp ;
static size_t  sel_bmp_size ;


/* --- static functions --- */

static void
urgent_bell(
	x_window_t *  win ,
	int  on
	)
{
	if( use_urgent_bell && ( ! win->is_focused || ! on))
	{
	#ifndef  XUrgencyHint
	#define  XUrgencyHint  (1L << 8)	/* not defined in X11R5 */
	#endif

		XWMHints *  hints ;

		win = x_get_root_window( win) ;

		if( ( hints = XGetWMHints( win->disp->display , win->my_window)))
		{
			if( on)
			{
				hints->flags |= XUrgencyHint ;
			}
			else
			{
				hints->flags &= ~XUrgencyHint ;
			}

			XSetWMHints( win->disp->display , win->my_window , hints) ;
			XFree( hints) ;
		}
	}
}

static int
clear_margin_area(
	x_window_t *  win
	)
{
	u_int  right_margin ;
	u_int  bottom_margin ;

	right_margin = RIGHT_MARGIN(win) ;
	bottom_margin = BOTTOM_MARGIN(win) ;

	if( win->hmargin > 0)
	{
		XClearArea( win->disp->display , win->my_window ,
			0 , 0 , win->hmargin , ACTUAL_HEIGHT(win) , 0) ;
	}

	if( win->hmargin + right_margin > 0)
	{
		XClearArea( win->disp->display , win->my_window ,
			win->width - right_margin + win->hmargin , 0 ,
			win->hmargin + right_margin , ACTUAL_HEIGHT(win) , 0) ;
	}

	if( win->vmargin > 0)
	{
		XClearArea( win->disp->display , win->my_window ,
			win->hmargin , 0 , win->width - right_margin , win->vmargin , 0) ;
	}

	if( win->vmargin + bottom_margin > 0)
	{
		XClearArea( win->disp->display , win->my_window ,
			win->hmargin , win->height - bottom_margin + win->vmargin ,
			win->width - right_margin , win->vmargin + bottom_margin , 0) ;
	}

	return  1 ;
}

/* Only used for set_transparent|update_modified_transparent */
static int
set_transparent_picture(
	x_window_t *  win ,
	Pixmap  pixmap
	)
{
	/*
	 * !! Notice !!
	 * This must be done before x_window_set_wall_picture() because
	 * x_window_set_wall_picture() doesn't do anything if is_transparent
	 * flag is on.
	 */
	win->is_transparent = 0 ;

	if( ! x_window_set_wall_picture( win , pixmap , 1))
	{
		win->pic_mod = NULL ;

		return  0 ;
	}

	win->is_transparent = 1 ;

	return  1 ;
}

/* Only used for set_transparent */
static int
update_transparent_picture(
	x_window_t *  win
	)
{
	x_picture_t *  pic ;

	if( ! ( pic = x_acquire_bg_picture( win , win->pic_mod , "root")))
	{
		goto  error1 ;
	}

	if( ! set_transparent_picture( win , pic->pixmap))
	{
		goto  error2 ;
	}

	x_release_picture( pic) ;

	return  1 ;

error2:
	x_release_picture( pic) ;

error1:
	win->is_transparent = 0 ;

	/* win->pic_mod = NULL is done in set_transparent. */

	return  0 ;
}

static int
unset_transparent(
	x_window_t *  win
	)
{
	/*
	 * XXX
	 * If previous mode is not modified transparent,
	 * ParentRelative mode of parent windows should be unset.
	 */

	/*
	 * !! Notice !!
	 * this must be done before x_window_unset_wall_picture() because
	 * x_window_unset_wall_picture() doesn't do anything if is_transparent
	 * flag is on.
	 */
	win->is_transparent = 0 ;
	win->pic_mod = NULL ;

	return  x_window_unset_wall_picture( win , 1) ;
}

static int
set_transparent(
	x_window_t *  win
	)
{
	Window  parent ;
	
	if( ! IS_INHERIT_TRANSPARENT(win))
	{
		/*
		 * XXX
		 * If previous mode is not modified transparent,
		 * ParentRelative mode of parent windows should be unset.
		 */

		/* win->is_transparent is set appropriately in update_transparent_picture(). */
		if( update_transparent_picture( win))
		{
			return  1 ;
		}
		else
		{
			kik_msg_printf( "_XROOTPMAP_ID is not found."
				" Trying ParentRelative for transparency instead.\n") ;

			if( ! x_picture_modifier_is_normal( win->pic_mod))
			{
				kik_msg_printf( "(brightness, contrast, gamma and alpha "
					"options are ignored)\n") ;
				
				win->pic_mod = NULL ;
			}

			use_inherit_transparent = 1 ;
		}
	}

	/*
	 * It is not necessary to set ParentRelative more than once, so
	 * this function should be used as follows.
	 * if( ! IS_INHERIT_TRANSPARENT(win) || ! win->wall_picture_is_set)
	 * {
	 *	set_transparent( win) ;
	 * }
	 */
	
	/*
	 * Root - Window A - Window C
	 *      - Window B - Window D
	 *                 - Window E
	 * If Window C is set_transparent(), C -> A -> Root are set ParentRelative.
	 * Window B,D and E are not set ParentRelative.
	 */

	while( win->parent)
	{
		/* win->is_transparent is set appropriately in set_transparent() */
		set_transparent_picture( win , ParentRelative) ;

		win = win->parent ;
	}

	set_transparent_picture( win , ParentRelative) ;

	parent = win->my_window ;
	while( 1)
	{
		Window  root ;
		Window *  list ;
		u_int  n ;
		XWindowAttributes  attr ;

		if( ! XQueryTree( win->disp->display , parent , &root , &parent , &list , &n))
		{
			break ;
		}

		XFree( list) ;

		if( ! parent || parent == root)
		{
			break ;
		}

		if( XGetWindowAttributes( win->disp->display , parent , &attr) &&
		    attr.depth == win->disp->depth)
		{
			XSetWindowBackgroundPixmap( win->disp->display , parent ,
				ParentRelative) ;
		}
		else
		{
			break ;
		}
	}

	return  1 ;
}

static void
notify_focus_in_to_children(
	x_window_t *  win
	)
{
	u_int  count ;

	if( ! win->is_focused && win->inputtable > 0)
	{
		win->is_focused = 1 ;

		if( win->window_focused)
		{
			(*win->window_focused)( win) ;
		}

		x_xic_set_focus( win) ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		notify_focus_in_to_children( win->children[count]) ;
	}
}

static void
notify_focus_out_to_children(
	x_window_t *  win
	)
{
	u_int  count ;

	if( win->is_focused)
	{
		win->is_focused = 0 ;
		
		if( win->window_unfocused)
		{
			(*win->window_unfocused)( win) ;
		}

		x_xic_unset_focus( win) ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		notify_focus_out_to_children( win->children[count]) ;
	}
}

static void
notify_configure_to_children(
	x_window_t *  win
	)
{
	u_int  count ;

	if( win->is_transparent)
	{
		if( ! IS_INHERIT_TRANSPARENT(win) || ! win->wall_picture_is_set)
		{
		#ifdef  __DEBUG
			kik_debug_printf( "configure notify for transparency\n") ;
		#endif
			set_transparent( win) ;
		}
		else if( win->window_exposed)
		{
			clear_margin_area( win) ;
			(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
		}
	#if  0
		else
		{
			clear_margin_area( win) ;
			x_window_clear_all( win) ;
		}
	#endif
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		notify_configure_to_children( win->children[count]) ;
	}
}

static void
notify_reparent_to_children(
	x_window_t *  win
	)
{
	u_int  count ;

	if( win->is_transparent)
	{
		/* Parent window is changed. => Reset transparent. */

		#ifdef  __DEBUG
			kik_debug_printf( "reparent notify for transparency\n") ;
		#endif
		set_transparent( win) ;
	}
	
	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		notify_reparent_to_children( win->children[count]) ;
	}
}

static void
notify_property_to_children(
	x_window_t *  win
	)
{
	u_int  count ;

	if( win->is_transparent)
	{
		/* Background image of desktop is changed. */

		if( ! IS_INHERIT_TRANSPARENT(win))
		{
		#ifdef  __DEBUG
			kik_debug_printf( "property notify for transparency\n") ;
		#endif
			set_transparent( win) ;
		}
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		notify_property_to_children( win->children[count]) ;
	}
}

static int
is_descendant_window(
	x_window_t *  win ,
	Window  window
	)
{
	u_int  count ;

	if( win->my_window == window)
	{
		return  1 ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( is_descendant_window( win->children[count] , window))
		{
			return  1 ;
		}
	}

	return  0 ;
}

static int
is_in_the_same_window_family(
	x_window_t *  win ,
	Window   window
	)
{
	return  is_descendant_window( x_get_root_window( win) , window) ;
}

static u_int
total_min_width(
	x_window_t *  win
	)
{
	u_int  count ;
	u_int  min_width ;

	min_width = win->min_width + win->hmargin * 2 ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			/* XXX */
			min_width += total_min_width( win->children[count]) ;
		}
	}

	return  min_width ;
}

static u_int
total_min_height(
	x_window_t *  win
	)
{
	u_int  count ;
	u_int  min_height ;

	min_height = win->min_height + win->vmargin * 2 ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			/* XXX */
			min_height += total_min_height( win->children[count]) ;
		}
	}

	return  min_height ;
}

static u_int
total_width_inc(
	x_window_t *  win
	)
{
	u_int  count ;
	u_int  width_inc ;

	width_inc = win->width_inc ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			u_int  sub_inc ;

			/*
			 * XXX
			 * we should calculate least common multiple of width_inc and sub_inc.
			 */
			if( ( sub_inc = total_width_inc( win->children[count])) > width_inc)
			{
				width_inc = sub_inc ;
			}
		}
	}

	return  width_inc ;
}

static u_int
total_height_inc(
	x_window_t *  win
	)
{
	u_int  count ;
	u_int  height_inc ;

	height_inc = win->height_inc ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			u_int  sub_inc ;

			/*
			 * XXX
			 * we should calculate least common multiple of width_inc and sub_inc.
			 */
			if( ( sub_inc = total_height_inc( win->children[count])) > height_inc)
			{
				height_inc = sub_inc ;
			}
		}
	}

	return  height_inc ;
}

static XID
reset_client_leader(
	x_window_t *  root
	)
{
	u_long  leader ;

	if( ( leader = x_display_get_group_leader( root->disp)) == None)
	{
		leader = root->my_window ;
	}

	XChangeProperty( root->disp->display , root->my_window ,
			 XA_WM_CLIENT_LEADER(root->disp->display) ,
			 XA_WINDOW , 32 , PropModeReplace ,
			 (unsigned char *)(&leader) , 1) ;

	return  leader ;
}

static void
convert_to_decsp_font_index(
	u_char *  str ,
	u_int  len
	)
{
	while( len != 0)
	{
		if( *str == 0x5f)
		{
			*str = 0x7f ;
		}
		else if( 0x5f < *str && *str < 0x7f)
		{
			(*str) -= 0x5f ;
		}

		len -- ;
		str ++ ;
	}
}

static void
scroll_region(
	x_window_t *  win ,
	int  src_x ,
	int  src_y ,
	u_int  width ,
	u_int  height ,
	int  dst_x ,
	int  dst_y
	)
{
	XCopyArea( win->disp->display , win->my_window , win->my_window , win->gc->gc ,
		src_x + win->hmargin , src_y + win->vmargin , width , height ,
		dst_x + win->hmargin , dst_y + win->vmargin) ;

	while( win->wait_copy_area_response)
	{
		XEvent  ev ;

		XWindowEvent( win->disp->display , win->my_window ,
				ExposureMask , &ev) ;
		if( ev.type == GraphicsExpose)
		{
			/*
			 * GraphicsExpose caused by the previous XCopyArea is
			 * processed *after* XCopyArea above to avoid following problem.
			 *
			 *  - : GraphicsExpose Area
			 *
			 * (X Window screen)  (ml_term_t)
			 * aaaaaaaaaa         aaaaaaaaaa
			 * bbbbbbbbbb         bbbbbbbbbb
			 * cccccccccc         cccccccccc
			 *   1||(CA)             1||
			 *    \/                  \/
			 * bbbbbbbbbb         bbbbbbbbbb
			 * cccccccccc         cccccccccc
			 * ----------         dddddddddd
			 *                       2||
			 *                        \/
			 * bbbbbbbbbb 3(GE)   cccccccccc
			 * cccccccccc <=====  dddddddddd
			 * eeeeeeeeee         eeeeeeeeee
			 *   4||(CA)
			 *    \/
			 * cccccccccc
			 * eeeeeeeeee
			 * eeeeeeeeee
			 */
			ev.xgraphicsexpose.x += (dst_x - src_x) ;
			ev.xgraphicsexpose.y += (dst_y - src_y) ;
		}
		x_window_receive_event( win , &ev) ;
	}

	win->wait_copy_area_response = 1 ;
}

static int
send_selection(
	x_window_t *  win ,
	XSelectionRequestEvent *  req_ev ,
	u_char *  sel_data ,
	size_t  sel_len ,
	Atom  sel_type ,
	int  sel_format
	)
{
	XEvent  res_ev ;

	res_ev.xselection.type = SelectionNotify ;
	res_ev.xselection.display = req_ev->display ;
	res_ev.xselection.requestor = req_ev->requestor ;
	res_ev.xselection.selection = req_ev->selection ;
	res_ev.xselection.target = req_ev->target ;
	res_ev.xselection.time = req_ev->time ;

	if( sel_data == NULL)
	{
		res_ev.xselection.property = None ;
	}
	else
	{
		if( req_ev->property == None)
		{
			/* An obsolete client may fill None as a property.
			 * Try to deal with them by using 'target' instead.
			 */
			req_ev->property = req_ev->target;
		}
		if( req_ev->property != None)
		{
			XChangeProperty( win->disp->display , req_ev->requestor ,
				req_ev->property , sel_type ,
				sel_format , PropModeReplace , sel_data , sel_len) ;
		}
		res_ev.xselection.property = req_ev->property ;
	}

	XSendEvent( win->disp->display , res_ev.xselection.requestor , False , 0 , &res_ev) ;

	return  1 ;
}

static int
right_shift(
	u_long  mask
	)
{
	int  shift = 0 ;
	int  count = 8 ;

	if( mask == 0)
	{
		return  0 ;
	}

	while((mask & 1) == 0)
	{
		mask >>= 1 ;
		shift ++ ;
	}

	while((mask & 1) == 1)
	{
		mask >>= 1 ;
		count -- ;
	}

	if( count > 0)
	{
		shift -= count ;
	}

	return  shift ;
}

static void
reset_input_focus(
	x_window_t *  win
	)
{
	u_int  count ;

	if( win->inputtable)
	{
		win->inputtable = -1 ;
	}
	else
	{
		win->inputtable = 0 ;
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		reset_input_focus( win->children[count]) ;
	}
}


#if  ! defined(NO_DYNAMIC_LOAD_TYPE)

static int
x_window_set_use_xft(
	x_window_t *  win ,
	int  use_xft
	)
{
	int (*func)( x_window_t * , int) ;

	if( ! ( func = x_load_type_xft_func( X_WINDOW_SET_TYPE)))
	{
		return  0 ;
	}

	return  (*func)( win , use_xft) ;
}

static int
x_window_xft_draw_string8(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	size_t  len
	)
{
	int (*func)( x_window_t * , x_font_t * , x_color_t * , int , int , u_char * , size_t) ;

	if( ! ( func = x_load_type_xft_func( X_WINDOW_DRAW_STRING8)))
	{
		return  0 ;
	}

	return  (*func)( win , font , fg_color , x , y , str , len) ;
}

static int
x_window_xft_draw_string32(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	/* FcChar32 */ u_int32_t *  str ,
	size_t  len
	)
{
	int (*func)( x_window_t * , x_font_t * , x_color_t * , int , int ,
		/* FcChar32 */ u_int32_t * , size_t) ;

	if( ! ( func = x_load_type_xft_func( X_WINDOW_DRAW_STRING32)))
	{
		return  0 ;
	}

	return  (*func)( win , font , fg_color , x , y , str , len) ;
}

static void
xft_set_clip(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	void (*func)( x_window_t * , int , int , u_int , u_int) ;

	if( ! ( func = x_load_type_xft_func( X_WINDOW_SET_CLIP)))
	{
		return ;
	}

	(*func)( win , x , y , width , height) ;
}

static void
xft_unset_clip(
	x_window_t *  win
	)
{
	void (*func)( x_window_t *) ;

	if( ! ( func = x_load_type_xft_func( X_WINDOW_UNSET_CLIP)))
	{
		return ;
	}

	(*func)( win) ;
}

static int
x_window_set_use_cairo(
	x_window_t *  win ,
	int  use_cairo
	)
{
	int (*func)( x_window_t * , int) ;

	if( ! ( func = x_load_type_cairo_func( X_WINDOW_SET_TYPE)))
	{
		return  0 ;
	}

	return  (*func)( win , use_cairo) ;
}

static int
x_window_cairo_draw_string8(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	size_t  len
	)
{
	int (*func)( x_window_t * , x_font_t * , x_color_t * , int , int , u_char * , size_t) ;

	if( ! ( func = x_load_type_cairo_func( X_WINDOW_DRAW_STRING8)))
	{
		return  0 ;
	}

	return  (*func)( win , font , fg_color , x , y , str , len) ;
}

static int
x_window_cairo_draw_string32(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	/* FcChar32 */ u_int32_t *  str ,
	size_t  len
	)
{
	int (*func)( x_window_t * , x_font_t * , x_color_t * , int , int ,
		/* FcChar32 */ u_int32_t * , size_t) ;

	if( ! ( func = x_load_type_cairo_func( X_WINDOW_DRAW_STRING32)))
	{
		return  0 ;
	}

	return  (*func)( win , font , fg_color , x , y , str , len) ;
}

static int
cairo_resize(
	x_window_t *  win
	)
{
	int (*func)( x_window_t *) ;

	if( ! ( func = x_load_type_cairo_func( X_WINDOW_RESIZE)))
	{
		return  0 ;
	}

	return  (*func)( win) ;
}

static void
cairo_set_clip(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	void (*func)( x_window_t * , int , int , u_int , u_int) ;

	if( ! ( func = x_load_type_cairo_func( X_WINDOW_SET_CLIP)))
	{
		return ;
	}

	(*func)( win , x , y , width , height) ;
}

static void
cairo_unset_clip(
	x_window_t *  win
	)
{
	void (*func)( x_window_t *) ;

	if( ! ( func = x_load_type_cairo_func( X_WINDOW_UNSET_CLIP)))
	{
		return ;
	}

	(*func)( win) ;
}

#else  /* NO_DYNAMIC_LOAD_TYPE */
#ifdef  USE_TYPE_XFT
int  x_window_set_use_xft( x_window_t *  win , int  use_xft) ;
int  x_window_xft_draw_string8( x_window_t *  win , x_font_t *  font , x_color_t *  fg_color ,
	int  x , int  y , u_char *  str , size_t  len) ;
int  x_window_xft_draw_string32( x_window_t *  win , x_font_t *  font , x_color_t *  fg_color ,
	int  x , int  y , u_int32_t *  str , size_t  len) ;
void  xft_set_clip( x_window_t *  win , int  x , int  y , u_int  width , u_int  height) ;
void  xft_unset_clip( x_window_t *  win) ;
#endif
#ifdef  USE_TYPE_CAIRO
int  x_window_set_use_cairo( x_window_t *  win , int  use_cairo) ;
int  x_window_cairo_draw_string8( x_window_t *  win , x_font_t *  font , x_color_t *  fg_color ,
	int  x , int  y , u_char *  str , size_t  len) ;
int  x_window_cairo_draw_string32( x_window_t *  win , x_font_t *  font , x_color_t *  fg_color ,
	int  x , int  y , u_int32_t *  str , size_t  len) ;
int  cairo_resize( x_window_t *  win) ;
void  cairo_set_clip( x_window_t *  win , int  x , int  y , u_int  width , u_int  height) ;
void  cairo_unset_clip( x_window_t *  win) ;
#endif
#endif /* NO_DYNAMIC_LOAD_TYPE */


/* --- global functions --- */

int
x_window_init(
	x_window_t *  win ,
	u_int  width ,
	u_int  height ,
	u_int  min_width ,	/* width_inc * 1 must be added to if width_inc > 0 */
	u_int  min_height ,	/* height_inc * 1 must be added to if height_inc > 0 */
	u_int  width_inc ,
	u_int  height_inc ,
	u_int  hmargin ,
	u_int  vmargin ,
	int  create_gc ,
	int  inputtable
	)
{
	memset( win , 0 , sizeof( x_window_t)) ;

	win->fg_color.pixel = 0xff000000 ;
	win->fg_color.alpha = 0xff ;
	memset( &win->bg_color , 0xff , sizeof(win->bg_color)) ;

	win->event_mask = ExposureMask | FocusChangeMask | PropertyChangeMask ;

	/* If wall picture is set, scrollable will be 0. */
	win->is_scrollable = 1 ;

#if  0
	/*
	 * is_focus member shoule be 0 by default in order to call
	 * XSetICFocus(x_xic_set_focus) in startup FocusIn event.
	 * If XSetICFocus() is not called, KeyPress event is discarded
	 * in XFilterEvent.
	 */
	win->is_focused = 0 ;
#endif

	win->inputtable = inputtable ;

	/* This flag will map window automatically in x_window_show() */
	win->is_mapped = 1 ;

	win->create_gc = create_gc ;

	win->width = width ;
	win->height = height ;
	win->min_width = min_width ;
	win->min_height = min_height ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;
	win->hmargin = hmargin ;
	win->vmargin = vmargin ;

	win->prev_clicked_button = -1 ;

	win->app_name = "mlterm" ;	/* Can be changed in x_display_show_root(). */

	return	1 ;
}

int
x_window_final(
	x_window_t *  win
	)
{
	u_int  count ;

#ifdef  DEBUG
	kik_debug_printf( "[deleting child windows]\n") ;
	x_window_dump_children( win) ;
#endif

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_final( win->children[count]) ;
	}

	free( win->children) ;

	if( win->my_window)
	{
		x_display_clear_selection( win->disp , win) ;

		x_xic_deactivate( win) ;

		/* Delete cairo/xft. */
		x_window_set_type_engine( win , TYPE_XCORE) ;

		XDestroyWindow( win->disp->display , win->my_window) ;

		if( win->create_gc)
		{
			x_gc_delete( win->gc) ;
		}
	}
	else
	{
		/* x_window_show() is not called yet. */
	}

	if( win->window_finalized)
	{
		(*win->window_finalized)( win) ;
	}

	return  1 ;
}


/*
 * Call this function in window_realized event at first.
 */
int
x_window_set_type_engine(
	x_window_t *  win ,
	x_type_engine_t  type_engine
	)
{
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	if( ( win->xft_draw != NULL) != ( type_engine == TYPE_XFT))
	{
		x_window_set_use_xft( win , ( type_engine == TYPE_XFT)) ;
	}
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	if( ( win->cairo_draw != NULL) != ( type_engine == TYPE_CAIRO))
	{
		x_window_set_use_cairo( win , ( type_engine == TYPE_CAIRO)) ;
	}
#endif

	return  1 ;
}

int
x_window_init_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
	if( win->my_window)
	{
		/*
		 * Don't use this function after x_window_show().
		 * After x_window_show(), use x_window_{add|remove}_event_mask().
		 */

		return  0 ;
	}

#if  0
	if( event_mask & ButtonMotionMask)
	{
		event_mask &= ~ButtonMotionMask ;
		event_mask |= ( Button1MotionMask | Button2MotionMask |
			Button3MotionMask | Button4MotionMask | Button5MotionMask) ;
	}
#endif

	win->event_mask |= event_mask ;

	return  1 ;
}

int
x_window_add_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
#if  0
	if( event_mask & ButtonMotionMask)
	{
		event_mask &= ~ButtonMotionMask ;
		event_mask |= ( Button1MotionMask | Button2MotionMask |
			Button3MotionMask | Button4MotionMask | Button5MotionMask) ;
	}
#endif

	win->event_mask |= event_mask ;

	XSelectInput( win->disp->display , win->my_window , win->event_mask) ;

	return  1 ;
}

int
x_window_remove_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
#if  0
	if( event_mask & ButtonMotionMask)
	{
		event_mask &= ~ButtonMotionMask ;
		event_mask |= ( Button1MotionMask | Button2MotionMask |
			Button3MotionMask | Button4MotionMask | Button5MotionMask) ;
	}
#endif

	win->event_mask &= ~event_mask ;

	XSelectInput( win->disp->display , win->my_window , win->event_mask) ;

	return  1 ;
}

int
x_window_ungrab_pointer(
	x_window_t *  win
	)
{
	XUngrabPointer( win->disp->display , CurrentTime) ;
	
	return  0 ;
}

int
x_window_set_wall_picture(
	x_window_t *  win ,
	Pixmap  pic ,
	int  do_expose
	)
{
	u_int  count ;

	if( win->is_transparent)
	{
		/*
		 * unset transparent before setting wall picture !
		 */

		return  0 ;
	}

	XSetWindowBackgroundPixmap( win->disp->display , win->my_window , pic) ;
	win->wall_picture_is_set = 1 ;
	win->is_scrollable = 0 ;

	if( do_expose)
	{
		clear_margin_area( win) ;

		if( win->window_exposed)
		{
			(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
		}
	#if  0
		else
		{
			x_window_clear_all( win) ;
		}
	#endif
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		x_window_set_wall_picture( win->children[count] , ParentRelative , do_expose) ;
	}

	return  1 ;
}

int
x_window_unset_wall_picture(
	x_window_t *  win ,
	int  do_expose
	)
{
	u_int  count ;

	/*
	 * win->wall_picture_is_set == 0 doesn't mean that following codes
	 * to disable wall picture is already processed.
	 * e.g.) If x_window_unset_transparent() is called after
	 * x_window_set_transparent() before my_window is created,
	 * XSetWindowBackground() which is not called in x_window_set_bg_color()
	 * (because is_transparent flag is set by x_window_set_transparent())
	 * is never called in startup by checking win->wall_picture_is_set as follows.
	 */
#if  0
	if( ! win->wall_picture_is_set)
	{
		/* already unset */

		return  1 ;
	}
#endif

	if( win->is_transparent)
	{
		/*
		 * transparent background is not a wall picture :)
		 * this case is regarded as not using a wall picture.
		 */

		return  1 ;
	}

	XSetWindowBackgroundPixmap( win->disp->display , win->my_window , None) ;
	XSetWindowBackground( win->disp->display , win->my_window , win->bg_color.pixel) ;

	win->wall_picture_is_set = 0 ;
	win->is_scrollable = 1 ;

	if( do_expose)
	{
		clear_margin_area( win) ;

		if( win->window_exposed)
		{
			(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
		}
	#if  0
		else
		{
			x_window_clear_all( win) ;
		}
	#endif
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		x_window_unset_wall_picture( win->children[count] , do_expose) ;
	}

	return  1 ;
}

/*
 * This function is possible to be called before my_window is created.
 * (because x_screen_t doesn't contain transparent flag.)
 */
int
x_window_set_transparent(
	x_window_t *  win ,		/* Transparency is applied to all children recursively */
	x_picture_modifier_t *  pic_mod
	)
{
	u_int  count ;

	win->pic_mod = pic_mod ;

	if( win->my_window == None)
	{
		/*
		 * If Window is not still created , actual drawing is delayed and
		 * ReparentNotify event will do transparent processing automatically after
		 * x_window_show().
		 */

		win->is_transparent = 1 ;
	}
	else
	{
		set_transparent( win) ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_set_transparent( win->children[count] , win->pic_mod) ;
	}

	return  1 ;
}

/*
 * This function is possible to be called before my_window is created.
 * (because x_screen_t doesn't contain transparent flag.)
 */
int
x_window_unset_transparent(
	x_window_t *  win
	)
{
	u_int  count ;

	if( win->my_window == None)
	{
		win->is_transparent = 0 ;
	}
	else if( win->is_transparent)
	{
		unset_transparent( win) ;

		clear_margin_area( win) ;

		if( win->window_exposed)
		{
			(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
		}
	#if  0
		else
		{
			x_window_clear_all( win) ;
		}
	#endif
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_unset_transparent( win->children[count]) ;
	}

	return  1 ;
}

/*
 * Cursor is not changeable after x_window_show().
 */
int
x_window_set_cursor(
	x_window_t *  win ,
	u_int  cursor_shape
	)
{
	if( win->my_window == None)
	{
		win->cursor_shape = cursor_shape ;
	}
	else
	{
		Cursor  cursor ;

		if( ( cursor = x_display_get_cursor( win->disp ,
				( win->cursor_shape = cursor_shape))))
		{
			XDefineCursor( win->disp->display , win->my_window , cursor) ;
		}
	}

	return  1 ;
}

int
x_window_set_fg_color(
	x_window_t *  win ,
	x_color_t *  fg_color
	)
{
	if( win->fg_color.pixel == fg_color->pixel)
	{
		return  0 ;
	}
	
	win->fg_color = *fg_color ;

	return  1 ;
}

int
x_window_set_bg_color(
	x_window_t *  win ,
	x_color_t *  bg_color
	)
{
	if( win->bg_color.pixel == bg_color->pixel)
	{
		return  0 ;
	}
	
	win->bg_color = *bg_color ;
	
	if( ! win->is_transparent && ! win->wall_picture_is_set)
	{
		XSetWindowBackground( win->disp->display , win->my_window , win->bg_color.pixel) ;

		clear_margin_area( win) ;
	}

	return  1 ;
}

int
x_window_add_child(
	x_window_t *  win ,
	x_window_t *  child ,
	int  x ,
	int  y ,
	int  map
	)
{
	void *  p ;

	if( ( p = realloc( win->children , sizeof( *win->children) * (win->num_of_children + 1)))
		== NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif

		return  0 ;
	}

	win->children = p ;

	child->parent = win ;
	child->x = x + win->hmargin ;
	child->y = y + win->vmargin ;

	if( ! ( child->is_mapped = map) && child->inputtable > 0)
	{
		child->inputtable = -1 ;
	}

	win->children[ win->num_of_children ++] = child ;

	return  1 ;
}

int
x_window_remove_child(
	x_window_t *  win ,
	x_window_t *  child
	)
{
	u_int  count ;

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		if( win->children[count] == child)
		{
			child->parent = NULL ;
			win->children[count] = win->children[--win->num_of_children] ;

			return  1 ;
		}
	}

	return  0 ;
}

x_window_t *
x_get_root_window(
	x_window_t *  win
	)
{
	while( win->parent != NULL)
	{
		win = win->parent ;
	}

	return  win ;
}

GC
x_window_get_fg_gc(
	x_window_t *  win
	)
{
	/* Reset */
	restore_fg_color( win) ;

#if  0
	restore_bg_color( win) ;
#endif

	return  win->gc->gc ;
}

GC
x_window_get_bg_gc(
	x_window_t *  win
	)
{
	x_gc_set_fg_color((win)->gc,(win)->bg_color.pixel) ;

#if  0
	x_gc_set_bg_color((win)->gc,(win)->fg_color.pixel) ;
#endif

	return  win->gc->gc ;
}

int
x_window_show(
	x_window_t *  win ,
	int  hint	/* If win->parent(_window) is None,
			   specify XValue|YValue to localte window at win->x/win->y. */
	)
{
	u_int  count ;
	XSetWindowAttributes  s_attr ;

	if( win->my_window)
	{
		/* already shown */

		return  0 ;
	}

	if( win->parent)
	{
		win->disp = win->parent->disp ;
		win->gc = win->parent->gc ;
		win->parent_window = win->parent->my_window ;
	}
	
	if( hint & XNegative)
	{
		win->x += (win->disp->width - ACTUAL_WIDTH(win)) ;
	}

	if( hint & YNegative)
	{
		win->y += (win->disp->height - ACTUAL_HEIGHT(win)) ;
	}

	s_attr.background_pixel = win->bg_color.pixel ;
	s_attr.border_pixel = win->fg_color.pixel ;
	s_attr.colormap = win->disp->colormap ;
#if  1
	win->my_window = XCreateWindow(
				win->disp->display , win->parent_window ,
				win->x , win->y , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				0 , win->disp->depth , InputOutput , win->disp->visual ,
				CWBackPixel | CWBorderPixel | CWColormap , &s_attr) ;
#else
	win->my_window = XCreateSimpleWindow(
				win->disp->display , win->parent_window ,
				win->x , win->y , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				0 , win->fg_color.pixel , win->bg_color.pixel) ;
#endif

	if( win->create_gc)
	{
		x_gc_t *  gc ;

		if( ( gc = x_gc_new( win->disp->display , win->my_window)) == NULL)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " x_gc_new failed.\n") ;
		#endif
			win->create_gc = 0 ;
		}
		else
		{
			win->gc = gc ;
		}
	}

	if( win->cursor_shape)
	{
		Cursor  cursor ;

		if( ( cursor = x_display_get_cursor( win->disp , win->cursor_shape)))
		{
			XDefineCursor( win->disp->display , win->my_window , cursor) ;
		}
	}

	/* Don't use win->parent here in case mlterm works as libvte. */
	if( PARENT_WINDOWID_IS_TOP(win))
	{
		/* Root window */

		XSizeHints  size_hints ;
		XClassHint  class_hint ;
		XWMHints  wm_hints ;
		int  argc = 1 ;
		char *  argv[] = { "mlterm" , NULL , } ;
		Atom  protocols[2] ;

		win->event_mask |= StructureNotifyMask ;

		/*
		 * XXX
		 * x/y/width/height are obsoleted. (see XSizeHints(3))
		 */
		size_hints.x = win->x ;
		size_hints.y = win->y ;
		size_hints.width = ACTUAL_WIDTH(win) ;
		size_hints.height = ACTUAL_HEIGHT(win) ;

		size_hints.width_inc = total_width_inc( win) ;
		size_hints.height_inc = total_height_inc( win) ;
		size_hints.min_width = total_min_width( win) + size_hints.width_inc ;
		size_hints.min_height = total_min_height( win) + size_hints.height_inc ;
		size_hints.base_width = size_hints.min_width > size_hints.width_inc ?
					size_hints.min_width - size_hints.width_inc : 0 ;
		size_hints.base_height = size_hints.min_height > size_hints.height_inc ?
					size_hints.min_height - size_hints.height_inc : 0 ;

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG
			" Size hints => w %d h %d wi %d hi %d mw %d mh %d bw %d bh %d\n" ,
			size_hints.width , size_hints.height , size_hints.width_inc ,
			size_hints.height_inc , size_hints.min_width , size_hints.min_height ,
			size_hints.base_width , size_hints.base_height) ;
	#endif

		if( hint & XNegative)
		{
			if( hint & YNegative)
			{
				size_hints.win_gravity = SouthEastGravity ;
			}
			else
			{
				size_hints.win_gravity = NorthEastGravity ;
			}
		}
		else
		{
			if( hint & YNegative)
			{
				size_hints.win_gravity = SouthWestGravity ;
			}
			else
			{
				size_hints.win_gravity = NorthWestGravity ;
			}
		}

		size_hints.flags = PSize | PMinSize | PResizeInc | PBaseSize | PWinGravity ;

		if( hint & (XValue | YValue))
		{
			size_hints.flags |= PPosition ;
			size_hints.flags |= USPosition ;
		}

		class_hint.res_name = win->app_name ;
		class_hint.res_class = win->app_name ;

		wm_hints.initial_state = NormalState ;	/* or IconicState */
		wm_hints.input = True ;			/* wants FocusIn/FocusOut */
		wm_hints.window_group = reset_client_leader( win) ;
		wm_hints.flags = StateHint | InputHint | WindowGroupHint ;
	#if  0
		kik_debug_printf( KIK_DEBUG_TAG " Group leader -> %x\n" , wm_hints.window_group) ;
	#endif
	
		/* notify to window manager */
	#if  1
		XmbSetWMProperties( win->disp->display , win->my_window , win->app_name ,
			win->app_name , argv , argc , &size_hints , &wm_hints , &class_hint) ;
	#else
		XmbSetWMProperties( win->disp->display , win->my_window , win->app_name ,
			win->app_name , argv , argc , &size_hints , &wm_hints , NULL) ;
	#endif

		protocols[0] = XA_DELETE_WINDOW(win->disp->display) ;
		protocols[1] = XA_TAKE_FOCUS(win->disp->display) ;

		XSetWMProtocols( win->disp->display , win->my_window , protocols , 2) ;
	}

	if( win->parent && ! win->parent->is_transparent &&
	    win->parent->wall_picture_is_set)
	{
		x_window_set_wall_picture( win , ParentRelative , 0) ;
	}

	/*
	 * This should be called after Window Manager settings, because
	 * x_set_{window|icon}_name() can be called in win->window_realized().
	 */
	if( win->window_realized)
	{
		(*win->window_realized)( win) ;
	}

	XSelectInput( win->disp->display , win->my_window , win->event_mask) ;

#if  0
	{
		char *  locale ;

		if( ( locale = kik_get_locale()))
		{
			XChangeProperty( win->disp->display , win->my_window ,
				XInternAtom( win->disp->display , "WM_LOCALE_NAME" , False) ,
				XA_STRING , 8 , PropModeReplace ,
				locale , strlen( locale)) ;
		}
	}
#endif

	/*
	 * showing child windows.
	 */

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_show( win->children[count] , 0) ;
	}

	/*
	 * really visualized.
	 */

	if( win->is_mapped)
	{
		XMapWindow( win->disp->display , win->my_window) ;

		if( win->inputtable > 0)
		{
			reset_input_focus( x_get_root_window( win)) ;
			win->inputtable = 1 ;
		}

	#if  0
		x_window_clear_all( win) ;
	#endif
	}

	return  1 ;
}

int
x_window_map(
	x_window_t *  win
	)
{
	if( win->is_mapped)
	{
		return  1 ;
	}

	XMapWindow( win->disp->display , win->my_window) ;
	win->is_mapped = 1 ;

	return  1 ;
}

int
x_window_unmap(
	x_window_t *  win
	)
{
	if( ! win->is_mapped)
	{
		return  1 ;
	}

	XUnmapWindow( win->disp->display , win->my_window) ;
	win->is_mapped = 0 ;

	return  1 ;
}

int
x_window_resize(
	x_window_t *  win ,
	u_int  width ,			/* excluding margin */
	u_int  height ,			/* excluding margin */
	x_resize_flag_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	if( win->width == width && win->height == height)
	{
		return  0 ;
	}

	/* Max width of each window is DisplayWidth(). */
	if( (flag & LIMIT_RESIZE) && win->disp->width < width)
	{
		win->width = win->disp->width - win->hmargin * 2 ;
	}
	else
	{
		win->width = width ;
	}

	/* Max height of each window is DisplayHeight(). */
	if( (flag & LIMIT_RESIZE) && win->disp->height < height)
	{
		win->height = win->disp->height - win->vmargin * 2 ;
	}
	else
	{
		win->height = height ;
	}

	if( (flag & NOTIFY_TO_PARENT) && win->parent && win->parent->child_window_resized)
	{
		(*win->parent->child_window_resized)( win->parent , win) ;
	}

	XResizeWindow( win->disp->display , win->my_window ,
		ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;

	if( (flag & NOTIFY_TO_MYSELF) && win->window_resized)
	{
		(*win->window_resized)( win) ;
	}

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	if( win->cairo_draw)
	{
		cairo_resize( win) ;
	}
#endif

	if( ! win->configure_root && ! (flag & NOTIFY_TO_PARENT) && win->parent)
	{
		notify_configure_to_children( win) ;
	}

	return  1 ;
}

/*
 * !! Notice !!
 * This function is not recommended.
 * Use x_window_resize if at all possible.
 */
int
x_window_resize_with_margin(
	x_window_t *  win ,
	u_int  width ,
	u_int  height ,
	x_resize_flag_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	u_int  min_width ;
	u_int  min_height ;

	min_width = total_min_width( win) ;
	min_height = total_min_height( win) ;
	
	return  x_window_resize( win ,
			width <= min_width ? min_width : width - win->hmargin * 2 ,
			height <= min_height ? min_height : height - win->vmargin * 2 , flag) ;
}

int
x_window_set_normal_hints(
	x_window_t *  win ,
	u_int  min_width ,
	u_int  min_height ,
	u_int  width_inc ,
	u_int  height_inc
	)
{
	XSizeHints  size_hints ;
	x_window_t *  root ;

	win->min_width = min_width ;
	win->min_height = min_height  ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;

	root = x_get_root_window(win) ;

	/*
	 * these hints must be set at the same time !
	 */
	size_hints.width_inc = total_width_inc( root) ;
	size_hints.height_inc = total_height_inc( root) ;
	size_hints.min_width = total_min_width( root) + size_hints.width_inc ;
	size_hints.min_height = total_min_height( root) + size_hints.height_inc ;
	size_hints.base_width = size_hints.min_width - size_hints.width_inc ;
	size_hints.base_height = size_hints.min_height - size_hints.height_inc ;
	size_hints.flags = PMinSize | PResizeInc | PBaseSize ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG
		" Size hints => wi %u hi %u mw %u mh %u bw %u bh %u\n" ,
		size_hints.width_inc , size_hints.height_inc ,
		size_hints.min_width , size_hints.min_height ,
		size_hints.base_width , size_hints.base_height) ;
#endif

	XSetWMNormalHints( root->disp->display , root->my_window , &size_hints) ;

	return  1 ;
}

int
x_window_set_override_redirect(
	x_window_t *  win ,
	int  flag
	)
{
	x_window_t *  root ;
	XSetWindowAttributes  s_attr ;
	XWindowAttributes  g_attr ;

	root = x_get_root_window(win) ;

	XGetWindowAttributes( root->disp->display , root->my_window , &g_attr) ;
	if( flag)
	{
		s_attr.override_redirect = True ;
	}
	else
	{
		s_attr.override_redirect = False ;
	}

	if( g_attr.override_redirect == s_attr.override_redirect)
	{
		return  1 ;
	}

	XChangeWindowAttributes( root->disp->display , root->my_window ,
				 CWOverrideRedirect , &s_attr) ;

	if( g_attr.map_state != IsUnmapped)
	{
		XUnmapWindow( root->disp->display , root->my_window) ;
		XMapWindow( root->disp->display , root->my_window) ;
	}

	reset_input_focus( root) ;
	/* XXX Always focused not to execute XSetInputFocus(). */
	win->inputtable = win->is_focused = 1 ;

	return  1 ;
}

int
x_window_set_borderless_flag(
	x_window_t *  win ,
	int  flag
	)
{
	/*
	 * XXX
	 * Support borderless with _MOTIF_WM_HINTS.
	 * (See Eterm/src/windows.c)
	 */

	x_window_t *  root ;
	Atom  atom ;

	root = x_get_root_window(win) ;

#ifdef  __DEBUG
	kik_debug_printf( "MOTIF_WM_HINTS: %x\nMOTIF_WM_INFO: %x\n" ,
		XInternAtom( root->disp->display , "_MOTIF_WM_HINTS" , True) ,
		XInternAtom( root->disp->display , "_MOTIF_WM_INFO" , True)) ;
#endif

	if( ( atom = XA_MWM_HINTS(root->disp->display)) != None)
	{
		if( flag)
		{
			MWMHints_t  mwmhints = { MWM_HINTS_DECORATIONS, 0, 0, 0, 0 } ;

			XChangeProperty( root->disp->display, root->my_window, atom , atom , 32,
				PropModeReplace, (u_char *)&mwmhints,
				sizeof(MWMHints_t)/sizeof(u_long)) ;
		}
		else
		{
			XDeleteProperty( root->disp->display, root->my_window, atom) ;
		}
	}
	else
	{
		/* fall back to override redirect */
		x_window_set_override_redirect( win , flag) ;
	}

	return  1 ;
}

int
x_window_move(
	x_window_t *  win ,
	int  x ,
	int  y
	)
{
	if( win->parent)
	{
		x += win->parent->hmargin ;
		y += win->parent->vmargin ;
	}

	if( win->x == x && win->y == y)
	{
		return  0 ;
	}

	win->x = x ;
	win->y = y ;

	XMoveWindow( win->disp->display , win->my_window , win->x , win->y) ;

	if( ! win->configure_root && win->parent)
	{
		notify_configure_to_children( win) ;
	}

	return  1 ;
}

int
x_window_clear(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
#ifdef  AUTO_CLEAR_MARGIN
	if( x + width >= win->width)
	{
		/* Clearing margin area */
		width += win->hmargin ;
	}

	if( x > 0)
#endif
	{
		x += win->hmargin ;
	}
#ifdef  AUTO_CLEAR_MARGIN
	else
	{
		/* Clearing margin area */
		width += win->hmargin ;
	}

	if( y + height >= win->height)
	{
		/* Clearing margin area */
		height += win->vmargin ;
	}

	if( y > 0)
#endif
	{
		y += win->vmargin ;
	}
#ifdef  AUTO_CLEAR_MARGIN
	else
	{
		/* Clearing margin area */
		height += win->vmargin ;
	}
#endif

	XClearArea( win->disp->display , win->my_window , x , y , width , height , False) ;

	return  1 ;
}

int
x_window_clear_all(
	x_window_t *  win
	)
{
	return  x_window_clear( win , 0 , 0 , win->width , win->height) ;
}

int
x_window_fill(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	restore_fg_color( win) ;

	XFillRectangle( win->disp->display , win->my_window , win->gc->gc ,
		x + win->hmargin , y + win->vmargin , width , height) ;

	return  1 ;
}

int
x_window_fill_with(
	x_window_t *  win ,
	x_color_t *  color ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	x_gc_set_fg_color( win->gc, color->pixel) ;

	XFillRectangle( win->disp->display , win->my_window , win->gc->gc ,
		x + win->hmargin , y + win->vmargin , width , height) ;

	return  1 ;
}

int
x_window_blank(
	x_window_t *  win
	)
{
	restore_fg_color( win) ;

	XFillRectangle( win->disp->display , win->my_window , win->gc->gc ,
		win->hmargin , win->vmargin ,
		win->width - RIGHT_MARGIN(win) ,
		win->height - BOTTOM_MARGIN(win)) ;

	return  1 ;
}

#if  0
/*
 * XXX
 * At the present time , not used and not maintained.
 */
int
x_window_blank_with(
	x_window_t *  win ,
	x_color_t *  color
	)
{
	x_gc_set_fg_color( win->gc, color->pixel) ;

	XFillRectangle( win->disp->display , win->my_window , win->gc->gc ,
		win->hmargin , win->vmargin , win->width , win->height) ;

	return  1 ;
}
#endif

int
x_window_update(
	x_window_t *  win ,
	int  flag
	)
{
	if( win->update_window)
	{
		(*win->update_window)( win, flag) ;

		if( win->gc->mask)
		{
			/*
			 * x_window_copy_area() can set win->gc->mask.
			 * It can cause unexpected drawing in x_animate_inline_pictures().
			 */
			XSetClipMask( win->disp->display , win->gc->gc , None) ;
			win->gc->mask = None ;
		}
	}

	return  1 ;
}

int
x_window_update_all(
	x_window_t *  win
	)
{
	u_int  count ;

	clear_margin_area( win) ;

	if( win->window_exposed)
	{
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_update_all( win->children[count]) ;
	}

	return  1 ;
}

void
x_window_idling(
	x_window_t *  win
	)
{
	u_int  count ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_idling( win->children[count]) ;
	}

#ifdef  __DEBUG
	if( win->button_is_pressing)
	{
		kik_debug_printf( KIK_DEBUG_TAG " button is pressing...\n") ;
	}
#endif

	if( win->button_is_pressing && win->button_press_continued)
	{
		(*win->button_press_continued)( win , &win->prev_button_press_event) ;
	}
	else if( win->idling)
	{
		(*win->idling)( win) ;
	}
}

/*
 * Return value: 0 => different window.
 *               1 => finished processing.
 */
int
x_window_receive_event(
	x_window_t *   win ,
	XEvent *  event
	)
{
	u_int  count ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( x_window_receive_event( win->children[count] , event))
		{
			return  1 ;
		}
	}

	if( win->my_window != event->xany.window)
	{
		/*
		 * XXX
		 * if some window invokes xim window open event and it doesn't have any xic ,
		 * no xim window will be opened at XFilterEvent() in x_display_receive_next_event().
		 * but it is desired to open xim window of x_screen when its event is invoked
		 * on scrollbar or title bar.
		 * this hack enables it , but this way won't deal with the case that multiple
		 * xics exist.
		 */
		if( win->xic)
		{
			if( is_in_the_same_window_family( win , event->xany.window) &&
				XFilterEvent( event , win->my_window))
			{
				return  1 ;
			}
		}

		if( event->type == PropertyNotify &&
			win == x_get_root_window( win) &&
			(event->xproperty.atom == XA_XSETROOT_ID(win->disp->display) ||
			 event->xproperty.atom == XA_XROOTPMAP_ID(win->disp->display)) )
		{
			/*
			 * Background image is changed.
			 * (notify_property_to_children() is called here because
			 * event->xproperty.window is not win->my_window.)
			 *
			 * twm => XA_XSETROOT_ID
			 * englightment => XA_XROOTPMAP_ID
			 */
			 
			notify_property_to_children( win) ;

			return  1 ;
		}

		if( event->type == MappingNotify && event->xmapping.request != MappingPointer)
		{
			if( win->disp)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " MappingNotify serial #%d\n",
					event->xmapping.serial) ;
			#endif
				XRefreshKeyboardMapping( &(event->xmapping));
				x_display_update_modifier_mapping( win->disp,
					event->xmapping.serial) ;
				/* have to process only once */
				return 1 ;
			}

			if( win->mapping_notify)
			{
				(*win->mapping_notify)( win);
			}
		}

		return  0 ;
	}

#ifndef  DISABLE_XDND
	if( x_dnd_filter_event( event, win))
	{
		/* event was consumed by xdnd handlers */
		return 1 ;
	}
#endif

	if( event->type == KeyPress)
	{
		if( win->key_pressed)
		{
			(*win->key_pressed)( win , &event->xkey) ;
		}
	}
	else if( event->type == FocusIn)
	{
	#ifdef  __DEBUG
		kik_debug_printf( "FOCUS IN %p\n" , event->xany.window) ;
	#endif

		urgent_bell( win , 0) ;

		/*
		 * Cygwin/X can send FocusIn/FocusOut events not to top windows
		 * but to child ones in changing window focus, so don't encircle
		 * notify_focus_{in|out}_to_children with if(!win->parent).
		 */
		notify_focus_in_to_children( win) ;
	}
	else if( event->type == FocusOut)
	{
	#ifdef  __DEBUG
		kik_debug_printf( "FOCUS OUT %p\n" , event->xany.window) ;
	#endif

		/*
		 * Cygwin/X can send FocusIn/FocusOut events not to top windows
		 * but to child ones in changing window focus, so don't encircle
		 * notify_focus_{in|out}_to_children with if(!win->parent).
		 */
		notify_focus_out_to_children( win) ;
	}
	else if( event->type == MotionNotify)
	{
		XEvent  ahead ;
		
		while( XEventsQueued(win->disp->display , QueuedAfterReading))
		{
			XPeekEvent( win->disp->display , &ahead) ;

			if( ahead.type != MotionNotify ||
				ahead.xmotion.window != event->xmotion.window)
			{
				break ;
			}

			XNextEvent( win->disp->display , event) ;
		}

		/*
		 * If ButtonReleaseMask is not set to win->event_mask, win->button_is_pressing
		 * is always 0. So, event->xmotion.state is also checked.
		 */
		if( win->button_is_pressing ||
			( event->xmotion.state & ButtonMask))
		{
			if( win->button_motion)
			{
				event->xmotion.x -= win->hmargin ;
				event->xmotion.y -= win->vmargin ;

				(*win->button_motion)( win , &event->xmotion) ;
			}

			/* following button motion ... */

			win->prev_button_press_event.x = event->xmotion.x ;
			win->prev_button_press_event.y = event->xmotion.y ;
			win->prev_button_press_event.time = event->xmotion.time ;
		}
		else if( win->pointer_motion)
		{
			event->xmotion.x -= win->hmargin ;
			event->xmotion.y -= win->vmargin ;

			(*win->pointer_motion)( win , &event->xmotion) ;
		}
	}
	else if( event->type == ButtonRelease)
	{
		if( win->button_released)
		{
			event->xbutton.x -= win->hmargin ;
			event->xbutton.y -= win->vmargin ;

			(*win->button_released)( win , &event->xbutton) ;
		}
		
		win->button_is_pressing = 0 ;
	}
	else if( event->type == ButtonPress)
	{
		if( win->button_pressed)
		{
			event->xbutton.x -= win->hmargin ;
			event->xbutton.y -= win->vmargin ;

			if( win->click_num == MAX_CLICK)
			{
				win->click_num = 0 ;
			}

			if( win->prev_clicked_time + click_interval >= event->xbutton.time &&
				event->xbutton.button == win->prev_clicked_button)
			{
				win->click_num ++ ;
				win->prev_clicked_time = event->xbutton.time ;
			}
			else
			{
				win->click_num = 1 ;
				win->prev_clicked_time = event->xbutton.time ;
				win->prev_clicked_button = event->xbutton.button ;
			}

			(*win->button_pressed)( win , &event->xbutton , win->click_num) ;
		}
		
		if( win->event_mask & ButtonReleaseMask)
		{
			/*
			 * if ButtonReleaseMask is not set and x_window_t doesn't receive
			 * ButtonRelease event , button_is_pressing flag must never be set ,
			 * since once it is set , it will never unset.
			 */
			win->button_is_pressing = 1 ;
			win->prev_button_press_event = event->xbutton ;
		}

		/* XXX Note that win->is_focused is always true on override redirect mode. */
		if( ! win->is_focused && win->inputtable && event->xbutton.button == Button1 &&
		    ! event->xbutton.state)
		{
			x_window_set_input_focus( win) ;
		}
	}
	else if( event->type == NoExpose)
	{
		win->wait_copy_area_response = 0 ;
	}
	else if( event->type == Expose || event->type == GraphicsExpose)
	{
		XEvent  next_ev ;
		int  x ;
		int  y ;
		u_int  width ;
		u_int  height ;
		int  margin_area_exposed ;
	#ifdef  __DEBUG
		int  nskip = 0 ;
	#endif

		/* Optimize redrawing. */
		while( XCheckTypedWindowEvent( win->disp->display , win->my_window ,
			event->type , &next_ev))
		{
			XEvent  ev ;
			int  diff ;

			ev = *event ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" x %d y %d w %d h %d + x %d y %d w %d h %d ->" ,
				ev.xexpose.x , ev.xexpose.y ,
				ev.xexpose.width , ev.xexpose.height ,
				next_ev.xexpose.x , next_ev.xexpose.y ,
				next_ev.xexpose.width , next_ev.xexpose.height) ;
		#endif

			if( ( diff = ev.xexpose.x - next_ev.xexpose.x) > 0)
			{
				ev.xexpose.width += diff ;
				ev.xexpose.x = next_ev.xexpose.x ;
			}

			if( ( diff = next_ev.xexpose.x + next_ev.xexpose.width -
			              ev.xexpose.x - ev.xexpose.width) > 0)
			{
				ev.xexpose.width += diff ;
			}

			if( ( diff = ev.xexpose.y - next_ev.xexpose.y) > 0)
			{
				ev.xexpose.height += diff ;
				ev.xexpose.y = next_ev.xexpose.y ;
			}

			if( ( diff = next_ev.xexpose.y + next_ev.xexpose.height -
			             ev.xexpose.y - ev.xexpose.height) > 0)
			{
				ev.xexpose.height += diff ;
			}

		#ifdef  __DEBUG
			kik_msg_printf( " x %d y %d w %d h %d\n" ,
				ev.xexpose.x , ev.xexpose.y ,
				ev.xexpose.width , ev.xexpose.height) ;
		#endif

			/* Minimum character size is regarded as w5 x h10. */
			if( ( ev.xexpose.width * ev.xexpose.height) / 4  >=
			    ( K_MAX( event->xexpose.width , 5) *
			      K_MAX( event->xexpose.height , 10) +
			      K_MAX( next_ev.xexpose.width , 5) *
			      K_MAX( next_ev.xexpose.height , 10)) / 3)
			{
				/* Redrawing area is increased over 33.3% by this combination. */

			#ifdef  __DEBUG
				kik_msg_printf( "=> Discard combination of XExposeEvents "
						"because of inefficiency.\n") ;
			#endif

				XPutBackEvent( win->disp->display , &next_ev) ;

				break ;
			}
			else
			{
			#ifdef  __DEBUG
				nskip ++ ;
			#endif

				*event = ev ;
			}
		}

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " skipped %d expose events.\n" , nskip) ;
	#endif

		margin_area_exposed = 0 ;
		
		if( event->xexpose.x < win->hmargin)
		{
			margin_area_exposed = 1 ;
			x = 0 ;

			if( x + event->xexpose.width > win->width)
			{
				width = win->width ;
			}
			else if( event->xexpose.width < (win->hmargin - event->xexpose.x))
			{
				width = 0 ;
			}
			else
			{
				width = event->xexpose.width - (win->hmargin - event->xexpose.x) ;
			}
		}
		else
		{
			x = event->xexpose.x - win->hmargin ;

			if( x + event->xexpose.width > win->width)
			{
				margin_area_exposed = 1 ;
				width = win->width - x ;
			}
			else
			{
				width = event->xexpose.width ;
			}
		}

		if( event->xexpose.y < win->vmargin)
		{
			margin_area_exposed = 1 ;
			y = 0 ;

			if( y + event->xexpose.height > win->height)
			{
				height = win->height ;
			}
			else if( event->xexpose.height < (win->vmargin - event->xexpose.y))
			{
				height = 0 ;
			}
			else
			{
				height = event->xexpose.height - (win->vmargin - event->xexpose.y) ;
			}
		}
		else
		{
			y = event->xexpose.y - win->vmargin ;

			if( y + event->xexpose.height > win->height)
			{
				margin_area_exposed = 1 ;
				height = win->height - y ;
			}
			else
			{
				height = event->xexpose.height ;
			}
		}

		/*
		 * It is desirable to set win->is_scrollable = 0 before calling
		 * window_exposed event for GraphicsExpose event, because
		 * GraphicsExpose event itself is caused by scrolling (XCopyArea).
		 *
		 * XXX
		 * But win->is_scrollable = 0 is disabled for now because there
		 * seems no cases which cause definite inconvenience.
		 * (ref. flush_scroll_cache() in x_screen.c)
		 */
		if( event->type == GraphicsExpose)
		{
			win->wait_copy_area_response = 0 ;
		#if  0
			win->is_scrollable = 0 ;
		#endif
		}

		if( margin_area_exposed)
		{
			clear_margin_area( win) ;
		}

		if( win->window_exposed)
		{
			(*win->window_exposed)( win , x , y , width , height) ;
		}
	#if  0
		else
		{
			x_window_clear_all( win) ;
		}
	#endif

	#if  0
		if( event->type == GraphicsExpose)
		{
			win->is_scrollable = 1 ;
		}
	#endif
	}
	else if( event->type == ConfigureNotify)
	{
		int  is_changed ;
		XEvent  next_ev ;

		/*
		 * Optimize transparent processing in notify_configure_to_children.
		 */
		while( XCheckTypedWindowEvent( win->disp->display , win->my_window ,
			ConfigureNotify , &next_ev))
		{
			*event = next_ev ;
		}

		is_changed = 0 ;

		if( event->xconfigure.x != win->x || event->xconfigure.y != win->y)
		{
			/*
			 * for fvwm2 style virtual screen.
			 */
			if( abs( event->xconfigure.x - win->x) % win->disp->width != 0 ||
			    abs( event->xconfigure.y - win->y) % win->disp->height != 0 ||
			   ( event->xconfigure.x < 0 &&
				event->xconfigure.x + (int)ACTUAL_WIDTH(win) > 0) ||
			   ( event->xconfigure.x > 0 &&
				event->xconfigure.x + (int)ACTUAL_WIDTH(win)
					> (int)win->disp->width) ||
			   ( event->xconfigure.y < 0 &&
				event->xconfigure.y + (int)ACTUAL_HEIGHT(win) > 0) ||
			   ( event->xconfigure.y > 0 &&
				event->xconfigure.y + (int)ACTUAL_HEIGHT(win)
					> (int)win->disp->height) )
			{
				is_changed = 1 ;
			}

			win->x = event->xconfigure.x ;
			win->y = event->xconfigure.y ;
		}

		if( event->xconfigure.width != ACTUAL_WIDTH(win) ||
			event->xconfigure.height != ACTUAL_HEIGHT(win))
		{
			win->width = event->xconfigure.width - win->hmargin * 2 ;
			win->height = event->xconfigure.height - win->vmargin * 2 ;

			if( win->window_resized)
			{
				win->configure_root = 1 ;
				(*win->window_resized)( win) ;
				win->configure_root = 0 ;
			}

			is_changed = 1 ;

		#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
			if( win->cairo_draw)
			{
				cairo_resize( win) ;
			}
		#endif
		}

		if( is_changed)
		{
			notify_configure_to_children( win) ;
		}
	}
	else if( event->type == ReparentNotify)
	{
		XEvent  next_ev ;
		
		/*
		 * Optimize transparent processing in notify_reparent_to_children.
		 */
		while( XCheckTypedWindowEvent( win->disp->display , win->my_window ,
			ReparentNotify , &next_ev))
		{
			*event = next_ev ;
		}

		win->x = event->xreparent.x ;
		win->y = event->xreparent.y ;

		notify_reparent_to_children( win) ;
	}
#if  0
	else if( event->type == MapNotify)
	{
		if( win->is_transparent && ! win->wall_picture_is_set)
		{
			set_transparent( win) ;
		}
	}
#endif
	else if( event->type == SelectionClear)
	{
		/*
		 * Call win->selection_cleared and win->is_sel_owner is set 0
		 * in x_display_clear_selection.
		 */
		x_display_clear_selection( win->disp , win) ;

		free( sel_bmp) ;
		sel_bmp = NULL ;
	}
	else if( event->type == SelectionRequest)
	{
		Atom  xa_utf8_string ;
		Atom  xa_compound_text ;
	#ifdef  DEBUG
		Atom  xa_multiple ;
	#endif
		Atom  xa_targets ;
		Atom  xa_text ;
		Atom  xa_bmp ;

		xa_compound_text = XA_COMPOUND_TEXT(win->disp->display) ;
		xa_targets = XA_TARGETS(win->disp->display) ;
	#ifdef  DEBUG
		xa_multiple = XA_MULTIPLE(win->disp->display) ;
	#endif
		xa_text = XA_TEXT(win->disp->display) ;
		xa_utf8_string = XA_UTF8_STRING(win->disp->display) ;
		xa_bmp = XA_BMP(win->disp->display) ;

		if( event->xselectionrequest.target == XA_STRING)
		{
			if( win->xct_selection_requested)
			{
				(*win->xct_selection_requested)( win , &event->xselectionrequest ,
					event->xselectionrequest.target) ;
			}
		}
		else if( event->xselectionrequest.target == xa_text ||
			event->xselectionrequest.target == xa_compound_text)
		{
			if( win->xct_selection_requested)
			{
				/*
				 * kterm requests selection with "TEXT" atom , but
				 * wants it to be sent back with "COMPOUND_TEXT" atom.
				 * why ?
				 */

				(*win->xct_selection_requested)( win , &event->xselectionrequest ,
					xa_compound_text) ;
			}
		}
		else if( event->xselectionrequest.target == xa_utf8_string)
		{
			if( win->utf_selection_requested)
			{
				(*win->utf_selection_requested)( win , &event->xselectionrequest ,
					xa_utf8_string) ;
			}
		}
		else if( event->xselectionrequest.target == xa_targets)
		{
			Atom  targets[6] ;

			targets[0] = xa_targets ;
			targets[1] = XA_STRING ;
			targets[2] = xa_text ;
			targets[3] = xa_compound_text ;
			targets[4] = xa_utf8_string ;
			targets[5] = xa_bmp ;

			send_selection( win , &event->xselectionrequest ,
				(u_char *)targets , sizeof(targets) / sizeof targets[0] ,
				XA_ATOM , 32) ;
		}
#ifdef  DEBUG
		else if( event->xselectionrequest.target == xa_multiple)
		{
			kik_debug_printf( "MULTIPLE requested(not yet implemented)\n") ;
		}
#endif
		else if( event->xselectionrequest.target == xa_bmp && sel_bmp)
		{
			send_selection( win , &event->xselectionrequest ,
				(u_char *)sel_bmp , sel_bmp_size , xa_bmp , 8) ;
		}
		else
		{
			send_selection( win , &event->xselectionrequest , NULL , 0 , 0 , 0) ;
		}
	}
	else if( event->type == SelectionNotify)
	{
		Atom  xa_utf8_string ;
		Atom  xa_compound_text ;
		Atom  xa_text ;
		Atom  xa_selection ;

		xa_compound_text = XA_COMPOUND_TEXT(win->disp->display) ;
		xa_text = XA_TEXT(win->disp->display) ;
		xa_utf8_string = XA_UTF8_STRING(win->disp->display) ;
		xa_selection = XA_SELECTION(win->disp->display) ;

		if( event->xselection.property == None ||
			event->xselection.property == XA_NONE(win->disp->display))
		{
			/*
			 * Selection request failed.
			 * Retrying with xa_compound_text => xa_text => XA_STRING
			 */

			if( event->xselection.target == xa_utf8_string)
			{
				XConvertSelection( win->disp->display , XA_PRIMARY ,
					xa_compound_text , xa_selection , win->my_window ,
					CurrentTime) ;
			}
			else if( event->xselection.target == xa_compound_text)
			{
				XConvertSelection( win->disp->display , XA_PRIMARY ,
					xa_text , xa_selection , win->my_window ,
					CurrentTime) ;
			}
			else if( event->xselection.target == xa_text)
			{
				XConvertSelection( win->disp->display , XA_PRIMARY ,
					XA_STRING , xa_selection , win->my_window ,
					CurrentTime) ;
			}

			return  1 ;
		}

		/* SELECTION */
		if( event->xselection.selection == XA_PRIMARY &&
		    ( event->xselection.property == xa_selection &&
		      ( event->xselection.target == XA_STRING ||
			event->xselection.target == xa_text ||
			event->xselection.target == xa_compound_text ||
			event->xselection.target == xa_utf8_string)))
		{
			u_long  bytes_after ;
			XTextProperty  ct ;
			int  seg ;

			for( seg = 0 ; ; seg += ct.nitems)
			{
				/*
				 * XXX
				 * long_offset and long_len is the same as rxvt-2.6.3 ,
				 * but I'm not confident if this is OK.
				 */
				if( XGetWindowProperty( win->disp->display ,
					event->xselection.requestor ,
					event->xselection.property , seg / 4 , 4096 , False ,
					AnyPropertyType , &ct.encoding , &ct.format ,
					&ct.nitems , &bytes_after , &ct.value) != Success)
				{
					break ;
				}

				if( ct.value == NULL || ct.nitems == 0)
				{
					break ;
				}

				if( ct.encoding == XA_STRING ||
					 ct.encoding == xa_text ||
					 ct.encoding == xa_compound_text)
				{
					if( win->xct_selection_notified)
					{
						(*win->xct_selection_notified)(
							win , ct.value , ct.nitems) ;
					}
				}
				else if( ct.encoding == xa_utf8_string)
				{
					if( win->utf_selection_notified)
					{
						(*win->utf_selection_notified)(
							win , ct.value , ct.nitems) ;
					}
				}

				XFree( ct.value) ;

				if( bytes_after == 0)
				{
					break ;
				}
			}

		}

		XDeleteProperty( win->disp->display, event->xselection.requestor,
			event->xselection.property) ;
	}
	else if( event->type == ClientMessage)
	{
		if( event->xclient.format == 32 &&
			event->xclient.data.l[0] == XA_DELETE_WINDOW( win->disp->display))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" DeleteWindow message is received. exiting...\n") ;
		#endif
			if( win->window_deleted)
			{
				(*win->window_deleted)( win) ;
			}
			else
			{
				exit(0) ;
			}
		}
#if  0
		else if( event->xclient.format == 32 &&
			event->xclient.data.l[0] == XA_TAKE_FOCUS( win->disp->display))
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" TakeFocus message is received.\n") ;
		}
#endif
	}
	else if( event->type == PropertyNotify)
	{
		if( event->xproperty.atom == XA_SELECTION( win->disp->display) &&
		    event->xproperty.state == PropertyNewValue)
		{
			XTextProperty  ct ;
			u_long  bytes_after ;

			XGetWindowProperty( win->disp->display, event->xproperty.window,
					    event->xproperty.atom, 0, 0, False,
					    AnyPropertyType, &ct.encoding, &ct.format,
					    &ct.nitems, &bytes_after, &ct.value) ;
			if( ct.value)
			{
				XFree( ct.value) ;
			}

			if( ct.encoding == XA_INCR(win->disp->display)
			    || bytes_after == 0)
			{
				XDeleteProperty( win->disp->display, event->xproperty.window,
						 ct.encoding) ;
			}
			else
			{
				XGetWindowProperty( win->disp->display , event->xproperty.window ,
						    event->xproperty.atom , 0 , bytes_after , True ,
						    AnyPropertyType , &ct.encoding , &ct.format ,
						    &ct.nitems , &bytes_after , &ct.value) ;
				if(ct.encoding == XA_STRING ||
				   ct.encoding == XA_TEXT(win->disp->display) ||
				   ct.encoding == XA_COMPOUND_TEXT(win->disp->display))
				{
					if( win->xct_selection_notified)
					{
						(*win->xct_selection_notified)(
							win , ct.value , ct.nitems) ;
					}
				}
				else if(ct.encoding == XA_UTF8_STRING(win->disp->display))
				{
					if( win->utf_selection_notified)
					{
						(*win->utf_selection_notified)(
							win , ct.value , ct.nitems) ;
					}
				}

				if( ct.value)
				{
					XFree( ct.value) ;
				}
			}
		}
	}
#ifdef  __DEBUG
	else
	{
		kik_warn_printf( KIK_DEBUG_TAG " event %d is received, but not processed.\n" ,
			event->type) ;
	}
#endif

	return  1 ;
}

size_t
x_window_get_str(
	x_window_t *  win ,
	u_char *  seq ,
	size_t  seq_len ,
	mkf_parser_t **  parser ,
	KeySym *  keysym ,
	XKeyEvent *  event
	)
{
	size_t  len ;

	*keysym = 0 ;

	if( ( len = x_xic_get_str( win , seq , seq_len , parser , keysym , event)) > 0)
	{
		return  len ;
	}

	if( ( len = XLookupString( event , seq , seq_len , keysym , NULL)) > 0)
	{
		*parser = NULL ;

                return  len ;
	}

	if( ( len = x_xic_get_utf8_str( win , seq , seq_len , parser , keysym , event)) > 0)
	{
		return  len ;
	}

	return  0 ;
}

/*
 * Scroll functions.
 * The caller side should clear the scrolled area.
 */

int
x_window_scroll_upward(
	x_window_t *  win ,
	u_int  height
	)
{
	return  x_window_scroll_upward_region( win , 0 , win->height , height) ;
}

int
x_window_scroll_upward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  height
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->height || boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" boundary start %d end %d height %d in window((h) %d (w) %d)\n" ,
			boundary_start , boundary_end , height , win->height , win->width) ;
	#endif

		return  0 ;
	}

	scroll_region( win ,
		0 , boundary_start + height ,	/* src */
		win->width , boundary_end - boundary_start - height ,	/* size */
		0 , boundary_start) ;		/* dst */

	return  1 ;
}

int
x_window_scroll_downward(
	x_window_t *  win ,
	u_int  height
	)
{
	return  x_window_scroll_downward_region( win , 0 , win->height , height) ;
}

int
x_window_scroll_downward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  height
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->height ||
		boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " boundary start %d end %d height %d\n" ,
			boundary_start , boundary_end , height) ;
	#endif

		return  0 ;
	}

	scroll_region( win ,
		0 , boundary_start ,
		win->width , boundary_end - boundary_start - height ,
		0 , boundary_start + height) ;

	return  1 ;
}

int
x_window_scroll_leftward(
	x_window_t *  win ,
	u_int  width
	)
{
	return  x_window_scroll_leftward_region( win , 0 , win->width , width) ;
}

int
x_window_scroll_leftward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  width
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->width || boundary_end <= boundary_start + width)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" boundary start %d end %d width %d in window((h) %d (w) %d)\n" ,
			boundary_start , boundary_end , width , win->height , win->width) ;
	#endif

		return  0 ;
	}

	scroll_region( win ,
		boundary_start + width , 0 ,	/* src */
		boundary_end - boundary_start - width , win->height ,	/* size */
		boundary_start , 0) ;		/* dst */

	return  1 ;
}

int
x_window_scroll_rightward(
	x_window_t *  win ,
	u_int  width
	)
{
	return  x_window_scroll_rightward_region( win , 0 , win->width , width) ;
}

int
x_window_scroll_rightward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  width
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->width || boundary_end <= boundary_start + width)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " boundary start %d end %d width %d\n" ,
			boundary_start , boundary_end , width) ;
	#endif

		return  0 ;
	}

	scroll_region( win ,
		boundary_start , 0 ,
		boundary_end - boundary_start - width , win->height ,
		boundary_start + width , 0) ;

	return  1 ;
}

int
x_window_copy_area(
	x_window_t *  win ,
	Pixmap  src ,
	PixmapMask  mask ,
	int  src_x ,	/* >= 0 */
	int  src_y ,	/* >= 0 */
	u_int  width ,
	u_int  height ,
	int  dst_x ,	/* >= 0 */
	int  dst_y	/* >= 0 */
	)
{
	if( dst_x >= win->width || dst_y >= win->height)
	{
		return  0 ;
	}

	if( dst_x + width > win->width)
	{
		width = win->width - dst_x ;
	}

	if( dst_y + height > win->height)
	{
		height = win->height - dst_y ;
	}

	if( win->gc->mask != mask)
	{
		XSetClipMask( win->disp->display , win->gc->gc , mask) ;
		win->gc->mask = mask ;
	}

	if( mask)
	{
		XSetClipOrigin( win->disp->display , win->gc->gc ,
			dst_x + win->hmargin - src_x , dst_y + win->vmargin - src_y) ;
	}

	XCopyArea( win->disp->display , src , win->my_window , win->gc->gc ,
		src_x , src_y , width , height , dst_x + win->hmargin , dst_y + win->vmargin) ;

	return  1 ;
}

void
x_window_set_clip(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	if( win->cairo_draw)
	{
		cairo_set_clip( win , x + win->hmargin , y + win->vmargin , width , height) ;
	}
	else
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	if( win->xft_draw)
	{
		xft_set_clip( win , x + win->hmargin , y + win->vmargin , width , height) ;
	}
	else
#endif
	{
		XRectangle  rect ;

		rect.x = 0 ;
		rect.y = 0 ;
		rect.width = width ;
		rect.height = height ;

		XSetClipRectangles( win->disp->display , win->gc->gc ,
			x + win->hmargin , y + win->vmargin , &rect , 1 , YSorted) ;
	}
}

void
x_window_unset_clip(
	x_window_t *  win
	)
{
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	if( win->cairo_draw)
	{
		cairo_unset_clip( win) ;
	}
	else
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	if( win->xft_draw)
	{
		xft_unset_clip( win) ;
	}
	else
#endif
	{
		XSetClipMask( win->disp->display , win->gc->gc , None) ;
	}
}

int
x_window_draw_decsp_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	convert_to_decsp_font_index( str , len) ;

	if( font->decsp_font)
	{
		x_gc_set_fg_color( win->gc, fg_color->pixel) ;

		return  x_decsp_font_draw_string( font->decsp_font ,
				win->disp->display , win->my_window , win->gc->gc ,
				x + win->hmargin , y + win->vmargin , str , len) ;
	}
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	else if( font->xfont)
	{
		return  x_window_draw_string( win , font , fg_color , x , y , str , len) ;
	}
#endif
	else
	{
		return  0 ;
	}
}

int
x_window_draw_decsp_image_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	convert_to_decsp_font_index( str , len) ;

	if( font->decsp_font)
	{
		x_gc_set_fg_color( win->gc, fg_color->pixel) ;
		x_gc_set_bg_color( win->gc, bg_color->pixel) ;

		return  x_decsp_font_draw_image_string( font->decsp_font ,
				win->disp->display , win->my_window , win->gc->gc ,
				x + win->hmargin , y + win->vmargin , str , len) ;
	}
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	else if( font->xfont)
	{
		return  x_window_draw_image_string( win , font , fg_color , bg_color ,
				x , y , str , len) ;
	}
#endif
	else
	{
		return  0 ;
	}
}

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
int
x_window_draw_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	/* Removing trailing spaces. */
	while( 1)
	{
		if( len == 0)
		{
			return  1 ;
		}

		if( *(str + len - 1) == ' ')
		{
			len-- ;
		}
		else
		{
			break ;
		}
	}

	x_gc_set_fid( win->gc, font->xfont->fid) ;
	x_gc_set_fg_color( win->gc, fg_color->pixel) ;

	XDrawString( win->disp->display , win->my_window , win->gc->gc ,
		x + font->x_off + win->hmargin ,
		y + win->vmargin , (char *)str , len) ;

	if( font->double_draw_gap)
	{
		XDrawString( win->disp->display , win->my_window , win->gc->gc ,
			x + font->x_off + win->hmargin + font->double_draw_gap ,
			y + win->vmargin , (char *)str , len) ;
	}

	return  1 ;
}

int
x_window_draw_string16(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	XChar2b *  str ,
	u_int  len
	)
{
	x_gc_set_fid( win->gc, font->xfont->fid) ;
	x_gc_set_fg_color( win->gc, fg_color->pixel) ;

	XDrawString16( win->disp->display , win->my_window , win->gc->gc ,
		       x + font->x_off + win->hmargin ,
		       y + win->vmargin , str , len) ;

	if( font->double_draw_gap)
	{
		XDrawString16( win->disp->display , win->my_window , win->gc->gc ,
			       x + font->x_off + win->hmargin + font->double_draw_gap ,
			       y + win->vmargin , str , len) ;
	}

	return  1 ;
}

int
x_window_draw_image_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	x_gc_set_fid( win->gc, font->xfont->fid) ;
	x_gc_set_fg_color( win->gc, fg_color->pixel) ;
	x_gc_set_bg_color( win->gc, bg_color->pixel) ;

	XDrawImageString( win->disp->display , win->my_window , win->gc->gc ,
			  x + font->x_off + win->hmargin ,
			  y + win->vmargin , (char *)str , len) ;

	if( font->double_draw_gap)
	{
		XDrawString( win->disp->display , win->my_window , win->gc->gc ,
			     x + font->x_off + win->hmargin + font->double_draw_gap ,
			     y + win->vmargin , (char *)str , len) ;
	}

	return  1 ;
}

int
x_window_draw_image_string16(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	XChar2b *  str ,
	u_int  len
	)
{
	x_gc_set_fid( win->gc, font->xfont->fid) ;
	x_gc_set_fg_color( win->gc, fg_color->pixel) ;
	x_gc_set_bg_color( win->gc, bg_color->pixel) ;

	XDrawImageString16( win->disp->display , win->my_window , win->gc->gc ,
			    x + font->x_off + win->hmargin ,
			    y + win->vmargin , str , len) ;

	if( font->double_draw_gap)
	{
		XDrawString16( win->disp->display , win->my_window , win->gc->gc ,
			       x + font->x_off + win->hmargin + font->double_draw_gap ,
			       y + win->vmargin , str , len) ;
	}

	return  1 ;
}
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
int
x_window_ft_draw_string8(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	size_t  len
	)
{
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	if( win->cairo_draw)
	{
		return  x_window_cairo_draw_string8( win , font , fg_color , x , y , str , len) ;
	}
	else
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	if( win->xft_draw)
	{
		return  x_window_xft_draw_string8( win , font , fg_color , x , y , str , len) ;
	}
	else
#endif
	{
		return  0 ;
	}
}

int
x_window_ft_draw_string32(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	/* FcChar32 */ u_int32_t *  str ,
	u_int  len
	)
{
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	if( win->cairo_draw)
	{
		return  x_window_cairo_draw_string32( win , font , fg_color , x , y , str , len) ;
	}
	else
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	if( win->xft_draw)
	{
		return  x_window_xft_draw_string32( win , font , fg_color , x , y , str , len) ;
	}
	else
#endif
	{
		return  0 ;
	}
}

#endif


int
x_window_draw_rect_frame(
	x_window_t *  win ,
	int  x1 ,
	int  y1 ,
	int  x2 ,
	int  y2
	)
{
	XPoint  points[5] =
	{
		{ x1 += win->hmargin	, y1 += win->vmargin } ,
		{ x1			, y2 += win->vmargin } ,
		{ x2 += win->hmargin	, y2 } ,
		{ x2			, y1 } ,
		{ x1			, y1 } ,
	} ;

	restore_fg_color(win) ;

	XDrawLines( win->disp->display , win->my_window , win->gc->gc , points , 5 ,
		CoordModeOrigin) ;

	return  1 ;
}

int
x_set_use_clipboard_selection(
	int  use_it
	)
{
	if( use_clipboard == use_it)
	{
		return  0 ;
	}

	use_clipboard = use_it ;

	/*
	 * 'is_sel_owner' member of all x_window_t is reset.
	 * If 'is_sel_owner' is not reset and value of 'use_clipboard' option
	 * is changed from false to true dynamically,
	 * x_window_set_selection_owner() returns before calling XSetSelectionOwner().
	 */
	x_display_clear_selection( NULL , NULL) ;

	return  1 ;
}

int
x_is_using_clipboard_selection(void)
{
	return  use_clipboard ;
}

int
x_window_set_selection_owner(
	x_window_t *  win ,
	Time  time
	)
{
	if( win->is_sel_owner)
	{
		/* Already owner */

		return  1 ;
	}

	XSetSelectionOwner( win->disp->display , XA_PRIMARY , win->my_window , time) ;
	if( use_clipboard)
	{
		XSetSelectionOwner( win->disp->display , XA_CLIPBOARD(win->disp->display) ,
			win->my_window , time) ;
	}

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " XA_PRIMARY => %lu, XA_CLIPBOARD => %lu (mywin %lu)\n" ,
		XGetSelectionOwner( win->disp->display , XA_PRIMARY) ,
		XGetSelectionOwner( win->disp->display , XA_CLIPBOARD(win->disp->display)) ,
		win->my_window) ;
#endif

	if( win->my_window != XGetSelectionOwner( win->disp->display , XA_PRIMARY) &&
	    ( ! use_clipboard ||
	      win->my_window != XGetSelectionOwner( win->disp->display ,
					XA_CLIPBOARD(win->disp->display))) )
	{
		return  0 ;
	}
	else
	{
		win->is_sel_owner = 1 ;
		
		return  x_display_own_selection( win->disp , win) ;
	}
}

int
x_window_xct_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
	XConvertSelection( win->disp->display , XA_PRIMARY , XA_COMPOUND_TEXT(win->disp->display) ,
		XA_SELECTION(win->disp->display) , win->my_window , time) ;

	return  1 ;
}

int
x_window_utf_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
	XConvertSelection( win->disp->display , XA_PRIMARY , XA_UTF8_STRING(win->disp->display) ,
		XA_SELECTION(win->disp->display) , win->my_window , time) ;

	return  1 ;
}

int
x_window_send_picture_selection(
	x_window_t *  win ,
	Pixmap  pixmap ,
	u_int  width ,
	u_int  height
	)
{
	XImage *  image ;

	if( win->disp->visual->class == TrueColor &&
	    ( image = XGetImage( win->disp->display , pixmap , 0 , 0 , width , height ,
			AllPlanes , ZPixmap)))
	{
		XVisualInfo *  vinfo ;

		if( ( vinfo = x_display_get_visual_info( win->disp)))
		{
			int  shift[3] ;
			u_long  mask[3] ;
			size_t  image_size ;

			shift[0] = right_shift((mask[0] = vinfo->blue_mask)) ;
			shift[1] = right_shift((mask[1] = vinfo->green_mask)) ;
			shift[2] = right_shift((mask[2] = vinfo->red_mask)) ;

			image_size = width * height * 4 ;
			sel_bmp_size = image_size + 54 ;

			if( ( sel_bmp = calloc( 1 , sel_bmp_size)))
			{
				int  x ;
				int  y ;
				u_char *  dst ;

				sel_bmp->h_type[0] = 0x42 ;
				sel_bmp->h_type[1] = 0x4d ;
				sel_bmp->h_size[0] = sel_bmp_size & 0xff ;
				sel_bmp->h_size[1] = (sel_bmp_size >> 8) & 0xff ;
				sel_bmp->h_size[2] = (sel_bmp_size >> 16) & 0xff ;
				sel_bmp->h_size[3] = (sel_bmp_size >> 24) & 0xff ;
				sel_bmp->h_offbits[0] = 54 ;
				sel_bmp->i_size[0] = 40 ;
				sel_bmp->i_width[0] = width & 0xff ;
				sel_bmp->i_width[1] = (width >> 8) & 0xff ;
				sel_bmp->i_width[2] = (width >> 16) & 0xff ;
				sel_bmp->i_width[3] = (width >> 24) & 0xff ;
				sel_bmp->i_height[0] = height & 0xff ;
				sel_bmp->i_height[1] = (height >> 8) & 0xff ;
				sel_bmp->i_height[2] = (height >> 16) & 0xff ;
				sel_bmp->i_height[3] = (height >> 24) & 0xff ;
				sel_bmp->i_planes[0] = 1 ;
				sel_bmp->i_bitcount[0] = 32 ;
				sel_bmp->i_sizeimage[0] = image_size & 0xff ;
				sel_bmp->i_sizeimage[1] = (image_size >> 8) & 0xff ;
				sel_bmp->i_sizeimage[2] = (image_size >> 16) & 0xff ;
				sel_bmp->i_sizeimage[3] = (image_size >> 24) & 0xff ;

				dst = sel_bmp->data ;
				for( y = height - 1 ; y >= 0 ; y--)
				{
					for( x = 0 ; x < width ; x++)
					{
						u_long  pixel ;
						int  count ;

						pixel = XGetPixel( image , x , y) ;

						for( count = 0 ; count < 3 ; count++)
						{
							if( shift[count] < 0)
							{
								*(dst++) = (pixel & mask[count]) <<
										(-shift[count]) ;
							}
							else
							{
								*(dst++) = (pixel & mask[count]) >>
										(shift[count]) ;
							}
						}

						*(dst++) = 0x00 ;
					}
				}

				x_window_set_selection_owner( win , CurrentTime) ;

				kik_msg_printf( "Set a clicked picture to the clipboard.\n") ;
			}

			XFree(vinfo) ;
		}

		XDestroyImage( image) ;
	}

	return  0 ;
}

int
x_window_send_text_selection(
	x_window_t *  win ,
	XSelectionRequestEvent *  req_ev ,
	u_char *  sel_data ,
	size_t  sel_len ,
	Atom  sel_type
	)
{
	XEvent  res_ev ;

	res_ev.xselection.type = SelectionNotify ;
	res_ev.xselection.display = req_ev->display ;
	res_ev.xselection.requestor = req_ev->requestor ;
	res_ev.xselection.selection = req_ev->selection ;
	res_ev.xselection.target = req_ev->target ;
	res_ev.xselection.time = req_ev->time ;

	if( sel_data == NULL)
	{
		res_ev.xselection.property = None ;
	}
	else
	{
		if( req_ev->property == None)
		{
			/* An obsolete client may fill None as a property.
			 * Try to deal with them by using 'target' instead.
			 */
			req_ev->property = req_ev->target;
		}
		if( req_ev->property != None)
		{
			XChangeProperty( win->disp->display , req_ev->requestor ,
				req_ev->property , sel_type ,
				8 , PropModeReplace , sel_data , sel_len) ;
		}
		res_ev.xselection.property = req_ev->property ;
	}

	XSendEvent( win->disp->display , res_ev.xselection.requestor , False , 0 , &res_ev) ;

	return  1 ;
}

int
x_set_window_name(
	x_window_t *  win ,
	u_char *  name
	)
{
	x_window_t *  root ;
	XTextProperty  prop ;

	root = x_get_root_window( win) ;

	if( name == NULL)
	{
		name = root->app_name ;
	}

	if( XmbTextListToTextProperty( root->disp->display , (char**)&name , 1 , XStdICCTextStyle ,
					&prop) >= Success)
	{
		XSetWMName( root->disp->display , root->my_window , &prop) ;

		XFree( prop.value) ;
	}
	else
	{
		/* XXX which is better , doing this or return 0 without doing anything ? */
		XStoreName( root->disp->display , root->my_window , name) ;
	}

	return  1 ;
}

int
x_set_icon_name(
	x_window_t *  win ,
	u_char *  name
	)
{
	x_window_t *  root ;
	XTextProperty  prop ;

	root = x_get_root_window( win) ;

	if( name == NULL)
	{
		name = root->app_name ;
	}

	if( XmbTextListToTextProperty( root->disp->display , (char**)&name , 1 , XStdICCTextStyle ,
				&prop) >= Success)
	{
		XSetWMIconName( root->disp->display , root->my_window , &prop) ;

		XFree( prop.value) ;
	}
	else
	{
		/* XXX which is better , doing this or return 0 without doing anything ? */
		XSetIconName( root->disp->display , root->my_window , name) ;
	}

	return  1 ;
}

int
x_window_set_icon(
	x_window_t *  win ,
	x_icon_picture_t *  icon
	)
{
	x_window_t *  root ;
	XWMHints *  hints ;

	root = x_get_root_window( win) ;

	/* set extended window manager hint's icon */
	if( icon->cardinal && icon->cardinal[0] && icon->cardinal[1])
	{
		int  num ;
		u_long *  data ;

		/* width * height + 2 */
		num = icon->cardinal[0] * icon->cardinal[1] + 2 ;

		if( sizeof(u_long) != 4)
		{
			int  count ;

			if( ! ( data = alloca( sizeof(u_long) * num)))
			{
				return  0 ;
			}

			for( count = 0 ; count < num ; count++)
			{
				data[count] = icon->cardinal[count] ;
			}
		}
		else
		{
			data = icon->cardinal ;
		}

		/*it should be possible to set multiple icons...*/
		XChangeProperty( root->disp->display, root->my_window,
				 XA_NET_WM_ICON( root->disp->display),
				 XA_CARDINAL, 32, PropModeReplace,
				 (u_char*)data , num) ;
	}

	if( ( hints = XGetWMHints( root->disp->display , root->my_window)) == NULL &&
		( hints = XAllocWMHints()) == NULL)
	{
		return  0 ;
	}
	
	if( icon->pixmap)
	{
		hints->flags |= IconPixmapHint ;
		hints->icon_pixmap = icon->pixmap ;
	}
	
	if( icon->mask)
	{
		hints->flags |= IconMaskHint ;
		hints->icon_mask = icon->mask ;
	}

	XSetWMHints( root->disp->display, root->my_window, hints) ;
	XFree( hints) ;

	return 1 ;
}

int
x_window_remove_icon(
	x_window_t *  win
	)
{
	x_window_t *  root ;
	XWMHints *  hints ;

	root = x_get_root_window( win) ;
	
	if( ( hints = XGetWMHints( root->disp->display , root->my_window)))
	{
	#if  0
		kik_debug_printf( " Removing icon.\n") ;
	#endif
	
		hints->flags &= ~(IconPixmapHint | IconMaskHint) ;
		hints->icon_pixmap = None ;
		hints->icon_mask = None ;

		XSetWMHints( root->disp->display, root->my_window, hints) ;
		XFree( hints) ;
	}
	
	XDeleteProperty( root->disp->display, root->my_window ,
		XA_NET_WM_ICON( root->disp->display)) ;

	return  1 ;
}

int
x_window_reset_group(
	x_window_t *  win
	)
{
	x_window_t *  root ;
	XWMHints *  hints ;

	root = x_get_root_window( win) ;

	if( ( hints = XGetWMHints( root->disp->display , root->my_window)) == NULL &&
		( hints = XAllocWMHints()) == NULL)
	{
		return  0 ;
	}
	
	hints->flags |= WindowGroupHint ;
	hints->window_group = reset_client_leader( root) ;

	XSetWMHints( root->disp->display, root->my_window, hints) ;
	XFree( hints) ;

	return  1 ;
}

int
x_window_get_visible_geometry(
	x_window_t *  win ,
	int *  x ,		/* x relative to root window */
	int *  y ,		/* y relative to root window */
	int *  my_x ,		/* x relative to my window */
	int *  my_y ,		/* y relative to my window */
	u_int *  width ,
	u_int *  height
	)
{
	Window  child ;

	XTranslateCoordinates( win->disp->display , win->my_window ,
		win->disp->my_window , 0 , 0 , x , y , &child) ;

	if( *x >= (int)win->disp->width || *y >= (int)win->disp->height)
	{
		/* no visible window */

		return  0 ;
	}

	if( *x < 0)
	{
		if( ACTUAL_WIDTH(win) <= abs(*x))
		{
			/* no visible window */

			return  0 ;
		}

		*my_x = abs(*x) ;

		*width = ACTUAL_WIDTH(win) - abs(*x) ;
		*x = 0 ;
	}
	else
	{
		*my_x = 0 ;
		*width = ACTUAL_WIDTH(win) ;
	}

	if( *y < 0)
	{
		if( ACTUAL_HEIGHT(win) <= abs(*y))
		{
			/* no visible window */

			return  0 ;
		}

		*my_y = abs(*y) ;

		*height = ACTUAL_HEIGHT(win) - abs(*y) ;
		*y = 0 ;
	}
	else
	{
		*my_y = 0 ;
		*height = ACTUAL_HEIGHT(win) ;
	}

	if( *x + (int)*width > (int)win->disp->width)
	{
		*width = win->disp->width - *x ;
	}

	if( *y + (int)*height > (int)win->disp->height)
	{
		*height = win->disp->height - *y ;
	}

	return  1 ;
}

int
x_set_click_interval(
	int  interval
	)
{
	click_interval = interval ;

	return  1 ;
}

u_int
x_window_get_mod_ignore_mask(
	x_window_t *  win ,
	KeySym *  keysyms
	)
{
	XModifierKeymap *  mod_map ;
	int  count ;
	u_int  ignore ;
	u_int  masks[] = { Mod1Mask , Mod2Mask , Mod3Mask , Mod4Mask , Mod5Mask } ;
	KeySym default_keysyms[] = { XK_Num_Lock , XK_Scroll_Lock , XK_ISO_Level3_Lock ,
				     NoSymbol } ;

	if( !keysyms)
	{
		keysyms = default_keysyms ;
	}
	
	if( ( mod_map = x_window_get_modifier_mapping( win)) == NULL)
	{
		return  ~0 ;
	}
	
	ignore = 0 ;

	count = 0 ;
	while( keysyms[count] != NoSymbol)
	{
		int  ks_count ;
		KeyCode  kc ;

		kc = XKeysymToKeycode( win->disp->display, keysyms[count]);
		for( ks_count = 0; ks_count < sizeof(masks)/sizeof(masks[0]); ks_count++)
		{
			int  kc_count ;
			KeyCode *  key_codes ;

			key_codes = &(mod_map->modifiermap[(ks_count+3)*mod_map->max_keypermod]) ;
			for( kc_count = 0; kc_count < mod_map->max_keypermod; kc_count++)
			{
				if( key_codes[kc_count] == 0)
				{
					break ;
				}
				if( key_codes[kc_count] == kc)
				{
				#ifdef  DEBUG
					kik_debug_printf("keycode = %d, mod%d  idx %d  (by %s)\n",
						kc,  ks_count+1, kc_count+1,
						XKeysymToString(keysyms[count]));
				#endif
					ignore |= masks[ks_count] ;
					break ;
				}
			}
		}
		count++ ;
	}

	return  ~ignore ;
}

u_int
x_window_get_mod_meta_mask(
	x_window_t *  win ,
	char *  mod_key
	)
{
	int  mask_count ;
	int  kc_count ;
	XModifierKeymap *  mod_map ;
	KeyCode *  key_codes ;
	KeySym  sym ;
	char *  mod_keys[] = { "mod1" , "mod2" , "mod3" , "mod4" , "mod5" } ;
	u_int  mod_masks[] = { Mod1Mask , Mod2Mask , Mod3Mask , Mod4Mask , Mod5Mask } ;
	if( mod_key)
	{
		int  count ;

		for( count = 0 ; count < sizeof( mod_keys) / sizeof( mod_keys[0]) ; count ++)
		{
			if( strcmp( mod_key , mod_keys[count]) == 0)
			{
				return  mod_masks[count] ;
			}
		}
	}

	if( ( mod_map = x_window_get_modifier_mapping( win)) == NULL)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " x_window_get_modifier_mapping failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	key_codes = mod_map->modifiermap ;

	for( mask_count = 0 ; mask_count < sizeof(mod_masks)/sizeof(mod_masks[0]) ; mask_count++)
	{
		int  count ;

		/*
		 * KeyCodes order is like this.
		 * Shift[max_keypermod] Lock[max_keypermod] Control[max_keypermod]
		 * Mod1[max_keypermod] Mod2[max_keypermod] Mod3[max_keypermod]
		 * Mod4[max_keypermod] Mod5[max_keypermod]
		 */

		/*
		 * this modmap handling is tested with Xsun and XFree86-4.x
		 * it works fine on both X servers. (2004-10-19 seiichi)
		 */

		/* skip shift/lock/control */
		kc_count = (mask_count + 3) * mod_map->max_keypermod ;

		for( count = 0 ; count < mod_map->max_keypermod ; count++)
		{
			if( key_codes[kc_count] == 0)
			{
				break ;
			}

			sym = XKeycodeToKeysym( win->disp->display , key_codes[kc_count] , 0) ;

			if( ( ( mod_key == NULL || strcmp( mod_key , "meta") == 0) &&
					( sym == XK_Meta_L || sym == XK_Meta_R)) ||
				( ( mod_key == NULL || strcmp( mod_key , "alt") == 0) &&
					( sym == XK_Alt_L || sym == XK_Alt_R)) ||
				( ( mod_key == NULL || strcmp( mod_key , "super") == 0) &&
					( sym == XK_Super_L || sym == XK_Super_R)) ||
				( ( mod_key == NULL || strcmp( mod_key , "hyper") == 0) &&
					( sym == XK_Hyper_L || sym == XK_Hyper_R)) )
			{
				return  mod_masks[mask_count] ;
			}

			kc_count ++ ;
		}
	}

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " No meta key was found.\n") ;
#endif

	return  0 ;
}

int
x_set_use_urgent_bell(
	int  use
	)
{
	use_urgent_bell = use ;

	return  1 ;
}

int
x_window_bell(
	x_window_t *  win ,
	x_bel_mode_t  mode
	)
{
	urgent_bell( win , 1) ;

	if( mode & BEL_VISUAL)
	{
		x_window_blank( win) ;

	#if  0
		XSync( win->disp->display , False) ;
	#else
		XFlush( win->disp->display) ;
	#endif
		kik_usleep( 1) ;

		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	if( mode & BEL_SOUND)
	{
		XBell( win->disp->display , 0) ;
	}

	return  1 ;
}

int
x_window_translate_coordinates(
	x_window_t *  win ,
	int  x ,
	int  y ,
	int *  global_x ,
	int *  global_y ,
	Window *  child
	)
{
	XTranslateCoordinates( win->disp->display, win->my_window,
		DefaultRootWindow( win->disp->display) , x , y , global_x , global_y , child) ;
	
	return  1 ;
}

void
x_window_set_input_focus(
	x_window_t *  win
	)
{
	reset_input_focus( x_get_root_window( win)) ;
	win->inputtable = 1 ;
	XSetInputFocus( win->disp->display , win->my_window ,
		RevertToParent , CurrentTime) ;
}

#ifdef  DEBUG
void
x_window_dump_children(
	x_window_t *  win
	)
{
	int  count ;

	kik_msg_printf( "%p(%li) => " , win , win->my_window) ;
	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		kik_msg_printf( "%p(%li) " , win->children[count] ,
			win->children[count]->my_window) ;
	}
	kik_msg_printf( "\n") ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_dump_children( win->children[count]) ;
	}
}
#endif
