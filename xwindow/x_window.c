/*
 *	$Id$
 */

/*
 * Functions designed and implemented by Minami Hirokazu(minami@mistfall.net) are:
 * - XDND support
 * - Extended Window Manager Hint(Icon) support
 */

#include  "x_window.h"

#include  <stdlib.h>		/* abs */
#include  <string.h>		/* memset/memcpy */
#include  <X11/Xutil.h>		/* for XSizeHints */
#include  <X11/Xatom.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* realloc/free */

#include  "x_xic.h"
#include  "x_window_manager.h"
#include  "x_imagelib.h"
#include  "x_dnd.h"


/*
 * Atom macros.
 * Not cached because Atom may differ on each display
 */

#define  XA_COMPOUND_TEXT(display)  (XInternAtom(display , "COMPOUND_TEXT" , False))
#define  XA_TARGETS(display)  (XInternAtom(display , "TARGETS" , False))
#ifdef  DEBUG
#define  XA_MULTIPLE(display)  (XInternAtom(display , "MULTIPLE" , False))
#endif
#define  XA_TEXT(display)  (XInternAtom( display , "TEXT" , False))
#define  XA_UTF8_STRING(display)  (XInternAtom(display , "UTF8_STRING" , False))
#define  XA_SELECTION(display) (XInternAtom(display , "MLTERM_SELECTION" , False))
#define  XA_DELETE_WINDOW(display) (XInternAtom(display , "WM_DELETE_WINDOW" , False))
#define  XA_TAKE_FOCUS(display) (XInternAtom(display , "WM_TAKE_FOCUS" , False))
#define  XA_INCR(display) (XInternAtom(display, "INCR", False))
#define  XA_XROOTPMAP_ID(display) (XInternAtom(display, "_XROOTPMAP_ID", False))

/*
 * Extended Window Manager Hint support
 */
#define  XA_NET_WM_ICON(display) (XInternAtom(display, "_NET_WM_ICON", False))


/*
 * Motif Window Manager Hint (for borderless window)
 */
#define  XA_MWM_INFO(display) (XInternAtom(display, "_MOTIF_WM_INFO", True))
#define  XA_MWM_HINTS(display) (XInternAtom(display, "_MOTIF_WM_HINTS", True))

typedef struct {
	u_int32_t  flags ;
	u_int32_t  functions ;
	u_int32_t  decorations ;
	int32_t  inputMode ;
	u_int32_t  status ;
} MWMHints_t ;

#define MWM_HINTS_DECORATIONS   (1L << 1)


#define  MAX_CLICK  3			/* max is triple click */

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  click_interval = 250 ;	/* millisecond, same as xterm. */


/* --- static functions --- */

static int
set_transparent(
	x_window_t *  win ,
	Pixmap  pixmap
	)
{
	/*
	 * !! Notice !!
	 * this must be done before x_window_set_wall_picture() because
	 * x_window_set_wall_picture() doesn't do anything if is_transparent
	 * flag is on.
	 */
	win->is_transparent = 0 ;

	if( ! x_window_set_wall_picture( win , pixmap))
	{
		win->pic_mod = NULL ;

		return  0 ;
	}

	win->is_transparent = 1 ;

	return  1 ;
}

static int
unset_transparent(
	x_window_t *  win
	)
{
	/*
	 * !! Notice !!
	 * this must be done before x_window_unset_wall_picture() because
	 * x_window_unset_wall_picture() doesn't do anything if is_transparent
	 * flag is on.
	 */
	win->is_transparent = 0 ;
	win->pic_mod = NULL ;

	return  x_window_unset_wall_picture( win) ;
}

static int
update_pic_transparent(
	x_window_t *  win
	)
{
	x_picture_t  pic ;

	if( ! x_picture_init( &pic , win , win->pic_mod))
	{
		return  0 ;
	}

	if( ! x_picture_load_background( &pic))
	{
		x_picture_final( &pic) ;

		return  0 ;
	}

	set_transparent( win , pic.pixmap) ;

	x_picture_final( &pic) ;

	return  1 ;
}

static void
notify_focus_in_to_children(
	x_window_t *  win
	)
{
	int  count ;

	if( win->window_focused)
	{
		(*win->window_focused)( win) ;
	}

	x_xic_set_focus( win) ;

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
	int  count ;

	if( win->window_unfocused)
	{
		(*win->window_unfocused)( win) ;
	}

	x_xic_unset_focus( win) ;

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
	int  count ;

	if( win->is_transparent)
	{
		if( win->pic_mod || x_root_pixmap_available( win->display))
		{
			update_pic_transparent( win) ;
		}
		else
		{
			x_window_clear_all( win) ;
			(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
		}
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
	int  count ;

	if( win->is_transparent)
	{
		x_window_set_transparent( win , win->pic_mod) ;
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
	int  count ;

	if( win->is_transparent && x_root_pixmap_available( win->display))
	{
		update_pic_transparent( win) ;
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
	int  count ;

	if( win->my_window == window)
	{
		return  1 ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( is_descendant_window( win->children[count] , count))
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
	int  count ;
	u_int  min_width ;

	min_width = win->min_width + win->margin * 2 ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
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
	int  count ;
	u_int  min_height ;

	min_height = win->min_height + win->margin * 2 ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			min_height += total_min_height( win->children[count]) ;
		}
	}

	return  min_height ;
}

static u_int
total_base_width(
	x_window_t *  win
	)
{
	int  count ;
	u_int  base_width ;

	base_width = win->base_width + win->margin * 2 ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			base_width += total_base_width( win->children[count]) ;
		}
	}

	return  base_width ;
}

static u_int
total_base_height(
	x_window_t *  win
	)
{
	int  count ;
	u_int  base_height ;

	base_height = win->base_height + win->margin * 2 ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			base_height += total_base_height( win->children[count]) ;
		}
	}

	return  base_height ;
}

