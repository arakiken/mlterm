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
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* realloc/free */

#include  "x_xic.h"
#include  "x_window_manager.h"
#include  "x_imagelib.h"

#ifndef  DISABLE_XDND
#include  "x_dnd.h"
#endif


#define  MAX_CLICK  3			/* max is triple click */
#define  WM_USER_PAINT  WM_USER


#if  0
#define  __DEBUG
#endif


typedef struct cp_parser
{
	mkf_parser_t  parser ;
	
	mkf_charset_t  cs ;
	size_t  cp_size ;
	
} cp_parser_t ;


/* --- static variables --- */

static int  click_interval = 250 ;	/* millisecond, same as xterm. */


/* --- static functions --- */

/*
 * For code point parser.
 */
 
static void
cp_parser_init(
	mkf_parser_t *  parser
	)
{
	cp_parser_t *  cp_parser ;

	cp_parser = (cp_parser_t*) parser ;
	
	mkf_parser_init( parser) ;
	cp_parser->cs = UNKNOWN_CS ;
	cp_parser->cp_size = 1 ;
}

static void
cp_parser_set_str_dummy(
	mkf_parser_t *  parser ,
	u_char *  str ,
	size_t  size
	)
{
	/* do nothing */
}

static int
cp_parser_set_str(
	mkf_parser_t *  parser ,
	u_char *  str ,
	size_t  size ,
	mkf_charset_t  cs
	)
{
	cp_parser_t *  cp_parser ;

	cp_parser = (cp_parser_t*) parser ;

	cp_parser->parser.str = str ;
	cp_parser->parser.left = size ;
	cp_parser->parser.marked_left = 0 ;
	cp_parser->parser.is_eos = 0 ;
	cp_parser->cs = cs ;

	if( cs == ISO10646_UCS4_1)
	{
		cp_parser->cp_size = 4 ;
	}
	else if( IS_BIWIDTH_CS(cs))
	{
		cp_parser->cp_size = 2 ;
	}
	else
	{
		cp_parser->cp_size = 1 ;
	}

	return  1 ;
}

static void
cp_parser_delete(
	mkf_parser_t *  parser
	)
{
	free( parser) ;
}

static int
cp_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	cp_parser_t *  cp_parser ;
	size_t  count ;

	cp_parser = (cp_parser_t*) parser ;

	if( cp_parser->parser.is_eos)
	{
		return  0 ;
	}

	if( cp_parser->parser.left < cp_parser->cp_size)
	{
		cp_parser->parser.is_eos = 1 ;
		
		return  0 ;
	}

	for( count = 0 ; count < cp_parser->cp_size ; count ++)
	{
		ch->ch[count] = cp_parser->parser.str[count] ;
	}

	mkf_parser_n_increment( cp_parser, count) ;
	
	ch->size = count ;
	ch->cs = cp_parser->cs ;
	ch->property = 0 ;

	return  1 ;
}


/*
 * only for double buffering
 */
static int
restore_view(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
#ifndef  USE_WIN32API
	if( win->buffer == win->drawable)
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
#endif
	{
		return  0 ;
	}
}

static int
restore_view_all(
	x_window_t *  win
	)
{
	return	restore_view( win , 0 , 0 , win->width , win->height) ;
}


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

static u_int
get_key_state(void)
{
	u_int  state ;

	state = 0 ;
	if( GetKeyState(VK_SHIFT) < 0)
	{
		state |= ShiftMask ;
	}
	if( GetKeyState(VK_CONTROL) < 0)
	{
		state |= ControlMask ;
	}
	if( GetKeyState(VK_MENU) < 0)
	{
		state |= ModMask ;
	}

	return  state ;
}