static u_int
total_width_inc(
	x_window_t *  win
	)
{
	int  count ;
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
	int  count ;
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

/* --- global functions --- */

int
x_window_init(
	x_window_t *  win ,
	u_int  width ,
	u_int  height ,
	u_int  min_width ,
	u_int  min_height ,
	u_int  base_width ,
	u_int  base_height ,
	u_int  width_inc ,
	u_int  height_inc ,
	u_int  margin
	)
{
	/*
	 * these values are set later
	 */

	win->display = NULL ;
	win->screen = 0 ;

	win->parent_window = None ;
	win->my_window = None ;

	win->drawable = None ;
	win->pixmap = None ;

#ifdef  USE_TYPE_XFT
	win->xft_draw = NULL ;
#endif

	win->gc = None ;
	win->ch_gc = None ;

	win->fg_color = 0 ;
	win->bg_color = 0 ;

	win->parent = NULL ;
	win->children = NULL ;
	win->num_of_children = 0 ;

	win->use_pixmap = 0 ;

	win->pic_mod = NULL ;

	win->wall_picture_is_set = 0 ;
	win->is_transparent = 0 ;

	win->cursor_shape = 0 ;

	win->event_mask = ExposureMask | VisibilityChangeMask | FocusChangeMask | PropertyChangeMask ;

	/* if visibility is partially obscured , scrollable will be 0. */
	win->is_scrollable = 1 ;

	win->is_mapped = 1 ;

	win->x = 0 ;
	win->y = 0 ;
	win->width = width ;
	win->height = height ;
	win->min_width = min_width ;
	win->min_height = min_height ;
	win->base_width = base_width ;
	win->base_height = base_height ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;
	win->margin = margin ;

	win->xic = NULL ;
	win->xim = NULL ;
	win->xim_listener = NULL ;

	win->prev_clicked_time = 0 ;
	win->prev_clicked_button = -1 ;
	win->click_num = 0 ;
	win->button_is_pressing = 0 ;
#ifndef  DISABLE_XDND
	win->dnd = NULL ;
#endif
	win->app_name = "mlterm" ;

	win->window_realized = NULL ;
	win->window_finalized = NULL ;
	win->window_exposed = NULL ;
	win->window_focused = NULL ;
	win->window_unfocused = NULL ;
	win->key_pressed = NULL ;
	win->button_motion = NULL ;
	win->button_released = NULL ;
	win->button_pressed = NULL ;
	win->button_press_continued = NULL ;
	win->window_resized = NULL ;
	win->child_window_resized = NULL ;
	win->selection_cleared = NULL ;
	win->xct_selection_requested = NULL ;
	win->utf8_selection_requested = NULL ;
	win->xct_selection_notified = NULL ;
	win->utf8_selection_notified = NULL ;
	win->window_deleted = NULL ;
	win->mapping_notify = NULL ;
#ifndef  DISABLE_XDND
	win->set_xdnd_config = NULL ;
#endif

	return	1 ;
}

int
x_window_final(
	x_window_t *  win
	)
{
	int  count ;

#ifdef  __DEBUG
	kik_debug_printf( "[deleting child windows]\n") ;
	x_window_dump_children( win) ;
#endif

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_final( win->children[count]) ;
	}

	if( win->children)
	{
		free( win->children) ;
	}

	x_window_manager_clear_selection( x_get_root_window( win)->win_man , win) ;

	XFreeGC( win->display , win->gc) ;
	XFreeGC( win->display , win->ch_gc) ;

	if( win->pixmap)
	{
		XFreePixmap( win->display , win->pixmap) ;
	}

#ifdef  USE_TYPE_XFT
	XftDrawDestroy( win->xft_draw) ;
#endif

	XDestroyWindow( win->display , win->my_window) ;

	x_xic_deactivate( win) ;

	if( win->window_finalized)
	{
		(*win->window_finalized)( win) ;
	}

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
		/* this can be used before x_window_show() */

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

	XSelectInput( win->display , win->my_window , win->event_mask) ;

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

	XSelectInput( win->display , win->my_window , win->event_mask) ;

	return  1 ;
}

int
x_window_set_wall_picture(
	x_window_t *  win ,
	Pixmap  pic
	)
{
	if( win->drawable != win->my_window)
	{
		/*
		 * wall picture can not be used in double buffering mode.
		 */

		return  0 ;
	}

	if( win->is_transparent)
	{
		/*
		 * unset transparent before setting wall picture !
		 */

		return  0 ;
	}

	if( win->event_mask & VisibilityChangeMask)
	{
		/* rejecting VisibilityNotify event. is_scrollable is always false */
		win->event_mask &= ~VisibilityChangeMask ;
		XSelectInput( win->display , win->my_window , win->event_mask) ;
		win->is_scrollable = 0 ;
	}

	XSetWindowBackgroundPixmap( win->display , win->my_window , pic) ;

	win->wall_picture_is_set = 1 ;

	if( win->window_exposed)
	{
		x_window_clear_all( win) ;
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	return  1 ;
}

int
x_window_unset_wall_picture(
	x_window_t *  win
	)
{
	if( win->drawable != win->my_window)
	{
		/*
		 * wall picture can not be used in double buffering mode.
		 */

		return  0 ;
	}

	if( ! win->wall_picture_is_set)
	{
		/* already unset */

		return  1 ;
	}

	if( win->is_transparent)
	{
		/*
		 * transparent background is not a wall picture :)
		 * this case is regarded as not using a wall picture.
		 */

		return  1 ;
	}

	if( !( win->event_mask & VisibilityChangeMask))
	{
		/* accepting VisibilityNotify event. is_scrollable is changed dynamically. */
		win->event_mask |= VisibilityChangeMask ;
		XSelectInput( win->display , win->my_window , win->event_mask) ;

		/* setting 0 in case the current status is VisibilityPartiallyObscured. */
		win->is_scrollable = 0 ;
	}

	XSetWindowBackgroundPixmap( win->display , win->my_window , None) ;
	XSetWindowBackground( win->display , win->my_window , win->bg_color) ;

	win->wall_picture_is_set = 0 ;

	if( win->window_exposed)
	{
		x_window_clear_all( win) ;
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	return  1 ;
}

int
x_window_set_transparent(
	x_window_t *  win ,
	x_picture_modifier_t *  pic_mod
	)
{
	Window  parent ;
	Window  root ;
	Window *  list ;
	u_int  n ;
	int  count ;

	win->pic_mod = pic_mod ;

	if( win->my_window == None)
	{
		/*
		 * If Window is not still created , actual drawing is delayed and
		 * ReparentNotify event will do transparent processing automatically after
		 * x_window_show().
		 */

		win->is_transparent = 1 ;

		goto  end ;
	}

	if( win->pic_mod || x_root_pixmap_available( win->display))
	{
		update_pic_transparent( win) ;
	}
	else
	{
		set_transparent( win , ParentRelative) ;

		XSetWindowBackgroundPixmap( win->display , win->my_window , ParentRelative) ;

		parent = win->my_window ;

		while( 1)
		{
			XQueryTree( win->display , parent , &root , &parent , &list , &n) ;
			XFree(list) ;

			if( parent == DefaultRootWindow(win->display))
			{
				break ;
			}

			XSetWindowBackgroundPixmap( win->display , parent , ParentRelative) ;
		}

		if( win->window_exposed)
		{
			x_window_clear_all( win) ;
			(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
		}
	}

end:
	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_set_transparent( win->children[count] , win->pic_mod) ;
	}

	return  1 ;
}

int
x_window_unset_transparent(
	x_window_t *  win
	)
{
	int  count ;
#if  0
	Window  parent ;
	Window  root ;
	Window *  list ;
	int  n ;
#endif

	if( ! win->is_transparent)
	{
		/* already unset */

		return  1 ;
	}

	unset_transparent( win) ;

#if  0
	/*
	 * XXX
	 * is this necessary ?
	 */
	parent = win->my_window ;

	for( count = 0 ; count < 2 ; count ++)
	{
		XQueryTree( win->display , parent , &root , &parent , &list , &n) ;
		XFree(list) ;
		if( parent == DefaultRootWindow(win->display))
		{
			break ;
		}

		XSetWindowBackgroundPixmap( win->display , parent , None) ;
	}
#endif

	if( win->window_exposed)
	{
		x_window_clear_all( win) ;
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_unset_transparent( win->children[count]) ;
	}

	return  1 ;
}

/*
 * Double buffering.
 */
int
x_window_use_pixmap(
	x_window_t *  win
	)
{
	win->use_pixmap = 1 ;

	win->event_mask &= ~VisibilityChangeMask ;

	/* always true */
	win->is_scrollable = 1 ;

	return  1 ;
}

int
x_window_set_cursor(
	x_window_t *  win ,
	u_int  cursor_shape
	)
{
	win->cursor_shape = cursor_shape ;

	return  1 ;
}

int
x_window_set_fg_color(
	x_window_t *  win ,
	u_long  fg_color
	)
{
	XSetForeground( win->display , win->gc , fg_color) ;
	win->fg_color = fg_color ;

	return  1 ;
}

int
x_window_set_bg_color(
	x_window_t *  win ,
	u_long  bg_color
	)
{
	XSetBackground( win->display , win->gc , bg_color) ;

	if( ! win->is_transparent && ! win->wall_picture_is_set)
	{
		XSetWindowBackground( win->display , win->my_window , bg_color) ;
	}

	win->bg_color = bg_color ;

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

	/*
	 * on the contrary , the root window does not have the ref of parent , but that of win_man.
	 */
	child->win_man = NULL ;
	child->parent = win ;
	child->x = x ;
	child->y = y ;
	child->is_mapped = map ;

	win->children[ win->num_of_children ++] = child ;

	return  1 ;
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

int
x_window_show(
	x_window_t *  win ,
	int  hint
	)
{
	XGCValues  gc_value ;
	int  count ;

	if( win->my_window)
	{
		/* already shown */

		return  0 ;
	}

	if( win->parent)
	{
		win->display = win->parent->display ;
		win->screen = win->parent->screen ;
		win->parent_window = win->parent->my_window ;
	}

	if( hint & XNegative)
	{
		win->x += (DisplayWidth( win->display , win->screen) - ACTUAL_WIDTH(win)) ;
	}

	if( hint & YNegative)
	{
		win->y += (DisplayHeight( win->display , win->screen) - ACTUAL_HEIGHT(win)) ;
	}

	win->my_window = XCreateSimpleWindow(
		win->display , win->parent_window ,
		win->x , win->y , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
		0 , win->fg_color , win->bg_color) ;

	if( win->use_pixmap)
	{
		win->drawable = win->pixmap = XCreatePixmap( win->display , win->parent_window ,
				ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				DefaultDepth( win->display , win->screen)) ;
	}
	else
	{
		win->drawable = win->my_window ;
		win->pixmap = 0 ;
	}

	if( win->cursor_shape)
	{
		XDefineCursor( win->display , win->my_window ,
			XCreateFontCursor( win->display , win->cursor_shape)) ;
	}

#ifdef  USE_TYPE_XFT
	win->xft_draw = XftDrawCreate( win->display , win->drawable ,
					DefaultVisual( win->display , win->screen) ,
					DefaultColormap( win->display , win->screen)) ;
#endif

	/*
	 * graphic context.
	 */

	gc_value.graphics_exposures = 0 ;

	win->gc = XCreateGC( win->display , win->my_window ,
			GCGraphicsExposures , &gc_value) ;
	win->ch_gc = XCreateGC( win->display , win->my_window ,
			GCGraphicsExposures , &gc_value) ;

	if( win->parent == NULL)
	{
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
		size_hints.min_width = total_min_width( win) ;
		size_hints.min_height = total_min_height( win) ;
		size_hints.base_width = total_base_width( win) ;
		size_hints.base_height = total_base_height( win) ;

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

		wm_hints.window_group = x_window_manager_get_group( win->win_man) ;
		wm_hints.initial_state = NormalState ;	/* or IconicState */
		wm_hints.input = True ;			/* wants FocusIn/FocusOut */
		wm_hints.flags = WindowGroupHint | StateHint | InputHint ;
		XChangeProperty( win->display, win->my_window,
				 XInternAtom( win->display, "WM_CLIENT_LEADER", False),
				 XA_WINDOW, 32, PropModeReplace,
				 (unsigned char *)(&wm_hints.window_group), 1) ;

		/* notify to window manager */
	#if  1
		XmbSetWMProperties( win->display , win->my_window , win->app_name , win->app_name ,
			argv , argc , &size_hints , &wm_hints , &class_hint) ;
	#else
		XmbSetWMProperties( win->display , win->my_window , win->app_name , win->app_name ,
			argv , argc , &size_hints , &wm_hints , NULL) ;
	#endif

		protocols[0] = XA_DELETE_WINDOW(win->display) ;
		protocols[1] = XA_TAKE_FOCUS(win->display) ;

		XSetWMProtocols( win->display , win->my_window , protocols , 2) ;
	}

	/*
	 * This should be called after Window Manager settings, because
	 * x_set_{window|icon}_name() can be called in win->window_realized().
	 */
	if( win->window_realized)
	{
		(*win->window_realized)( win) ;
	}

	XSelectInput( win->display , win->my_window , win->event_mask) ;

#if  0
	{
		char *  locale ;

		if( ( locale = kik_get_locale()))
		{
			XChangeProperty( win->display , win->my_window ,
				XInternAtom( win->display , "WM_LOCALE_NAME" , False) ,
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
		XMapWindow( win->display , win->my_window) ;

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

	XMapWindow( win->display , win->my_window) ;
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

	XUnmapWindow( win->display , win->my_window) ;
	win->is_mapped = 0 ;

	return  1 ;
}

int
x_window_resize(
	x_window_t *  win ,
	u_int  width ,			/* excluding margin */
	u_int  height ,			/* excluding margin */
	x_event_dispatch_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	if( win->width == width && win->height == height)
	{
		return  0 ;
	}

	win->width = width ;
	win->height = height ;

	if( win->pixmap)
	{
		XFreePixmap( win->display , win->pixmap) ;
		win->pixmap = XCreatePixmap( win->display ,
			win->parent_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;
		win->drawable = win->pixmap ;

	#ifdef  USE_TYPE_XFT
		XftDrawChange( win->xft_draw , win->drawable) ;
	#endif
	}

	if( (flag & NOTIFY_TO_PARENT) && win->parent && win->parent->child_window_resized)
	{
		(*win->parent->child_window_resized)( win->parent , win) ;
	}

	XResizeWindow( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;

	if( (flag & NOTIFY_TO_MYSELF) && win->window_resized)
	{
		(*win->window_resized)( win) ;
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
	x_event_dispatch_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	return  x_window_resize( win , width - win->margin * 2 , height - win->margin * 2 , flag) ;
}

int
x_window_set_normal_hints(
	x_window_t *  win ,
	u_int  min_width ,
	u_int  min_height ,
	u_int  base_width ,
	u_int  base_height ,
	u_int  width_inc ,
	u_int  height_inc
	)
{
	XSizeHints  size_hints ;
	x_window_t *  root ;

	win->min_width = min_width ;
	win->min_height = min_height  ;
	win->base_width = base_width ;
	win->base_height = base_height  ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;

	root = x_get_root_window(win) ;

	/*
	 * these hints must be set at the same time !
	 */
	size_hints.width_inc = total_width_inc( root) ;
	size_hints.height_inc = total_height_inc( root) ;
	size_hints.min_width = total_min_width( root) ;
	size_hints.min_height = total_min_height( root) ;
	size_hints.base_width = total_base_width( root) ;
	size_hints.base_height = total_base_height( root) ;
	size_hints.flags = PMinSize | PResizeInc | PBaseSize ;

	XSetWMNormalHints( root->display , root->my_window , &size_hints) ;

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
		XInternAtom( root->display , "_MOTIF_WM_HINTS" , True) ,
		XInternAtom( root->display , "_MOTIF_WM_INFO" , True)) ;
#endif

	if( ( atom = XA_MWM_HINTS(root->display)) != None)
	{
		if( flag)
		{
			MWMHints_t  mwmhints = { MWM_HINTS_DECORATIONS, 0, 0, 0, 0 } ;

			XChangeProperty( root->display, root->my_window, atom , atom , 32,
				PropModeReplace, (u_char *)&mwmhints, sizeof(MWMHints_t)/4) ;
		}
		else
		{
			XDeleteProperty( root->display, root->my_window, atom) ;
		}
	}
	else
	{
		/* fall back to override redirect */

		XSetWindowAttributes  s_attr ;
		XWindowAttributes  g_attr ;

		XGetWindowAttributes( root->display , root->my_window , &g_attr) ;
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

		XChangeWindowAttributes( root->display , root->my_window ,
					 CWOverrideRedirect , &s_attr) ;

		if( g_attr.map_state != IsUnmapped)
		{
			XUnmapWindow( root->display , root->my_window) ;
			XMapWindow( root->display , root->my_window) ;
		}
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
	XMoveWindow( win->display , win->my_window , x , y) ;

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
	if( win->drawable == win->pixmap)
	{
		XSetForeground( win->display , win->gc , win->bg_color) ;
		XFillRectangle( win->display , win->drawable , win->gc ,
			x + win->margin , y + win->margin , width , height) ;
		XSetForeground( win->display , win->gc , win->fg_color) ;
	}
	else if( win->drawable == win->my_window)
	{
		XClearArea( win->display , win->drawable , x + win->margin , y + win->margin ,
			width , height , 0) ;
	}

	return  1 ;
}

int
x_window_clear_margin_area(
	x_window_t *  win
	)
{
	if( win->drawable == win->pixmap)
	{
		XSetForeground( win->display , win->gc , win->bg_color) ;

		XFillRectangle( win->display , win->drawable , win->gc ,
			0 , 0 , win->margin , ACTUAL_HEIGHT(win)) ;
		XFillRectangle( win->display , win->drawable , win->gc ,
			win->margin , 0 , win->width , win->margin) ;
		XFillRectangle( win->display , win->drawable , win->gc ,
			win->width + win->margin , 0 , win->margin , ACTUAL_HEIGHT(win)) ;
		XFillRectangle( win->display , win->drawable , win->gc ,
			win->margin , win->height + win->margin , win->width , win->margin) ;

		XSetForeground( win->display , win->gc , win->fg_color) ;
	}
	else if( win->drawable == win->my_window)
	{
		XClearArea( win->display , win->drawable ,
			0 , 0 , win->margin , ACTUAL_HEIGHT(win) , 0) ;
		XClearArea( win->display , win->drawable ,
			win->margin , 0 , win->width , win->margin , 0) ;
		XClearArea( win->display , win->drawable ,
			win->width + win->margin , 0 , win->margin , ACTUAL_HEIGHT(win) , 0) ;
		XClearArea( win->display , win->drawable ,
			win->margin , win->height + win->margin , win->width , win->margin , 0) ;
	}

	return  1 ;
}

int
x_window_clear_all(
	x_window_t *  win
	)
{
	if( win->drawable == win->pixmap)
	{
		XSetForeground( win->display , win->gc , win->bg_color) ;
		XFillRectangle( win->display , win->drawable , win->gc ,
			0 , 0 , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
		XSetForeground( win->display , win->gc , win->fg_color) ;
	}
	else if( win->drawable == win->my_window)
	{
		XClearArea( win->display , win->drawable , 0 , 0 ,
			ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) , 0) ;
	}

	return  1 ;
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
	XFillRectangle( win->display , win->drawable , win->gc , x + win->margin , y + win->margin ,
		width , height) ;

	return  1 ;
}

int
x_window_fill_with(
	x_window_t *  win ,
	u_long  color ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	if( color != win->fg_color)
	{
		XSetForeground( win->display , win->gc , color) ;
	}

	XFillRectangle( win->display , win->drawable , win->gc , x + win->margin , y + win->margin ,
		width , height) ;

	if( color != win->fg_color)
	{
		XSetForeground( win->display , win->gc , win->fg_color) ;
	}

	return  1 ;
}

int
x_window_fill_all(
	x_window_t *  win
	)
{
	XFillRectangle( win->display , win->drawable , win->gc , 0 , 0 ,
		ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;

	return  1 ;
}

int
x_window_fill_all_with(
	x_window_t *  win ,
	u_long  color
	)
{
	if( color != win->fg_color)
	{
		XSetForeground( win->display , win->gc , color) ;
	}

	XFillRectangle( win->display , win->drawable , win->gc ,
		0 , 0 , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;

	if( color != win->fg_color)
	{
		XSetForeground( win->display , win->gc , win->fg_color) ;
	}

	return  1 ;
}

/*
 * only for double buffering
 */
int
x_window_update_view(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	if( win->pixmap == win->drawable)
	{
		if( win->width < x + width)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" update size (x)%d (w)%d is overflowed.(screen width %d)\n" ,
				x , width , win->width) ;
		#endif

			width = win->width - x ;

		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " width is modified -> %d\n" , width) ;
		#endif
		}

		if( win->height < y + height)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" update size (y)%d (h)%d is overflowed.(screen height is %d)\n" ,
				y , height , win->height) ;
		#endif

			height = win->height - y ;

		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " height is modified -> %d\n" , height) ;
		#endif
		}

		XCopyArea( win->display , win->drawable , win->my_window , win->gc ,
			x + win->margin , y + win->margin , width , height ,
			x + win->margin , y + win->margin) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
x_window_update_view_all(
	x_window_t *  win
	)
{
	return	x_window_update_view( win , 0 , 0 , win->width , win->height) ;
}

void
x_window_idling(
	x_window_t *  win
	)
{
	int  count ;

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
}

int
x_window_receive_event(
	x_window_t *   win ,
	XEvent *  event
	)
{
	int  count ;

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
		 * no xim window will be opened at XFilterEvent() in x_window_manager_receive_next_event().
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

		if( x_root_pixmap_available( win->display) &&
			( event->type == PropertyNotify) &&
			( win == x_get_root_window( win)) &&
			( event->xproperty.atom == XA_XROOTPMAP_ID(win->display)))
		{
			notify_property_to_children( win) ;

			return  1 ;
		}

		if( event->type == MappingNotify && event->xmapping.request != MappingPointer)
		{
			if( win->win_man)
			{
#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
						 " MappingNotify serial #%d\n", event->xmapping.serial) ;
#endif
				XRefreshKeyboardMapping( &(event->xmapping));
				x_window_manager_update_modifier_mapping( win->win_man, event->xmapping.serial) ;
				/* have to  rocess only once */
				return 1 ;
			}

			if( win->mapping_notify){
				(*win->mapping_notify)( win);
			}
		}
		return  0 ;
	}
#ifndef  DISABLE_XDND
	if( x_dnd_filter_event( event, win) != 0)
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

	#if  1
		/* root window event only */
		if( win->parent == NULL)
	#endif
		{
			notify_focus_in_to_children( win) ;
		}
	}
	else if( event->type == FocusOut)
	{
	#ifdef  __DEBUG
		kik_debug_printf( "FOCUS OUT %p\n" , event->xany.window) ;
	#endif

	#if  1
		/* root window event only */
		if( win->parent == NULL)
	#endif
		{
			notify_focus_out_to_children( win) ;
		}
	}
	else if( event->type == MotionNotify)
	{
		XEvent  ahead ;

		while( XEventsQueued(win->display , QueuedAfterReading))
		{
			XPeekEvent( win->display , &ahead) ;

			if( ahead.type != MotionNotify || ahead.xmotion.window != event->xmotion.window)
			{
				break ;
			}

			XNextEvent( win->display , event) ;
		}

		event->xmotion.x -= win->margin ;
		event->xmotion.y -= win->margin ;

		if( win->button_motion)
		{
			(*win->button_motion)( win , &event->xmotion) ;
		}

		if( win->button_is_pressing)
		{
			/* following button motion ... */

			win->prev_button_press_event.x = event->xmotion.x ;
			win->prev_button_press_event.y = event->xmotion.y ;
			win->prev_button_press_event.time = event->xmotion.time ;
		}
	}
	else if( event->type == ButtonRelease)
	{
		event->xbutton.x -= win->margin ;
		event->xbutton.y -= win->margin ;

		if( win->button_released)
		{
			(*win->button_released)( win , &event->xbutton) ;
		}

		win->button_is_pressing = 0 ;
	}
	else if( event->type == ButtonPress)
	{
		event->xbutton.x -= win->margin ;
		event->xbutton.y -= win->margin ;

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

		if( win->button_pressed)
		{
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
	}
	else if( event->type == Expose /* && event->xexpose.count == 0 */)
	{
		int  x ;
		int  y ;
		u_int  width ;
		u_int  height ;

		if( event->xexpose.x < win->margin)
		{
			x = 0 ;

			if( x + event->xexpose.width > win->width)
			{
				width = win->width ;
			}
			else if( event->xexpose.width < (win->margin - event->xexpose.x))
			{
				width = 0 ;
			}
			else
			{
				width = event->xexpose.width - (win->margin - event->xexpose.x) ;
			}
		}
		else
		{
			x = event->xexpose.x - win->margin ;

			if( x + event->xexpose.width > win->width)
			{
				width = win->width - x ;
			}
			else
			{
				width = event->xexpose.width ;
			}
		}

		if( event->xexpose.y < win->margin)
		{
			y = 0 ;

			if( y + event->xexpose.height > win->height)
			{
				height = win->height ;
			}
			else if( event->xexpose.height < (win->margin - event->xexpose.y))
			{
				height = 0 ;
			}
			else
			{
				height = event->xexpose.height - (win->margin - event->xexpose.y) ;
			}
		}
		else
		{
			y = event->xexpose.y - win->margin ;

			if( y + event->xexpose.height > win->height)
			{
				height = win->height - y ;
			}
			else
			{
				height = event->xexpose.height ;
			}
		}

		if( ! x_window_update_view( win , x , y , width , height))
		{
			if( win->window_exposed)
			{
				(*win->window_exposed)( win , x , y , width , height) ;
			}
		}
	}
	else if( event->type == ConfigureNotify)
	{
		int  is_changed ;
		XEvent  next_ev ;

		is_changed = 0 ;

		if( event->xconfigure.x != win->x || event->xconfigure.y != win->y)
		{
			/*
			 * for fvwm2 style virtual screen.
			 */
			if( abs( event->xconfigure.x - win->x) % DisplayWidth(win->display , win->screen) != 0 ||
				abs( event->xconfigure.y - win->y) % DisplayHeight(win->display , win->screen) != 0 ||
				( event->xconfigure.x < 0 && event->xconfigure.x + (int)ACTUAL_WIDTH(win) > 0) ||
				( event->xconfigure.x > 0 &&
					event->xconfigure.x + (int)ACTUAL_WIDTH(win)
						> DisplayWidth( win->display , win->screen)) ||
				( event->xconfigure.y < 0 && event->xconfigure.y + (int)ACTUAL_HEIGHT(win) > 0) ||
				( event->xconfigure.y > 0 &&
					event->xconfigure.y + (int)ACTUAL_HEIGHT(win)
						> DisplayHeight( win->display , win->screen)) )
			{
				is_changed = 1 ;
			}

			win->x = event->xconfigure.x ;
			win->y = event->xconfigure.y ;
		}

		if( event->xconfigure.width != ACTUAL_WIDTH(win) ||
			event->xconfigure.height != ACTUAL_HEIGHT(win))
		{
			win->width = event->xconfigure.width - win->margin * 2 ;
			win->height = event->xconfigure.height - win->margin * 2 ;

			if( win->pixmap)
			{
				XFreePixmap( win->display , win->pixmap) ;
				win->pixmap = XCreatePixmap( win->display ,
					win->parent_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
					DefaultDepth( win->display , win->screen)) ;
				win->drawable = win->pixmap ;

			#ifdef  USE_TYPE_XFT
				XftDrawChange( win->xft_draw , win->drawable) ;
			#endif
			}

			x_window_clear_all( win) ;

			if( win->window_resized)
			{
				(*win->window_resized)( win) ;
			}

			x_xic_resized( win) ;

			is_changed = 1 ;
		}

		/*
		 * transparent processing.
		 */

		if( is_changed &&
			XCheckMaskEvent( win->display , StructureNotifyMask , &next_ev))
		{
			if( next_ev.type == ConfigureNotify)
			{
				is_changed = 0 ;
			}

			XPutBackEvent( win->display , &next_ev) ;
		}

		if( is_changed)
		{
			notify_configure_to_children( win) ;
		}
	}
	else if( event->type == ReparentNotify)
	{
		/*
		 * transparent processing.
		 */

		win->x = event->xreparent.x ;
		win->y = event->xreparent.y ;

		notify_reparent_to_children( win) ;
	}
	else if( event->type == SelectionClear)
	{
		if( win->selection_cleared)
		{
			(*win->selection_cleared)( win , &event->xselectionclear) ;
		}

		x_window_manager_clear_selection( x_get_root_window( win)->win_man , win) ;
	}
	else if( event->type == SelectionRequest)
	{
		Atom  xa_utf8_string ;
		Atom  xa_compound_text ;
		Atom  xa_multiple ;
		Atom  xa_targets ;
		Atom  xa_text ;

		xa_compound_text = XA_COMPOUND_TEXT(win->display) ;
		xa_targets = XA_TARGETS(win->display) ;
#ifdef  DEBUG
		xa_multiple = XA_MULTIPLE(win->display) ;
#endif
		xa_text = XA_TEXT(win->display) ;
		xa_utf8_string = XA_UTF8_STRING(win->display) ;

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
			if( win->utf8_selection_requested)
			{
				(*win->utf8_selection_requested)( win , &event->xselectionrequest ,
					xa_utf8_string) ;
			}
		}
		else if( event->xselectionrequest.target == xa_targets)
		{
			Atom  targets[4] ;

			targets[0] =XA_STRING ;
			targets[1] =xa_text ;
			targets[2] =xa_compound_text ;
			targets[3] =xa_utf8_string ;
			x_window_send_selection( win , &event->xselectionrequest , (u_char *)(&targets) , sizeof(targets) , XA_ATOM) ;
		}
#ifdef  DEBUG
		else if( event->xselectionrequest.target == xa_multiple)
		{
			kik_debug_printf("MULTIPLE requested(not yet implemented)\n") ;
		}
#endif
		else
		{
			x_window_send_selection( win , &event->xselectionrequest , NULL , 0 , 0) ;
		}
	}
	else if( event->type == SelectionNotify)
	{
		Atom  xa_utf8_string ;
		Atom  xa_compound_text ;
		Atom  xa_text ;
		Atom  xa_selection ;

		xa_compound_text = XA_COMPOUND_TEXT(win->display) ;
		xa_text = XA_TEXT(win->display) ;
		xa_utf8_string = XA_UTF8_STRING(win->display) ;
		xa_selection = XA_SELECTION(win->display) ;

		if( event->xselection.property == None)
		{
			/*
			 * Selection request failed.
			 * Retrying with xa_compound_text => xa_text => XA_STRING
			 */

			if( event->xselection.target == xa_utf8_string)
			{
				XConvertSelection( win->display , XA_PRIMARY , xa_compound_text ,
					xa_selection , win->my_window , CurrentTime) ;
			}
			else if( event->xselection.target == xa_compound_text)
			{
				XConvertSelection( win->display , XA_PRIMARY , xa_text ,
					xa_selection , win->my_window , CurrentTime) ;
			}
			else if( event->xselection.target == xa_text)
			{
				XConvertSelection( win->display , XA_PRIMARY , XA_STRING ,
					xa_selection , win->my_window , CurrentTime) ;
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
				if( XGetWindowProperty( win->display , event->xselection.requestor ,
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

				if( event->xselection.target == XA_STRING ||
					 event->xselection.target == xa_text ||
					 event->xselection.target == xa_compound_text)
				{
					if( win->xct_selection_notified)
					{
						(*win->xct_selection_notified)(
							win , ct.value , ct.nitems) ;
					}
				}
				else if( event->xselection.target == xa_utf8_string)
				{
					if( win->utf8_selection_notified)
					{
						(*win->utf8_selection_notified)(
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

		XDeleteProperty( win->display, event->xselection.requestor,
			event->xselection.property) ;
	}
	else if( event->type == VisibilityNotify)
	{
		/* this event is changeable in run time. */

		if( win->event_mask & VisibilityChangeMask)
		{
			if( event->xvisibility.state == VisibilityPartiallyObscured)
			{
				win->is_scrollable = 0 ;
			}
			else if( event->xvisibility.state == VisibilityUnobscured)
			{
				win->is_scrollable = 1 ;
			}
		}
	}
	else if( event->type == ClientMessage)
	{
		if( event->xclient.format == 32 &&
			event->xclient.data.l[0] == XA_DELETE_WINDOW( win->display))
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
		#ifdef  DEBUG
		if( event->xclient.format == 32 &&
			event->xclient.data.l[0] == XA_TAKE_FOCUS( win->display))
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" TakeFocus message is received.\n") ;
		}
		#endif
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

int
x_window_is_scrollable(
	x_window_t *  win
	)
{
	return  win->is_scrollable ;
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

	XCopyArea( win->display , win->drawable , win->drawable , win->gc ,
		win->margin , win->margin + boundary_start + height ,	/* src */
		win->width , boundary_end - boundary_start - height ,	/* size */
		win->margin , win->margin + boundary_start) ;		/* dst */

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

	if( boundary_start < 0 || boundary_end > win->height || boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " boundary start %d end %d height %d\n" ,
			boundary_start , boundary_end , height) ;
	#endif

		return  0 ;
	}

	XCopyArea( win->display , win->drawable , win->drawable , win->gc ,
		win->margin , win->margin + boundary_start ,
		win->width , boundary_end - boundary_start - height ,
		win->margin , win->margin + boundary_start + height) ;

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

	XCopyArea( win->display , win->drawable , win->drawable , win->gc ,
		win->margin + boundary_start + width , win->margin ,	/* src */
		boundary_end - boundary_start - width , win->height ,	/* size */
		win->margin + boundary_start , win->margin) ;		/* dst */

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

	XCopyArea( win->display , win->drawable , win->drawable , win->gc ,
		win->margin + boundary_start , win->margin ,
		boundary_end - boundary_start - width , win->height ,
		win->margin + boundary_start + width , win->margin) ;

	return  1 ;
}

int
x_window_draw_decsp_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,		/* can be NULL */
	x_color_t *  bg_color ,		/* can be NULL */
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	if( font->decsp_font)
	{
		if( fg_color)
		{
			XSetForeground( win->display , win->ch_gc , fg_color->pixel) ;
		}

		if( bg_color)
		{
			XSetBackground( win->display , win->ch_gc , bg_color->pixel) ;
			x_decsp_font_draw_image_string( font->decsp_font ,
						  win->display , win->drawable , win->ch_gc ,
						  x + win->margin , y + win->margin , str , len) ;
		}
		else
		{
			x_decsp_font_draw_string( font->decsp_font ,
						  win->display , win->drawable , win->ch_gc ,
						  x + win->margin , y + win->margin , str , len) ;
		}
		return  1 ;
	}
#ifdef  USE_TYPE_XCORE
	else if( font->xfont)
	{
		if( bg_color)
		{
			return  x_window_draw_image_string( win , font , fg_color , bg_color ,
					x , y , str , len) ;
		}
		else
		{
			return  x_window_draw_string( win , font , fg_color , x , y , str , len) ;
		}
	}
#endif
	else
	{
		return  0 ;
	}
}

#ifdef  USE_TYPE_XCORE
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
	XSetFont( win->display , win->ch_gc , font->xfont->fid) ;
	XSetForeground( win->display , win->ch_gc , fg_color->pixel) ;

	XDrawString( win->display , win->drawable , win->ch_gc ,
		x + font->x_off + win->margin , y + win->margin , str , len) ;

	if( font->is_double_drawing)
	{
		XDrawString( win->display , win->drawable , win->ch_gc ,
			x + font->x_off + win->margin + 1 , y + win->margin , str , len) ;
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
	XSetFont( win->display , win->ch_gc , font->xfont->fid) ;
	XSetForeground( win->display , win->ch_gc , fg_color->pixel) ;

	XDrawString16( win->display , win->drawable , win->ch_gc ,
		x + font->x_off + win->margin , y + win->margin , str , len) ;

	if( font->is_double_drawing)
	{
		XDrawString16( win->display , win->drawable , win->ch_gc ,
			x + font->x_off + win->margin + 1 , y + win->margin , str , len) ;
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
	XSetFont( win->display , win->ch_gc , font->xfont->fid) ;
	XSetForeground( win->display , win->ch_gc , fg_color->pixel) ;
	XSetBackground( win->display , win->ch_gc , bg_color->pixel) ;

	XDrawImageString( win->display , win->drawable , win->ch_gc ,
		x + font->x_off + win->margin , y + win->margin , str , len) ;

	if( font->is_double_drawing)
	{
		XDrawString( win->display , win->drawable , win->ch_gc ,
			x + font->x_off + win->margin + 1 , y + win->margin , str , len) ;
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
	XSetFont( win->display , win->ch_gc , font->xfont->fid) ;
	XSetForeground( win->display , win->ch_gc , fg_color->pixel) ;
	XSetBackground( win->display , win->ch_gc , bg_color->pixel) ;

	XDrawImageString16( win->display , win->drawable , win->ch_gc ,
		x + font->x_off + win->margin , y + win->margin , str , len) ;

	if( font->is_double_drawing)
	{
		XDrawString16( win->display , win->drawable , win->ch_gc ,
			x + font->x_off + win->margin + 1 , y + win->margin , str , len) ;
	}

	return  1 ;
}
#endif

#ifdef  USE_TYPE_XFT
int
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
	XftDrawString8( win->xft_draw , fg_color , font->xft_font ,
		x + win->margin , y + win->margin , str , len) ;

	if( font->is_double_drawing)
	{
		XftDrawString8( win->xft_draw , fg_color , font->xft_font ,
			x + win->margin + 1 , y + win->margin , str , len) ;
	}

	return  1 ;
}

int
x_window_xft_draw_string32(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	XftChar32 *  str ,
	u_int  len
	)
{
	XftDrawString32( win->xft_draw , fg_color , font->xft_font ,
		x + win->margin , y + win->margin , str , len) ;

	if( font->is_double_drawing)
	{
		XftDrawString32( win->xft_draw , fg_color , font->xft_font ,
			x + win->margin + 1 , y + win->margin , str , len) ;
	}

	return  1 ;
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
	XDrawLine( win->display , win->drawable , win->gc , x1 , y1 , x2 , y1) ;
	XDrawLine( win->display , win->drawable , win->gc , x1 , y1 , x1 , y2) ;
	XDrawLine( win->display , win->drawable , win->gc , x2 , y2 , x2 , y1) ;
	XDrawLine( win->display , win->drawable , win->gc , x2 , y2 , x1 , y2) ;

	return  1 ;
}

int
x_window_set_selection_owner(
	x_window_t *  win ,
	Time  time
	)
{
	XSetSelectionOwner( win->display , XA_PRIMARY , win->my_window , time) ;

	if( win->my_window != XGetSelectionOwner( win->display , XA_PRIMARY))
	{
		return  0 ;
	}
	else
	{
		return  x_window_manager_own_selection( x_get_root_window( win)->win_man , win) ;
	}
}

int
x_window_xct_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
	XConvertSelection( win->display , XA_PRIMARY , XA_COMPOUND_TEXT(win->display) ,
		XA_SELECTION(win->display) , win->my_window , time) ;

	return  1 ;
}

int
x_window_utf8_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
	XConvertSelection( win->display , XA_PRIMARY , XA_UTF8_STRING(win->display) ,
		XA_SELECTION(win->display) , win->my_window , time) ;

	return  1 ;
}

int
x_window_send_selection(
	x_window_t *  win ,
	XSelectionRequestEvent *  req_ev ,
	u_char *  sel_str ,
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

	if( sel_str == NULL)
	{
		res_ev.xselection.property = None ;
	}
	else
	{
		XChangeProperty( win->display , req_ev->requestor ,
			req_ev->property , sel_type ,
			8 , PropModeReplace , sel_str , sel_len) ;
		res_ev.xselection.property = req_ev->property ;
	}

	XSendEvent( win->display , res_ev.xselection.requestor , False , 0 , &res_ev) ;

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

	if( XmbTextListToTextProperty( root->display , (char**)&name , 1 , XStdICCTextStyle , &prop)
		>= Success)
	{
		XSetWMName( root->display , root->my_window , &prop) ;

		XFree( prop.value) ;
	}
	else
	{
		/* XXX which is better , doing this or return 0 without doing anything ? */
		XStoreName( root->display , root->my_window , name) ;
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

	if( XmbTextListToTextProperty( root->display , (char**)&name , 1 , XStdICCTextStyle , &prop)
		>= Success)
	{
		XSetWMIconName( root->display , root->my_window , &prop) ;

		XFree( prop.value) ;
	}
	else
	{
		/* XXX which is better , doing this or return 0 without doing anything ? */
		XSetIconName( root->display , root->my_window , name) ;
	}

	return  1 ;
}

int
x_window_set_icon(
	x_window_t *  win,
	Pixmap  icon,
	Pixmap  mask,
	u_int32_t *  cardinal
	)
{
	XWMHints *hints ;

	/* set extended window manager hint's icon */
	if( cardinal && cardinal[0] && cardinal[1])
	{
		/*it should be possible to set multiple icons...*/
		XChangeProperty( win->display, win->my_window,
				 XA_NET_WM_ICON( win->display),
				 XA_CARDINAL, 32, PropModeReplace,
				 (unsigned char *)(cardinal),
				 /* (cardinal[0])*(cardinal[1])
				  *          = width * height */
				 (cardinal[0])*(cardinal[1]) +2) ;
	}
	/* set old style window manager hint's icon */
	hints = NULL ;
	if (icon || mask)
	{
		hints = XGetWMHints( win->display, win->my_window) ;
	}
	if (!hints){
		hints = XAllocWMHints() ;
	}
	if (!hints){
		return 0 ;
	}

	if( icon)
	{
		hints->flags |= IconPixmapHint ;
		hints->icon_pixmap = icon ;
	}
	if( mask)
	{
		hints->flags |= IconMaskHint ;
		hints->icon_mask = mask ;
	}

	XSetWMHints( win->display, win->my_window, hints) ;
	XFree( hints) ;

	return 1 ;
}

int
x_window_get_visible_geometry(
	x_window_t *  win ,
	int *  x ,		/* x relative to parent window */
	int *  y ,		/* y relative to parent window */
	int *  my_x ,		/* x relative to my window */
	int *  my_y ,		/* y relative to my window */
	u_int *  width ,
	u_int *  height
	)
{
	Window  child ;
	int screen_width ;
	int screen_height ;

	XTranslateCoordinates( win->display , win->my_window ,
		DefaultRootWindow( win->display) , 0 , 0 ,
		x , y , &child) ;

	screen_width = DisplayWidth( win->display , win->screen) ;
	screen_height = DisplayHeight( win->display , win->screen) ;

	if( *x >= screen_width || *y >= screen_height)
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

	if( *x + *width > screen_width)
	{
		*width = screen_width - *x ;
	}

	if( *y + *height > screen_height)
	{
		*height = screen_height - *y ;
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

XModifierKeymap *
x_window_get_modifier_mapping(
	x_window_t *  win
	)
{
	return  x_window_manager_get_modifier_mapping( win->win_man) ;
}

#if  0
/*
 * XXX
 * at the present time , not used and not maintained.
 */
int
x_window_paste(
	x_window_t *  win ,
	Drawable  src ,
	int  src_x ,
	int  src_y ,
	u_int  src_width ,
	u_int  src_height ,
	int  dst_x ,
	int  dst_y
	)
{
	if( win->width < dst_x + src_width)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " size (x)%d (w)%d is overflowed.(screen width %d)\n" ,
			dst_x , src_width , win->width) ;
	#endif

		src_width = win->width - dst_x ;

		kik_warn_printf( KIK_DEBUG_TAG " width is modified -> %d\n" , src_width) ;
	}

	if( win->height < dst_y + src_height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " size (y)%d (h)%d is overflowed.(screen height is %d)\n" ,
			dst_y , src_height , win->height) ;
	#endif

		src_height = win->height - dst_y ;

		kik_warn_printf( KIK_DEBUG_TAG " height is modified -> %d\n" , src_height) ;
	}

	XCopyArea( win->display , src , win->drawable , win->gc ,
		src_x , src_y , src_width , src_height ,
		dst_x + win->margin , dst_y + win->margin) ;

	return  1 ;
}
#endif

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