static int
draw_string(
	x_window_t *  win ,
	x_font_t *  font ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	u_char *  str2 ;

	str2 = NULL ;

	if( font->conv)
	{
		if( ( str2 = malloc( len * 8)))	/* assume utf8 */
		{
			cp_parser_set_str( win->cp_parser, str, len, FONT_CS(font->id)) ;
			len = (*font->conv->convert)( font->conv, str2, len * 8, win->cp_parser) ;

			if( len > 0)
			{
				str = str2 ;
			}
		}
	}

	TextOut( win->gc, x + (font->is_var_col_width ? 0 : font->x_off) + win->margin,
		y + win->margin, str, len) ;
		
	if( font->is_double_drawing)
	{
		TextOut( win->gc, x + (font->is_var_col_width ? 0 : font->x_off) + win->margin + 1,
				y + win->margin, str, len) ;
	}

	if( str2)
	{
		free( str2) ;
	}

	return  1 ;
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

	win->parent_window = None ;
	win->my_window = None ;

	win->drawable = None ;
	win->buffer = None ;

	win->fg_color = RGB_BLACK ;	/* black */
	win->bg_color = RGB_WHITE ;	/* white */

	win->parent = NULL ;
	win->children = NULL ;
	win->num_of_children = 0 ;

	win->use_buffer = 0 ;

	win->pic_mod = NULL ;

	win->wall_picture_is_set = 0 ;
	win->is_transparent = 0 ;

	win->cursor_shape = 0 ;

#ifdef  USE_WIN32API
	win->event_mask = 0 ;
#else
	win->event_mask = ExposureMask | VisibilityChangeMask | FocusChangeMask | PropertyChangeMask ;
#endif

	/* if visibility is partially obscured , scrollable will be 0. */
	win->is_scrollable = 1 ;

	/* This flag will map window automatically in x_window_show(). */
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

	win->xim = NULL ;
	win->xim_listener = NULL ;
	
#ifdef  USE_WIN32API
	win->xic = NULL ;
	x_xic_activate( win, NULL, NULL) ;
	
	if( ( win->cp_parser = malloc( sizeof( cp_parser_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " cp_parser_t malloc failed.\n") ;
	#endif
	}
	else
	{
		cp_parser_init( win->cp_parser) ;
		win->cp_parser->init = cp_parser_init ;
		win->cp_parser->set_str = cp_parser_set_str_dummy ;
		win->cp_parser->delete = cp_parser_delete ;
		win->cp_parser->next_char = cp_parser_next_char ;
	}

	win->update_window_flag = 0 ;
#else
	win->xic = NULL ;
#endif

	win->prev_clicked_time = 0 ;
	win->prev_clicked_button = -1 ;
	win->click_num = 0 ;
	win->button_is_pressing = 0 ;
	win->dnd = NULL ;
	win->app_name = "mlterm" ;

	win->icon_pix = None;
	win->icon_mask = None;
	win->icon_card = NULL;
	
	win->window_realized = NULL ;
	win->window_finalized = NULL ;
	win->window_exposed = NULL ;
	win->update_window = NULL ;
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
	win->set_xdnd_config = NULL ;

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

	free( win->children) ;

#ifdef  USE_WIN32API
	if( win->bg_color != RGB_WHITE)
	{
		DeleteObject( (HBRUSH)GetClassLong( win->my_window, GCL_HBRBACKGROUND)) ;
	}
#else
	x_window_manager_clear_selection( x_get_root_window( win)->win_man , win) ;

	if( win->buffer)
	{
		XFreePixmap( win->display , win->buffer) ;
	}

	XDestroyWindow( win->display , win->my_window) ;

	if( win->icon_pix){
		XFreePixmap( win->display , win->icon_pix) ;
	}
	if( win->icon_mask){
		XFreePixmap( win->display , win->icon_mask) ;
	}
	free( win->icon_card) ;
#endif
	
	x_xic_deactivate( win) ;

	(*win->cp_parser->delete)( win->cp_parser) ;

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

#ifndef  USE_WIN32API
	if( event_mask & ButtonMotionMask)
	{
		event_mask &= ~ButtonMotionMask ;
		event_mask |= ( Button1MotionMask | Button2MotionMask |
			Button3MotionMask | Button4MotionMask | Button5MotionMask) ;
	}

	win->event_mask |= event_mask ;
#endif

	return  1 ;
}

int
x_window_add_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
#ifndef  USE_WIN32API
	if( event_mask & ButtonMotionMask)
	{
		event_mask &= ~ButtonMotionMask ;
		event_mask |= ( Button1MotionMask | Button2MotionMask |
			Button3MotionMask | Button4MotionMask | Button5MotionMask) ;
	}

	win->event_mask |= event_mask ;

	XSelectInput( win->display , win->my_window , win->event_mask) ;
#endif

	return  1 ;
}

int
x_window_remove_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
#ifndef  USE_WIN32API
	if( event_mask & ButtonMotionMask)
	{
		event_mask &= ~ButtonMotionMask ;
		event_mask |= ( Button1MotionMask | Button2MotionMask |
			Button3MotionMask | Button4MotionMask | Button5MotionMask) ;
	}

	win->event_mask &= ~event_mask ;

	XSelectInput( win->display , win->my_window , win->event_mask) ;
#endif

	return  1 ;
}

int
x_window_ungrab_pointer(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_window_set_wall_picture(
	x_window_t *  win ,
	Pixmap  pic
	)
{
#ifndef  USE_WIN32API
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
#endif

	return  1 ;
}

int
x_window_unset_wall_picture(
	x_window_t *  win
	)
{
#ifndef  USE_WIN32API
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
#endif

	return  1 ;
}

int
x_window_set_transparent(
	x_window_t *  win ,
	x_picture_modifier_t *  pic_mod
	)
{
#ifndef  USE_WIN32API
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
#endif

	return  1 ;
}

int
x_window_unset_transparent(
	x_window_t *  win
	)
{
#ifndef  USE_WIN32API
	int  count ;

	if( ! win->is_transparent)
	{
		/* already unset */

		return  1 ;
	}

	unset_transparent( win) ;

	if( win->window_exposed)
	{
		x_window_clear_all( win) ;
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_unset_transparent( win->children[count]) ;
	}
#endif

	return  1 ;
}

/*
 * Double buffering.
 */
int
x_window_use_buffer(
	x_window_t *  win
	)
{
#ifndef  USE_WIN32API
	win->use_buffer = 1 ;

	win->event_mask &= ~VisibilityChangeMask ;

	/* always true */
	win->is_scrollable = 1 ;
#endif

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
#ifndef  USE_WIN32API
	XSetForeground( win->display , win->gc , fg_color) ;
#endif
	
	win->fg_color = fg_color ;

	return  1 ;
}

int
x_window_set_bg_color(
	x_window_t *  win ,
	u_long  bg_color
	)
{
	HBRUSH  old_br ;

	if( win->my_window != None)
	{
		old_br = (HBRUSH)SetClassLong( win->my_window, GCL_HBRBACKGROUND,
						(LONG)CreateSolidBrush(bg_color)) ;
		if( win->bg_color != RGB_WHITE)
		{
			DeleteObject( old_br) ;
		}

		InvalidateRect( win->my_window, NULL, FALSE) ;
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
	int  count ;
	u_int  width ;
	u_int  height ;

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

#ifndef  USE_WIN32API
	if( hint & XNegative)
	{
		win->x += (DisplayWidth( win->display , win->screen) - ACTUAL_WIDTH(win)) ;
	}

	if( hint & YNegative)
	{
		win->y += (DisplayHeight( win->display , win->screen) - ACTUAL_HEIGHT(win)) ;
	}
#endif

	width = ACTUAL_WIDTH(win) + GetSystemMetrics(SM_CXEDGE) + GetSystemMetrics(SM_CXFRAME) ;
	height = ACTUAL_HEIGHT(win) + GetSystemMetrics(SM_CYEDGE) + GetSystemMetrics(SM_CYFRAME)
			+ GetSystemMetrics(SM_CYCAPTION) ;
#if  1
	kik_debug_printf( "%d %d %d %d %d %d %d\n", GetSystemMetrics(SM_CXEDGE),
		GetSystemMetrics(SM_CXBORDER), GetSystemMetrics(SM_CYEDGE),
		GetSystemMetrics(SM_CXFRAME),
		GetSystemMetrics(SM_CYBORDER), GetSystemMetrics(SM_CYCAPTION),
		GetSystemMetrics(SM_CYFRAME)) ;
#endif

        win->my_window = CreateWindow("MLTERM", "mlterm", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
        		width, height, NULL, NULL,
			win->display->hinst, NULL) ;

  	if( ! win->my_window)
        {
          	MessageBox(NULL,"Failed to create window.",NULL,MB_ICONSTOP) ;

          	return  0 ;
        }

	SetClassLong( win->my_window, GCL_HBRBACKGROUND,
		win->bg_color == RGB_BLACK ? (LONG)GetStockObject(WHITE_BRUSH) :
					(LONG)CreateSolidBrush(win->bg_color)) ;

#ifndef  USE_WIN32API
	if( win->use_buffer)
	{
		win->drawable = win->buffer = XCreatePixmap( win->display , win->parent_window ,
				ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				DefaultDepth( win->display , win->screen)) ;
	}
	else
#endif
	{
		win->drawable = win->my_window ;
		win->buffer = 0 ;
	}

	/*
	 * This should be called after Window Manager settings, because
	 * x_set_{window|icon}_name() can be called in win->window_realized().
	 */
	if( win->window_realized)
	{
		(*win->window_realized)( win) ;
	}

#ifndef  USE_WIN32API
	XSelectInput( win->display , win->my_window , win->event_mask) ;
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
		ShowWindow( win->my_window , SW_SHOWNORMAL) ;

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

  	ShowWindow( win->my_window , SW_SHOWNORMAL) ;
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

#ifndef  USE_WIN32API
	XUnmapWindow( win->display , win->my_window) ;
#endif

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

#ifndef  USE_WIN32API
	if( win->buffer)
	{
		XFreePixmap( win->display , win->buffer) ;
		win->buffer = XCreatePixmap( win->display ,
			win->parent_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;
		win->drawable = win->buffer ;
	}
#endif

	if( (flag & NOTIFY_TO_PARENT) && win->parent && win->parent->child_window_resized)
	{
		(*win->parent->child_window_resized)( win->parent , win) ;
	}

#ifndef  USE_WIN32API
	XResizeWindow( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
#endif

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
#ifndef  USE_WIN32API
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
#endif

	return  1 ;
}

int
x_window_set_override_redirect(
	x_window_t *  win ,
	int  flag
	)
{
#ifndef  USE_WIN32API
	x_window_t *  root ;
	XSetWindowAttributes  s_attr ;
	XWindowAttributes  g_attr ;

	root = x_get_root_window(win) ;

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
#endif

	return  1 ;
}

int
x_window_set_borderless_flag(
	x_window_t *  win ,
	int  flag
	)
{
#ifndef  USE_WIN32API
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
		x_window_set_override_redirect( win , flag) ;
	}
#endif

	return  1 ;
}

int
x_window_move(
	x_window_t *  win ,
	int  x ,
	int  y
	)
{
#ifndef  USE_WIN32API
	XMoveWindow( win->display , win->my_window , x , y) ;
#endif

	return  1 ;
}

int
x_window_clear(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	RECT  r ;

	r.left = x + win->margin ;
	r.top = y + win->margin ;
	r.right = x + width + win->margin * 2 ;
	r.bottom = y + height + win->margin * 2 ;
	
	if( win->gc == None)
	{
		InvalidateRect( win->my_window, &r, TRUE) ;

		return  1 ;
	}
	else if( win->drawable == win->buffer)
	{
	#ifndef  USE_WIN32API
		XSetForeground( win->display , win->gc , win->bg_color) ;
		XFillRectangle( win->display , win->drawable , win->gc ,
			x + win->margin , y + win->margin , width , height) ;
		XSetForeground( win->display , win->gc , win->fg_color) ;
	#endif

		return  0 ;
	}
	else if( win->drawable == win->my_window)
	{
		SelectObject( win->gc, GetStockObject(NULL_PEN)) ;
		SelectObject( win->gc, (HBRUSH)GetClassLong( win->my_window, GCL_HBRBACKGROUND)) ;
		Rectangle( win->gc, r.left, r.top, r.right, r.bottom) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
x_window_clear_margin_area(
	x_window_t *  win
	)
{
	if( win->gc == None)
	{
		return  0 ;
	}
	else if( win->drawable == win->buffer)
	{
	#ifndef  USE_WIN32API
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
	#endif

		return  0 ;
	}
	else if( win->drawable == win->my_window)
	{
		SelectObject( win->gc, GetStockObject(NULL_PEN)) ;
		SelectObject( win->gc, (HBRUSH)GetClassLong( win->my_window, GCL_HBRBACKGROUND)) ;
		Rectangle( win->gc, 0, 0, win->margin, ACTUAL_HEIGHT(win)) ;
		Rectangle( win->gc, win->margin, 0, win->width + win->margin, win->margin) ;
		Rectangle( win->gc, win->width + win->margin, 0,
			ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win)) ;
		Rectangle( win->gc, win->margin, win->height + win->margin,
			win->width + win->margin, ACTUAL_HEIGHT(win)) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
x_window_clear_all(
	x_window_t *  win
	)
{
	return  x_window_clear( win, 0, 0, win->width, win->height) ;
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
#ifndef  USE_WIN32API
	XFillRectangle( win->display , win->drawable , win->gc , x + win->margin , y + win->margin ,
		width , height) ;
#endif

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
#ifndef  USE_WIN32API
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
#endif

	return  1 ;
}

int
x_window_fill_all(
	x_window_t *  win
	)
{
#ifndef  USE_WIN32API
	XFillRectangle( win->display , win->drawable , win->gc , 0 , 0 ,
		ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
#endif

	return  1 ;
}

int
x_window_fill_all_with(
	x_window_t *  win ,
	u_long  color
	)
{
#ifndef  USE_WIN32API
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
#endif

	return  1 ;
}

int
x_window_update(
	x_window_t *  win ,
	int  flag
	)
{
	if( win->update_window_flag)
	{
		win->update_window_flag |= flag ;
		
		return  1 ;
	}
	else
	{
		win->update_window_flag = flag ;
	}
	
	PostMessage( win->my_window, WM_USER_PAINT, 0, 0) ;

	return  1 ;
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

	if( event->window != win->my_window)
	{
		return  0 ;
	}

	x_xic_filter_event( win, event) ;

  	switch(event->msg)
        {
        case WM_DESTROY:
		if( win->window_deleted)
		{
			(*win->window_deleted)( win) ;
		}

          	return  1 ;

	case WM_USER_PAINT:
		if( win->update_window)
		{
			win->gc = GetDC( win->my_window) ;
			SetTextAlign( win->gc, TA_LEFT|TA_BASELINE) ;

			(*win->update_window)( win , win->update_window_flag) ;

			ReleaseDC( win->my_window, win->gc) ;
			win->gc = None ;
			win->update_window_flag = 0 ;
			
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG "WM_USER_PAINT_END\n") ;
		#endif
		}

		return  1 ;
		
        case WM_PAINT:
		if( win->window_exposed)
		{
			PAINTSTRUCT  ps ;
			int  x ;
			int  y ;
			u_int  width ;
			u_int  height ;

			win->gc = BeginPaint( win->my_window, &ps) ;
			SetTextAlign( win->gc, TA_LEFT|TA_BASELINE) ;

			if( ps.rcPaint.left < win->margin)
			{
				x = 0 ;
			}
			else
			{
				x = ps.rcPaint.left - win->margin ;
			}

			if( ps.rcPaint.top < win->margin)
			{
				y = 0 ;
			}
			else
			{
				y = ps.rcPaint.top - win->margin ;
			}

			if( ps.rcPaint.right > win->width - win->margin)
			{
				width = win->width - win->margin - x ;
			}
			else
			{
				width = ps.rcPaint.right - x ;
			}

			if( ps.rcPaint.bottom > win->height - win->margin)
			{
				height = win->height - win->margin - y ;
			}
			else
			{
				height = ps.rcPaint.bottom - y ;
			}

			(*win->window_exposed)( win, x, y, width, height) ;

			EndPaint( win->my_window, &ps) ;
			win->gc = None ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG "WM_PAINT_END\n") ;
		#endif
		}
		
        	return  1 ;

#if  0
	case WM_ERASEBKGND:
		/* Stop erase background. */
		return  1 ;
#endif
	
	case WM_KEYDOWN:
		if( win->key_pressed)
		{
			if( ( 0x30 <= event->wparam && event->wparam <= VK_DIVIDE) ||
				event->wparam == VK_BACK || event->wparam == VK_TAB ||
				event->wparam == VK_RETURN || event->wparam == VK_ESCAPE ||
				event->wparam == VK_SPACE)
			{
				/* wait for WM_**_CHAR message. */
				break ;
			}
			else
			{
				XKeyEvent  kev ;

				kev.state = get_key_state() ;
				kev.ch = 0 ;

				(*win->key_pressed)( win, &kev) ;
			}
		}

		break ;
		
	case WM_IME_CHAR:
        case WM_CHAR:
		if( win->key_pressed)
		{
			XKeyEvent  kev ;

			kev.state = get_key_state() ;
			kev.ch = event->wparam ;

			(*win->key_pressed)( win, &kev) ;
		}
		
		return  1 ;

	case WM_SETFOCUS:
	#ifdef  __DEBUG
		kik_debug_printf( "FOCUS IN %p\n" , win->my_window) ;
	#endif

	#if  1
		/* root window event only */
		if( win->parent == NULL)
	#endif
		{
			notify_focus_in_to_children( win) ;
		}

		break ;

	case WM_KILLFOCUS:
	#ifdef  __DEBUG
		kik_debug_printf( "FOCUS OUT %p\n" , win->my_window) ;
	#endif

	#if  1
		/* root window event only */
		if( win->parent == NULL)
	#endif
		{
			notify_focus_out_to_children( win) ;
		}

		break ;
        }

	return  0 ;

#ifndef  USE_WIN32API
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
				kik_warn_printf( KIK_DEBUG_TAG " MappingNotify serial #%d\n",
					event->xmapping.serial) ;
#endif
				XRefreshKeyboardMapping( &(event->xmapping));
				x_window_manager_update_modifier_mapping(
					win->win_man, event->xmapping.serial) ;
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

		if( ! restore_view( win , x , y , width , height))
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

			if( win->buffer)
			{
				XFreePixmap( win->display , win->buffer) ;
				win->buffer = XCreatePixmap( win->display ,
					win->parent_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
					DefaultDepth( win->display , win->screen)) ;
				win->drawable = win->buffer ;

			#ifdef  USE_TYPE_XFT
				XftDrawChange( win->xft_draw , win->drawable) ;
			#endif
			}

			x_window_clear_all( win) ;

			if( win->window_resized)
			{
				(*win->window_resized)( win) ;
			}

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
	else if( event->type == MapNotify)
	{
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
	#ifdef  DEBUG
		Atom  xa_multiple ;
	#endif
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
			Atom  targets[5] ;

			targets[0] =xa_targets ;
			targets[1] =XA_STRING ;
			targets[2] =xa_text ;
			targets[3] =xa_compound_text ;
			targets[4] =xa_utf8_string ;
			x_window_send_selection( win , &event->xselectionrequest , (u_char *)(&targets) , sizeof(targets) / sizeof targets[0] , XA_ATOM , 32) ;
		}
#ifdef  DEBUG
		else if( event->xselectionrequest.target == xa_multiple)
		{
			kik_debug_printf("MULTIPLE requested(not yet implemented)\n") ;
		}
#endif
		else
		{
			x_window_send_selection( win , &event->xselectionrequest , NULL , 0 , 0 , 0) ;
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

		if( event->xselection.property == None ||
			event->xselection.property == XA_NONE(win->display))
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
#if 0
		if( event->xclient.format == 32 &&
			event->xclient.data.l[0] == XA_TAKE_FOCUS( win->display))
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" TakeFocus message is received.\n") ;
		}
#endif
	}
	else if( event->type == PropertyNotify)
	{

		if( event->xproperty.atom == XA_SELECTION( win->display) &&
		    event->xproperty.state == PropertyNewValue)
		{
			XTextProperty  ct ;
			u_long  bytes_after ;

			XGetWindowProperty( win->display, event->xproperty.window,
					    event->xproperty.atom, 0, 0, False,
					    AnyPropertyType, &ct.encoding, &ct.format,
					    &ct.nitems, &bytes_after, &ct.value) ;
			if( ct.value)
			{
				XFree( ct.value) ;
			}

			if( ct.encoding == XA_INCR(win->display)
			    || bytes_after == 0)
			{
				XDeleteProperty( win->display, event->xproperty.window,
						 ct.encoding) ;
			}
			else
			{
				XGetWindowProperty( win->display , event->xproperty.window ,
						    event->xproperty.atom , 0 , bytes_after , True ,
						    AnyPropertyType , &ct.encoding , &ct.format ,
						    &ct.nitems , &bytes_after , &ct.value) ;
				if(ct.encoding == XA_STRING ||
				   ct.encoding == XA_TEXT(win->display) ||
				   ct.encoding == XA_COMPOUND_TEXT(win->display))
				{
					if( win->xct_selection_notified)
					{
						(*win->xct_selection_notified)(
							win , ct.value , ct.nitems) ;
					}
				}
				else if(ct.encoding == XA_UTF8_STRING(win->display))
				{
					if( win->utf8_selection_notified)
					{
						(*win->utf8_selection_notified)(
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

#endif /* USE_WIN32API */
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
	return  x_xic_get_str( win, seq, seq_len, parser, keysym, event) ;
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

	BitBlt( win->gc, win->margin, win->margin + boundary_start,		/* dst */
		win->width, boundary_end - boundary_start - height,		/* size */
		win->gc, win->margin, win->margin + boundary_start + height,	/* src */
		SRCCOPY) ;

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

	BitBlt( win->gc, win->margin, win->margin + boundary_start + height,	/* dst */
		win->width, boundary_end - boundary_start - height,		/* size */
		win->gc, win->margin , win->margin + boundary_start,		/* src */
		SRCCOPY) ;

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

#ifndef  USE_WIN32API
	XCopyArea( win->display , win->drawable , win->drawable , win->gc ,
		win->margin + boundary_start + width , win->margin ,	/* src */
		boundary_end - boundary_start - width , win->height ,	/* size */
		win->margin + boundary_start , win->margin) ;		/* dst */
#endif

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

#ifndef  USE_WIN32API
	XCopyArea( win->display , win->drawable , win->drawable , win->gc ,
		win->margin + boundary_start , win->margin ,
		boundary_end - boundary_start - width , win->height ,
		win->margin + boundary_start + width , win->margin) ;
#endif

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
#ifndef  USE_WIN32API
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
	else
#endif
	{
		return  0 ;
	}
}

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
	if( win->gc == None)
	{
		return  0 ;
	}

	SelectObject( win->gc, font->xfont) ;
	SetTextColor( win->gc, fg_color->pixel) ;
	SetBkMode( win->gc, TRANSPARENT) ;

	draw_string( win, font, x, y, str, len) ;
	
	SetBkMode( win->gc, OPAQUE) ;

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
	return  x_window_draw_string( win, font, fg_color, x, y, (u_char*)str, len * 2) ;
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
	if( win->gc == None)
	{
		return  0 ;
	}

	SelectObject( win->gc, font->xfont) ;
	SetTextColor( win->gc, fg_color->pixel) ;
	SetBkColor( win->gc, bg_color->pixel) ;

	draw_string( win, font, x, y, str, len) ;

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
	return  x_window_draw_image_string( win, font, fg_color, bg_color,
						x, y, (u_char*)str, len * 2) ;
}

int
x_window_draw_rect_frame(
	x_window_t *  win ,
	int  x1 ,
	int  y1 ,
	int  x2 ,
	int  y2
	)
{
	if( win->gc == None)
	{
		return  0 ;
	}

	/* XXX */
	SelectObject( win->gc, GetStockObject(BLACK_PEN)) ;

	SelectObject( win->gc, GetStockObject(NULL_BRUSH)) ;
	
	Rectangle( win->gc, x1, y1, x2, y2) ;

	return  1 ;
}

int
x_window_draw_line(
	x_window_t *  win,
	int  x1,
	int  y1,
	int  x2,
	int  y2
	)
{
	if( win->gc == None)
	{
		return  0 ;
	}

	/* XXX */
	SelectObject( win->gc, GetStockObject(BLACK_PEN)) ;

	MoveToEx( win->gc, x1, y1, NULL) ;
	LineTo( win->gc, x2, y2) ;

	return  1 ;
}

int
x_window_set_selection_owner(
	x_window_t *  win ,
	Time  time
	)
{
#ifndef  USE_WIN32API
	XSetSelectionOwner( win->display , XA_PRIMARY , win->my_window , time) ;

	if( win->my_window != XGetSelectionOwner( win->display , XA_PRIMARY))
	{
		return  0 ;
	}
	else
	{
		return  x_window_manager_own_selection( x_get_root_window( win)->win_man , win) ;
	}
#endif

	return  0 ;
}

int
x_window_xct_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
#ifndef  USE_WIN32API
	XConvertSelection( win->display , XA_PRIMARY , XA_COMPOUND_TEXT(win->display) ,
		XA_SELECTION(win->display) , win->my_window , time) ;
#endif

	return  1 ;
}

int
x_window_utf8_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
#ifndef  USE_WIN32API
	XConvertSelection( win->display , XA_PRIMARY , XA_UTF8_STRING(win->display) ,
		XA_SELECTION(win->display) , win->my_window , time) ;
#endif

	return  1 ;
}

int
x_window_send_selection(
	x_window_t *  win ,
	XSelectionRequestEvent *  req_ev ,
	u_char *  sel_data ,
	size_t  sel_len ,
	Atom  sel_type ,
	int sel_format
	)
{
#ifndef  USE_WIN32API
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
		if( req_ev->property != None){
			XChangeProperty( win->display , req_ev->requestor ,
				req_ev->property , sel_type ,
				sel_format , PropModeReplace , sel_data , sel_len) ;
		}
		res_ev.xselection.property = req_ev->property ;
	}

	XSendEvent( win->display , res_ev.xselection.requestor , False , 0 , &res_ev) ;
#endif

	return  1 ;
}

int
x_set_window_name(
	x_window_t *  win ,
	u_char *  name
	)
{
#ifndef  USE_WIN32API
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
#endif

	return  1 ;
}

int
x_set_icon_name(
	x_window_t *  win ,
	u_char *  name
	)
{
#ifndef  USE_WIN32API
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
#endif

	return  1 ;
}

int
x_window_remove_icon(
	x_window_t *  win
	)
{
#ifndef  USE_WIN32API
	XWMHints *hints ;

	hints = XGetWMHints( win->display, win->my_window) ;

	if( hints)
	{

		hints->flags &= ~IconPixmapHint ;
		hints->flags &= ~IconMaskHint ;

		XSetWMHints( win->display, win->my_window, hints) ;
		XFree( hints) ;
	}
	/* do not use hints->icon_*. they may be shared! */
	if( win->icon_pix != None)
	{
		XFreePixmap( win->display, win->icon_pix);
		win->icon_pix = None ;
	}
	if( win->icon_mask != None)
	{
		XFreePixmap( win->display, win->icon_mask);
		win->icon_mask = None ;
	}
	free( win->icon_card);
	win->icon_card = NULL ;

	XDeleteProperty( win->display, win->my_window,XA_NET_WM_ICON( win->display)) ;
#endif

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
#ifndef  USE_WIN32API
	XWMHints *  hints = NULL ;

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
#endif

	return 1 ;
}

int
x_window_set_icon_from_file(
	x_window_t *  win,
	char *  path
	)
{
#ifndef  USE_WIN32API
	int icon_size = 48;

	x_window_remove_icon( win);

	if( !x_imagelib_load_file( win->display ,
				   path,
				   &(win->icon_card),
				   &(win->icon_pix),
				   &(win->icon_mask),
				   &icon_size ,&icon_size))
	{
		return  0 ;
	}
#endif

	return x_window_set_icon( win,
				  win->icon_pix,
				  win->icon_mask,
				  win->icon_card) ;
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
#ifndef  USE_WIN32API
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
#endif

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

int
x_window_bell(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_window_translate_coordinates(
	x_window_t *  win,
	int x,
	int y,
	int *  global_x,
	int *  global_y,
	Window *  child
	)
{
	*global_x = 0 ;
	*global_y = 0 ;
	*child = None ;
	
	return  0 ;
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
