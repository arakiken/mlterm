/*
 *	$Id$
 */

#include  "../x_window.h"

#include  <stdlib.h>		/* abs */
#include  <string.h>		/* memset/memcpy */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* realloc/free */
#include  <kiklib/kik_util.h>	/* K_MIN/K_MAX */
#include  <kiklib/kik_dialog.h>
#include  <mkf/mkf_codepoint_parser.h>
#include  <ml_char.h>		/* UTF_MAX_SIZE */

#if  ! defined(USE_WIN32API) && defined(HAVE_PTHREAD)
#include  <pthread.h>
#endif

#include  "../x_xic.h"
#include  "../x_picture.h"
#include  "../x_imagelib.h"
#ifndef  DISABLE_XDND
#include  "../x_dnd.h"
#endif

#include  "x_display.h"		/* x_display_get_cursor */
#include  "x_gdiobj_pool.h"


#define  MAX_CLICK  3			/* max is triple click */

#define  WM_APP_PAINT  (WM_APP + 0x0)
#define  WM_APP_PASTE  (WM_APP + 0x1)
#define  WM_APP_WPASTE (WM_APP + 0x2)

/*
 * ACTUAL_(WIDTH|HEIGHT) is the width of client area, ACTUAL_WINDOW_(WIDTH|HEIGHT) is
 * that of window area.
 */
#if  1
#define  ACTUAL_WINDOW_WIDTH(win) (ACTUAL_WIDTH(win) + decorate_width)
#define  ACTUAL_WINDOW_HEIGHT(win) (ACTUAL_HEIGHT(win) + decorate_height)
#else
#define  ACTUAL_WINDOW_WIDTH(win) \
	ACTUAL_WIDTH(win) + GetSystemMetrics(SM_CXEDGE) * 2 + \
	/* GetSystemMetrics(SM_CXBORDER) * 2 + */ GetSystemMetrics(SM_CXFRAME)
#define  ACTUAL_WINDOW_HEIGHT(win) \
	ACTUAL_HEIGHT(win) + GetSystemMetrics(SM_CYEDGE) * 2 + \
	/* GetSystemMetrics(SM_CXBORDER) * 2 + */ GetSystemMetrics(SM_CYFRAME) + \
	GetSystemMetrics(SM_CYCAPTION)
#endif

/* win->width is not multiples of (win)->width_inc if window is maximized. */
#define  RIGHT_MARGIN(win) \
	((win)->width_inc ? ((win)->width - (win)->min_width) % (win)->width_inc : 0)
#define  BOTTOM_MARGIN(win) \
	((win)->height_inc ? ((win)->height - (win)->min_height) % (win)->height_inc : 0)

#define  ParentRelative  (1L)

#if  0
#define  DEBUG_SCROLLABLE
#endif

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  click_interval = 250 ;	/* millisecond, same as xterm. */
static mkf_parser_t *  m_cp_parser ;
static LONG  decorate_width ;
static LONG  decorate_height ;		/* Height of Title bar etc. */
static int  use_urgent_bell ;


/* --- static functions --- */

#ifdef  FLASHW_ALL
static void
urgent_bell(
	x_window_t *  win ,
	int  on
	)
{
	if( use_urgent_bell && ( ! win->is_focused || ! on))
	{
		win = x_get_root_window( win) ;

		if( on)
		{
			FLASHWINFO  info ;

			info.cbSize = sizeof(info) ;
			info.hwnd = win->my_window ;
			info.dwFlags = FLASHW_ALL ;
			info.uCount = 5 ;
			info.dwTimeout = 0 ;	/* standard */

			FlashWindowEx( &info) ;
		}
		else
		{
			FlashWindow( win->my_window , FALSE) ;
		}
	}
}
#else
#define  urgent_bell(win,on)  (0)
#endif


static int
set_transparent(
	x_window_t *  win ,
	int  alpha
	)
{
/*
 * XXX
 * LWA_ALPHA and SetLayeredWindowAttributes() are not defined
 * in older winuser.h and libuser32.a(e.g. MSYS-DTK 1.0.1).
 */
#if  defined(WS_EX_LAYERED) && defined(LWA_ALPHA)
	if( ( win = x_get_root_window( win))->my_window)
	{
		LONG  style ;

		style = GetWindowLong( win->my_window , GWL_EXSTYLE) ;
		SetWindowLong( win->my_window , GWL_EXSTYLE , style | WS_EX_LAYERED) ;

	#if  1
		SetLayeredWindowAttributes( win->my_window , 0 , alpha , LWA_ALPHA) ;
	#else
		SetLayeredWindowAttributes( win->my_window ,
			ARGB_TO_RGB(win->bg_color.pixel) , 0 , LWA_COLORKEY) ;
	#endif
	}

	return  1 ;
#else
	return  0 ;
#endif
}

static int
unset_transparent(
	x_window_t *  win
	)
{
/*
 * XXX
 * LWA_ALPHA and SetLayeredWindowAttributes() are not defined
 * in older winuser.h and libuser32.a(e.g. MSYS-DTK 1.0.1).
 */
#if  defined(WS_EX_LAYERED) && defined(LWA_ALPHA)
	if( ( win = x_get_root_window( win))->my_window)
	{
		LONG  style ;

		style = GetWindowLong( win->my_window , GWL_EXSTYLE) ;
		SetWindowLong( win->my_window , GWL_EXSTYLE , style & ~WS_EX_LAYERED) ;
	}

	return  1 ;
#else
	return  0 ;
#endif
}

/*
 * XXX
 * Adhoc alternative of VisibilityNotify event.
 */
static int
check_scrollable(
	x_window_t *  win	/* Assume root window. */
	)
{
	if( win->is_focused)
	{
	#ifdef  DEBUG_SCROLLABLE
		kik_debug_printf( "SCREEN W %d H %d WINDOW W %d H %d X %d Y %d " ,
			win->disp->width , win->disp->height ,
			ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) , win->x , win->y) ;
	#endif

		/*
		 * If window is outside of screen partially, is_scrollable = 0.
		 */

		if( win->y < 0 || win->x < 0)
		{
		#ifdef  DEBUG_SCROLLABLE
			kik_debug_printf( "NOT SCROLLABLE(1)\n") ;
		#endif
			return  0 ;
		}
		else
		{
			if( win->y + ACTUAL_HEIGHT(win) > win->disp->height)
			{
			#ifdef  DEBUG_SCROLLABLE
				kik_debug_printf( "NOT SCROLLABLE(2)\n") ;
			#endif
				return  0 ;
			}
			else
			{
				if( win->x + ACTUAL_WIDTH(win) > win->disp->width)
				{
				#ifdef  DEBUG_SCROLLABLE
					kik_debug_printf( "NOT SCROLLABLE(3)\n") ;
				#endif
					return  0 ;
				}
				else
				{
					APPBARDATA  barinfo ;

					ZeroMemory( &barinfo , sizeof( barinfo)) ;
					barinfo.cbSize = sizeof( barinfo) ;
					barinfo.hWnd = win->my_window ;

					SHAppBarMessage( ABM_GETTASKBARPOS , &barinfo) ;

				#ifdef  DEBUG_SCROLLABLE
					kik_debug_printf( "TASKBAR t %d b %d l %d r %d " ,
						barinfo.rc.top , barinfo.rc.bottom ,
						barinfo.rc.left , barinfo.rc.right) ;
				#endif

					if( barinfo.rc.top <= 0)
					{
						if( barinfo.rc.left <= 0)
						{
							if( barinfo.rc.right >= win->disp->width)
							{
								/* North */
								if( win->y < barinfo.rc.bottom)
								{
								#ifdef  DEBUG_SCROLLABLE
									kik_debug_printf(
									"NOT SCROLLABLE(4)\n") ;
								#endif
									return  0 ;
								}
							}
							else
							{
								/* West */
								if( win->x < barinfo.rc.right)
								{
								#ifdef  DEBUG_SCROLLABLE
									kik_debug_printf(
									"NOT SCROLLABLE(5)\n") ;
								#endif
									return  0 ;
								}
							}
						}
						else
						{
							/* East */
							if( win->x + ACTUAL_WIDTH(win)
								> barinfo.rc.left)
							{
							#ifdef  DEBUG_SCROLLABLE
								kik_debug_printf(
								"NOT SCROLLABLE(6)\n") ;
							#endif
								return  0 ;
							}
						}
					}
					else
					{
						/* South */
						if( win->y + ACTUAL_HEIGHT(win) > barinfo.rc.top)
						{
						#ifdef  DEBUG_SCROLLABLE
							kik_debug_printf( "NOT SCROLLABLE(7)\n") ;
						#endif
							return  0 ;
						}
					}
				
				#ifdef  DEBUG_SCROLLABLE
					kik_debug_printf( "SCROLLABLE\n") ;
				#endif
					return  1 ;
				}
			}
		}
	}
	else
	{
	#ifdef  DEBUG_SCROLLABLE
		kik_debug_printf( "NOT SCROLLABLE(4)\n") ;
	#endif
		return  0 ;
	}
}

static void
notify_focus_in_to_children(
	x_window_t *  win
	)
{
	u_int  count ;

	if( ! win->is_focused)
	{
		if( win->inputtable > 0 ||
		    win->parent == NULL)	/* check_scrollable checks root->is_focused */
		{
			win->is_focused = 1 ;
		}

		/* If win->wall_picture is set, is_scrollable is always 0. */
		if( ! win->wall_picture)
		{
			if( ! win->parent)
			{
				win->is_scrollable = check_scrollable( win) ;
			}
			else
			{
				win->is_scrollable = win->parent->is_scrollable ;
			}
		}

		if( win->inputtable > 0)
		{
			x_xic_set_focus( win) ;

			if( win->window_focused)
			{
				(*win->window_focused)( win) ;
			}
		}
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

		win->is_scrollable = 0 ;

		x_xic_unset_focus( win) ;

		if( win->window_unfocused)
		{
			(*win->window_unfocused)( win) ;
		}
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		notify_focus_out_to_children( win->children[count]) ;
	}
}

static void
notify_move_to_children(
	x_window_t *  win
	)
{
	int  count ;

	
	if( win->wall_picture)
	{
		/* If win->wall_picture is set, is_scrollable is always 0. */
	}
	else if( ! win->parent)
	{
		int  is_scrollable ;

		if( ( is_scrollable = check_scrollable( win)) == win->is_scrollable)
		{
			/*
			 * If is_scrollable is not changed, nothing should be
			 * notified to children.
			 */
			
			return ;
		}

		win->is_scrollable = is_scrollable ;
	}
	else
	{
		win->is_scrollable = win->parent->is_scrollable ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		notify_move_to_children( win->children[count]) ;
	}
}

#if  0
/*
 * Not used for now in win32.
 */
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

/*
 * Not used for now in win32.
 */
static int
is_in_the_same_window_family(
	x_window_t *  win ,
	Window   window
	)
{
	return  is_descendant_window( x_get_root_window( win) , window) ;
}
#endif

static u_int
total_min_width(
	x_window_t *  win
	)
{
	int  count ;
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
	int  count ;
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
		state |= Mod1Mask ;
	}

	return  state ;
}

static BOOL
text_out(
	GC  gc ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len ,
	mkf_charset_t  cs	/* FONT_CS(font->id) */
	)
{
	if( cs == ISO10646_UCS4_1)
	{
		/* TextOutW is supported in windows 9x. */
		return  TextOutW( gc , x , y , (WCHAR*)str , len / 2) ;
	}
	else if( cs == ISCII_BENGALI || cs == ISCII_ASSAMESE)
	{
		u_int  count ;
		u_int16_t *  str2 ;

		str2 = alloca( len * 2) ;

		/* char code -> glyph index in BN-TTDurga and AS-TTDurga fonts. */
		for( count = 0 ; count < len ; count++)
		{
			if( str[count] <= 0x7e)
			{
				str2[count] = str[count] - 0x1d ;
			}
			else
			{
				str2[count] = str[count] - 0x1e ;
			}
		}

		/* ExtTextOutA doesn't work correctly. */
		return  ExtTextOutW( gc , x , y , ETO_GLYPH_INDEX , NULL , str2 , len , NULL) ;
	}
	else
	{
		return  TextOutA( gc , x , y , str , len) ;
	}
}

static int
draw_string(
	x_window_t *  win ,
	x_font_t *  font ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len ,
	int  is_tp
	)
{
	u_char *  str2 ;
	
	str2 = NULL ;

	if( font->conv)
	{
		if( ( str2 = alloca( len * UTF_MAX_SIZE)))	/* assume utf8 */
		{
			(*m_cp_parser->init)( m_cp_parser) ;
			/* 3rd argument of cp_parser->set_str is len(16bit) + cs(16bit) */
			(*m_cp_parser->set_str)( m_cp_parser, str,
							len | (FONT_CS(font->id) << 16) ) ;
			
			(*font->conv->init)( font->conv) ;
			if( ( len = (*font->conv->convert)( font->conv, str2, len * UTF_MAX_SIZE,
					m_cp_parser)) > 0)
			{
				str = str2 ;
			}
		}
	}

	if( is_tp)
	{
		SetBkMode( win->gc->gc , TRANSPARENT) ;
	}

	text_out( win->gc->gc, x + (font->is_var_col_width ? 0 : font->x_off) + win->hmargin ,
		y + win->vmargin , str , len , FONT_CS(font->id)) ;
	
	if( font->double_draw_gap)
	{
		SetBkMode( win->gc->gc , TRANSPARENT) ;

		text_out( win->gc->gc ,
			x + (font->is_var_col_width ? 0 : font->x_off) + win->hmargin +
				font->double_draw_gap ,
			y + win->vmargin , str , len , FONT_CS(font->id)) ;

		SetBkMode( win->gc->gc , OPAQUE) ;
	}
	else if( is_tp)
	{
		SetBkMode( win->gc->gc , OPAQUE) ;
	}

	return  1 ;
}

/*
 * Return 1 => decorate size is changed.
 *        0 => decorate size is not changed.
 */
static int
update_decorate_size(
	x_window_t *  win
	)
{
	RECT  wr ;
	RECT  cr ;
	LONG  width ;
	LONG  height ;

	if( win->parent)
	{
		return  0 ;
	}
	
	GetWindowRect( win->my_window , &wr) ;
	GetClientRect( win->my_window , &cr) ;
	width = wr.right - wr.left - cr.right + cr.left ;
	height = wr.bottom - wr.top - cr.bottom + cr.top ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " DECORATE W %d H %d -> W %d H %d\n" ,
		decorate_width , decorate_height , width , height) ;
#endif

	if( width != decorate_width || height != decorate_height)
	{
		decorate_width = width ;
		decorate_height = height ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static int
clear_margin_area(
	x_window_t *  win
	)
{
	u_int  right_margin ;
	u_int  bottom_margin ;
	u_int  win_width ;
	u_int  win_height ;

	if( win->gc->gc == None)
	{
		return  0 ;
	}

	right_margin = RIGHT_MARGIN(win) ;
	bottom_margin = BOTTOM_MARGIN(win) ;
	win_width = win->width - right_margin ;
	win_height = win->height - bottom_margin ;

	if( win->wall_picture)
	{
		Pixmap  pic ;
		int  src_x ;
		int  src_y ;

		if( win->wall_picture == ParentRelative)
		{
			pic = win->parent->wall_picture ;
			src_x = win->x ;
			src_y = win->y ;
		}
		else
		{
			pic = win->wall_picture ;
			src_x = src_y = 0 ;
		}

		if( win->hmargin > 0 || right_margin > 0)
		{
			BitBlt( win->gc->gc , 0 , 0 ,
				win->hmargin , ACTUAL_HEIGHT(win) ,
				pic , src_x , src_y , SRCCOPY) ;
			BitBlt( win->gc->gc , win_width + win->hmargin , 0 ,
				win->hmargin + right_margin , ACTUAL_HEIGHT(win) ,
				pic ,
				src_x + win_width + win->hmargin , src_y , SRCCOPY) ;
		}

		if( win->vmargin > 0 || bottom_margin > 0)
		{
			BitBlt( win->gc->gc , win->hmargin , 0 ,
				win_width , win->vmargin ,
				pic ,
				src_x + win->hmargin , src_y , SRCCOPY) ;
			BitBlt( win->gc->gc , win->hmargin , win_height + win->vmargin ,
				win_width , win->vmargin + bottom_margin ,
				pic ,
				src_x + win->hmargin , src_y + win_height + win->vmargin ,
				SRCCOPY) ;
		}
	}
	else
	{
		HBRUSH  brush ;
		RECT  r ;

		brush = x_acquire_brush( win->bg_color.pixel) ;

		if( win->hmargin > 0 || right_margin > 0)
		{
			SetRect( &r , 0 , 0 , win->hmargin , ACTUAL_HEIGHT(win)) ;
			FillRect( win->gc->gc , &r , brush) ;
			SetRect( &r , win_width + win->hmargin , 0 ,
				ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
			FillRect( win->gc->gc , &r , brush) ;
		}

		if( win->vmargin > 0 || bottom_margin > 0)
		{
			SetRect( &r , win->hmargin , 0 ,
				win_width + win->hmargin , win->vmargin) ;
			FillRect( win->gc->gc , &r , brush) ;
			SetRect( &r , win->hmargin , win_height + win->vmargin ,
				win_width + win->hmargin , ACTUAL_HEIGHT(win)) ;
			FillRect( win->gc->gc , &r , brush) ;
		}

		x_release_brush( brush) ;
	}

	return  1 ;
}

static WORD
oem_key_to_char(
	WORD  wparam
	)
{
	if( VK_OEM_1 <= wparam && wparam <= VK_OEM_2)
	{
		/*
		 * VK_OEM_1 0xba
		 * VK_OEM_PLUS 0xbb
		 * VK_OEM_COMMA 0xbc
		 * VK_OEM_MINUS 0xbd
		 * VK_OEM_PERIOD 0xbe
		 * VK_OEM_2 0xbf
		 */
		return  '*' + wparam - VK_OEM_1 ;
	}
	else if( wparam == VK_OEM_3)
	{
		return  '@' ;
	}
	else if( VK_OEM_4 <= wparam && wparam <= VK_OEM_7)
	{
		/*
		 * VK_OEM_4 0xdb
		 * VK_OEM_5 0xdc
		 * VK_OEM_6 0xdd
		 * VK_OEM_7 0xde
		 */
		return  '[' + wparam - VK_OEM_4 ;
	}
	else if( wparam == VK_OEM_102)
	{
		return  '_' ;
	}
	else
	{
		return  0 ;
	}
}

static int
selection_request(
	x_window_t *  win ,
	UINT  format
	)
{
	HGLOBAL  hmem ;
	u_char *  l_data ;
	u_char *  g_data ;
	size_t  len ;

#if  ! defined(USE_WIN32API) && defined(HAVE_PTHREAD)
	pthread_detach( pthread_self()) ;
#endif

	if( OpenClipboard( win->my_window) == FALSE)
	{
		return  0 ;
	}

	g_data = NULL ;

	if( ( hmem = GetClipboardData( format)) == NULL ||
	    ( g_data = GlobalLock( hmem)) == NULL ||
	    ( len = (format == CF_TEXT ? strlen( g_data) : lstrlenW( g_data))) == 0 ||
	    ( l_data = malloc( (len + 1) * (format == CF_TEXT ? 1 : 2))) == NULL)
	{
		if( g_data)
		{
			GlobalUnlock( hmem) ;
		}

		CloseClipboard() ;

		return  0 ;
	}

	if( format == CF_TEXT)
	{
		strcpy( l_data , g_data) ;
	}
	else
	{
		lstrcpyW( l_data , g_data) ;
	}

	GlobalUnlock( hmem) ;
	CloseClipboard() ;

#if  0
	kik_debug_printf( "%s SELECTION: %d\n", format == CF_TEXT ? "XCT" : "UTF" , len) ;
#endif

	PostMessage( win->my_window ,
		format == CF_TEXT ? WM_APP_PASTE : WM_APP_WPASTE , len , l_data) ;

	return  0 ;
}

#ifdef  USE_WIN32API
static u_int __stdcall
#else
static void *
#endif
xct_selection_request(
	LPVOID  thr_param
	)
{
	return  selection_request( thr_param , CF_TEXT) ;
}

#ifdef  USE_WIN32API
static u_int __stdcall
#else
static void *
#endif
utf_selection_request(
	LPVOID  thr_param
	)
{
	return  selection_request( thr_param , CF_UNICODETEXT) ;
}

/*
 * Request selection from another thread for x11 applications on x11 forwarding.
 */
static int
invoke_selection_request(
	x_window_t *  win ,
	UINT  format ,
#ifdef  USE_WIN32API
	u_int (*selection_request)(LPVOID)
#else
	void *  (*selection_request)(LPVOID)
#endif
	)
{
	if( IsClipboardFormatAvailable( format))
	{
	#if  defined(USE_WIN32API)
		HANDLE  thrd ;
		u_int  tid ;

		if( ! ( thrd = _beginthreadex( NULL , 0 , selection_request , win , 0 , &tid)))
		{
			return  0 ;
		}

		CloseHandle( thrd) ;
	#elif  defined(HAVE_PTHREAD)
		pthread_t  thrd ;

		pthread_create( &thrd , NULL , selection_request , win) ;
	#else
		selection_request( win) ;
	#endif

		return  1 ;
	}

	return  0 ;
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


/* --- global functions --- */

int
x_window_init(
	x_window_t *  win ,
	u_int  width ,
	u_int  height ,
	u_int  min_width ,
	u_int  min_height ,
	u_int  width_inc ,
	u_int  height_inc ,
	u_int  hmargin ,
	u_int  vmargin ,
	int  create_gc ,	/* ignored */
	int  inputtable
	)
{
	memset( win , 0 , sizeof( x_window_t)) ;

	win->fg_color.pixel = 0xff000000 ;
	win->bg_color.pixel = 0xffffffff ;

	/* if visibility is partially obscured , scrollable will be 0. */
	win->is_scrollable = 1 ;

	win->inputtable = inputtable ;

	/* This flag will map window automatically in x_window_show(). */
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

	/* Not freed explicitly. Expect to be freed on process exited. */
	if( ! m_cp_parser && ( m_cp_parser = mkf_codepoint_parser_new()) == NULL)
	{
		return  0 ;
	}

	win->prev_clicked_button = -1 ;

	win->app_name = __("mlterm") ;

	win->cmd_show = SW_SHOWNORMAL ;

	return	1 ;
}

int
x_window_final(
	x_window_t *  win
	)
{
	u_int  count ;

#ifdef  __DEBUG
	kik_debug_printf( "[deleting child windows]\n") ;
	x_window_dump_children( win) ;
#endif

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_final( win->children[count]) ;
	}

	free( win->children) ;

	x_display_clear_selection( win->disp , win) ;

	/*
	 * DestroyWindow() is not called here because DestroyWindow internally sends
	 * WM_DESTROY message which causes window_deleted event again.
	 * If you want to close window, call SendMessage( WM_CLOSE ) instead of x_window_final().
	 */

	x_xic_deactivate( win) ;

#if  0
	(*m_cp_parser.delete)( &m_cp_parser) ;
#endif

	if( win->window_finalized)
	{
		(*win->window_finalized)( win) ;
	}

	return  1 ;
}

int
x_window_set_type_engine(
	x_window_t *  win ,
	x_type_engine_t  type_engine
	)
{
	return  0 ;
}

int
x_window_init_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
	return  1 ;
}

int
x_window_add_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
	return  1 ;
}

int
x_window_remove_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
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
	Pixmap  pic ,
	int  do_expose
	)
{
	u_int  count ;

	win->is_scrollable = 0 ;
	win->wall_picture = pic ;

	if( win->my_window != None && do_expose)
	{
		InvalidateRect( win->my_window, NULL, FALSE) ;
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

	win->is_scrollable = check_scrollable( win) ;
	win->wall_picture = None ;

	if( win->my_window != None)
	{
		InvalidateRect( win->my_window, NULL, FALSE) ;
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		x_window_set_wall_picture( win->children[count] , ParentRelative , do_expose) ;
	}

	return  1 ;
}

int
x_window_set_transparent(
	x_window_t *  win ,
	x_picture_modifier_t *  pic_mod
	)
{
	return  0 ;
}

int
x_window_unset_transparent(
	x_window_t *  win
	)
{
	return  0 ;
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
	int  alpha ;

	if( win->bg_color.pixel == bg_color->pixel)
	{
		return  0 ;
	}

	if( ( alpha = ((bg_color->pixel >> 24) & 0xff)) != 0xff)
	{
		set_transparent( win , alpha) ;
	}
	else if( ((win->bg_color.pixel >> 24) & 0xff) != 0xff)
	{
		/*
		 * If alpha is changed from less than 0xff to 0xff,
		 * transparent is disabled.
		 * XXX It is assumed that alpha is changed by only one window.
		 */
		unset_transparent( win) ;
	}

	win->bg_color = *bg_color ;

	if( win->my_window != None)
	{
		InvalidateRect( win->my_window, NULL, FALSE) ;
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
	if( win->gc->gc == None)
	{
		return  None ;
	}
	
#if  0
	x_gc_set_fg_color( win->gc , win->fg_color.pixel) ;
	x_gc_set_bg_color( win->gc , win->bg_color.pixel) ;
#endif

	x_release_pen( x_gc_set_pen( win->gc , x_acquire_pen( win->fg_color.pixel))) ;
	x_release_brush( x_gc_set_brush( win->gc , x_acquire_brush( win->fg_color.pixel))) ;

	return  win->gc->gc ;
}

GC
x_window_get_bg_gc(
	x_window_t *  win
	)
{
	if( win->gc->gc == None)
	{
		return  None ;
	}
	
#if  0
	x_gc_set_fg_color( win->gc , win->bg_color.pixel) ;
	x_gc_set_bg_color( win->gc , win->fg_color.pixel) ;
#endif

	x_release_pen( x_gc_set_pen( win->gc , GetStockObject(NULL_PEN))) ;
	x_release_brush( x_gc_set_brush( win->gc , x_acquire_brush( win->bg_color.pixel))) ;

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

	if( win->my_window)
	{
		/* already shown */

		return  0 ;
	}

	if( win->parent)
	{
		win->disp = win->parent->disp ;
		win->parent_window = win->parent->my_window ;
		win->gc = win->parent->gc ;
	}

#ifndef  USE_WIN32GUI
	if( hint & XNegative)
	{
		win->x += (win->disp->width - ACTUAL_WIDTH(win)) ;
	}

	if( hint & YNegative)
	{
		win->y += (win->disp->height - ACTUAL_HEIGHT(win)) ;
	}
#endif

#ifdef  __DEBUG
	kik_debug_printf( "X: EDGE%d BORDER%d FRAME%d Y: EDGE%d BORDER%d FRAME%d CAPTION%d\n",
		GetSystemMetrics(SM_CXEDGE),
		GetSystemMetrics(SM_CXBORDER), GetSystemMetrics(SM_CXFRAME),
		GetSystemMetrics(SM_CYEDGE), GetSystemMetrics(SM_CYBORDER),
		GetSystemMetrics(SM_CYFRAME), GetSystemMetrics(SM_CYCAPTION)) ;
#endif

	win->my_window = CreateWindowEx( 0 , __("MLTERM") , win->app_name ,
				PARENT_WINDOWID_IS_TOP(win) ?
					WS_OVERLAPPEDWINDOW : WS_CHILD | WS_VISIBLE ,
				PARENT_WINDOWID_IS_TOP(win) && ! (hint & XValue) ?
					CW_USEDEFAULT : win->x ,
				PARENT_WINDOWID_IS_TOP(win) && ! (hint & YValue) ?
					CW_USEDEFAULT : win->y ,
				PARENT_WINDOWID_IS_TOP(win) ?
					ACTUAL_WINDOW_WIDTH(win) : ACTUAL_WIDTH(win) ,
				PARENT_WINDOWID_IS_TOP(win) ?
					ACTUAL_WINDOW_HEIGHT(win) : ACTUAL_HEIGHT(win) ,
				win->parent_window , NULL , win->disp->display->hinst , NULL) ;

	if( ! win->my_window)
	{
		kik_dialog( KIK_DIALOG_ALERT , "Failed to create window.") ;

		return  0 ;
	}

#ifndef  DISABLE_XDND
	DragAcceptFiles( win->my_window , TRUE) ;
#endif

	if( win->parent && ! win->parent->is_transparent &&
	    win->parent->wall_picture)
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

	/* Don't place this before x_window_show(children). */
	if( update_decorate_size( win))
	{
		SetWindowPos( win->my_window , win->cmd_show == SW_SHOWNA ? HWND_TOPMOST : 0 ,
			0 , 0 , ACTUAL_WINDOW_WIDTH(win) , ACTUAL_WINDOW_HEIGHT(win) ,
			(win->cmd_show == SW_SHOWNA ? SWP_NOACTIVATE : 0) |
			SWP_NOMOVE | SWP_NOZORDER) ;
	}

	if( win->is_mapped)
	{
		ShowWindow( win->my_window , win->cmd_show) ;

		if( win->inputtable > 0)
		{
			reset_input_focus( x_get_root_window( win)) ;
			win->inputtable = 1 ;
		}

	#if  0
		x_window_clear_all( win) ;
	#endif
	}
	else
	{
		ShowWindow( win->my_window , SW_HIDE) ;
	}

	if( win->is_transparent)
	{
		x_window_set_transparent( win , win->pic_mod) ;
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

  	ShowWindow( win->my_window , win->cmd_show) ;
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

  	ShowWindow( win->my_window , SW_HIDE) ;
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

	/* Max width of each window is screen width. */
	if( (flag & LIMIT_RESIZE) && win->disp->width < width)
	{
		win->width = win->disp->width - win->hmargin * 2 ;
	}
	else
	{
		win->width = width ;
	}

	/* Max height of each window is screen height. */
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

	update_decorate_size( win) ;
	SetWindowPos( win->my_window , win->cmd_show == SW_SHOWNA ? HWND_TOPMOST : 0 , 0 , 0 ,
		win->parent ? ACTUAL_WIDTH(win) : ACTUAL_WINDOW_WIDTH(win) ,
		win->parent ? ACTUAL_HEIGHT(win) : ACTUAL_WINDOW_HEIGHT(win) ,
		(win->cmd_show == SW_SHOWNA ? SWP_NOACTIVATE : 0) | SWP_NOMOVE | SWP_NOZORDER) ;
		
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
	x_resize_flag_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	return  x_window_resize( win , width - win->hmargin * 2 ,
		height - win->vmargin * 2 , flag) ;
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
	win->min_width = min_width ;
	win->min_height = min_height  ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;

	return  1 ;
}

int
x_window_set_override_redirect(
	x_window_t *  win ,
	int  flag
	)
{
	x_window_t *  root ;

	if( ( root = x_get_root_window( win))->my_window)
	{
	#if  0
		if( root->is_mapped)
		{
			ShowWindow( root->my_window , SW_HIDE) ;
		}
	#endif

		if( flag)
		{
			SetWindowLong( root->my_window , GWL_STYLE , 0) ;
			win->cmd_show = SW_SHOWNA ;

			SetWindowPos( root->my_window , HWND_TOPMOST , 0 , 0 , 0 , 0 ,
				SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER) ;
		}
		else
		{
			SetWindowLong( root->my_window , GWL_STYLE ,
				! win->parent ? WS_OVERLAPPEDWINDOW : WS_CHILD | WS_VISIBLE) ;
			win->cmd_show = SW_SHOWNORMAL ;
		}

	#if  0
		if( root->is_mapped)
		{
			ShowWindow( root->my_window , win->cmd_show) ;
		}
	#endif

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
x_window_set_borderless_flag(
	x_window_t *  win ,
	int  flag
	)
{
	return  0 ;
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

	update_decorate_size( win) ;

	SetWindowPos( win->my_window , 0 , win->x , win->y ,
		win->parent ? ACTUAL_WIDTH(win) : ACTUAL_WINDOW_WIDTH(win) ,
		win->parent ? ACTUAL_HEIGHT(win) : ACTUAL_WINDOW_HEIGHT(win) ,
		(win->cmd_show == SW_SHOWNA ? SWP_NOACTIVATE : 0) | SWP_NOSIZE | SWP_NOZORDER) ;

	return  1 ;
}

/*
 * This function can be used in context except window_exposed and update_window events.
 */
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

	r.left = x ;
	r.top = y ;
	
	/* XXX Garbage is left in screen in scrolling without +1 due to NULL_PEN ? */
	r.right = x + width /* + 1 */ ;
	r.bottom = y + height /* + 1 */ ;

	if( win->gc->gc == None)
	{
		InvalidateRect( win->my_window, &r, TRUE) ;

		return  1 ;
	}
	else
	{
		if( win->wall_picture)
		{
			Pixmap  pic ;
			int  src_x ;
			int  src_y ;

			if( win->wall_picture == ParentRelative)
			{
				pic = win->parent->wall_picture ;
				src_x = win->x ;
				src_y = win->y ;
			}
			else
			{
				pic = win->wall_picture ;
				src_x = src_y = 0 ;
			}

			BitBlt( win->gc->gc ,
				r.left , r.top , r.right - r.left , r.bottom - r.top ,
				pic , src_x + r.left , src_y + r.top , SRCCOPY) ;
		}
		else
		{
			HBRUSH  brush ;

			brush = x_acquire_brush( win->bg_color.pixel) ;
			FillRect( win->gc->gc , &r , brush) ;
			x_release_brush( brush) ;
		}

		return  1 ;
	}
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
	return  x_window_fill_with( win , &win->fg_color , x , y , width , height) ;
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
	if( win->gc->gc == None)
	{
		return  0 ;
	}

	if( height == 1 || width == 1)
	{
		x_release_pen( x_gc_set_pen( win->gc , x_acquire_pen( color->pixel))) ;
		MoveToEx( win->gc->gc , win->hmargin + x , win->vmargin + y , NULL) ;
		LineTo( win->gc->gc , win->hmargin + x + (width == 1 ? 0 : width) ,
		                      win->vmargin + y + (width == 1 ? height : 0)) ;
	}
	else
	{
		HBRUSH  brush ;
		RECT  r ;

		brush = x_acquire_brush( color->pixel) ;
		SetRect( &r , win->hmargin + x, win->vmargin + y,
				win->hmargin + x + width, win->vmargin + y + height) ;
		FillRect( win->gc->gc , &r , brush) ;
		x_release_brush( brush) ;
	}
	
	return  1 ;
}

/*
 * This function can be used in context except window_exposed and update_window events.
 */
int
x_window_blank(
	x_window_t *  win
	)
{
	int  get_dc ;
	HBRUSH  brush ;
	RECT  r ;

	if( win->gc->gc == None)
	{
		x_set_gc( win->gc , GetDC( win->my_window)) ;
		get_dc = 1 ;
	}
	else
	{
		get_dc = 0 ;
	}

	brush = x_acquire_brush( win->fg_color.pixel) ;
	SetRect( &r , win->hmargin , win->vmargin , win->width , win->height) ;
	FillRect( win->gc->gc , &r , brush) ;
	x_release_brush( brush) ;

	if( get_dc)
	{
		ReleaseDC( win->my_window , win->gc->gc) ;
		x_set_gc( win->gc , None) ;
	}

	return  1 ;
}

#if  0
/*
 * XXX
 * at the present time , not used and not maintained.
 */
int
x_window_fill_all_with(
	x_window_t *  win ,
	x_color_t *  color
	)
{
	return  0 ;
}
#endif

int
x_window_update(
	x_window_t *  win ,
	int  flag
	)
{
	if( win->update_window_flag)
	{
		/* WM_APP_PAINT has been already posted. */
		win->update_window_flag |= flag ;

		return  1 ;
	}
	else
	{
		win->update_window_flag = flag ;
	}

	/*
	 * WM_APP_PAINT message is posted only when update_window_flag is 0.
	 */
	PostMessage( win->my_window, WM_APP_PAINT, 0, 0) ;

	return  1 ;
}

int
x_window_update_all(
	x_window_t *  win
	)
{
	u_int  count ;

	InvalidateRect( win->my_window , NULL , FALSE) ;

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
 *              -1 => continuing default processing.
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
		int  val ;
		if( ( val = x_window_receive_event( win->children[count] , event)) != 0)
		{
			return  val ;
		}
	}

	if( event->window != win->my_window)
	{
		return  0 ;
	}

#ifndef  DISABLE_XDND
	if( x_dnd_filter_event( event, win))
	{
		/* event was consumed by xdnd handler. */
		return  1 ;
	}
#endif

  	switch(event->msg)
        {
        case WM_DESTROY:
		if( win->window_deleted)
		{
			(*win->window_deleted)( win) ;
		}

          	return  1 ;

	case WM_APP_PASTE:
		if( win->xct_selection_notified)
		{
			(*win->xct_selection_notified)( win , event->lparam , event->wparam) ;
			free( event->lparam) ;
		}

		return  1 ;

	case WM_APP_WPASTE:
		if( win->utf_selection_notified)
		{
			(*win->utf_selection_notified)( win , event->lparam , event->wparam * 2) ;
			free( event->lparam) ;
		}

		return  1 ;

	case WM_APP_PAINT:
		if( win->update_window)
		{
			x_set_gc( win->gc, GetDC( win->my_window)) ;

			(*win->update_window)( win , win->update_window_flag) ;

			ReleaseDC( win->my_window, win->gc->gc) ;
			x_set_gc( win->gc, None) ;
			win->update_window_flag = 0 ;
			
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG "WM_APP_PAINT_END\n") ;
		#endif
		}

		return  1 ;
		
        case WM_PAINT:
		if( win->window_exposed)
		{
			PAINTSTRUCT  ps ;
			int  margin_area_exposed ;
			int  x ;
			int  y ;
			u_int  width ;
			u_int  height ;

			x_set_gc( win->gc, BeginPaint( win->my_window, &ps)) ;

			margin_area_exposed = 0 ;

			if( ps.rcPaint.left < win->hmargin)
			{
				margin_area_exposed = 1 ;
				x = 0 ;
			}
			else
			{
				x = ps.rcPaint.left - win->hmargin ;
			}

			if( ps.rcPaint.top < win->vmargin)
			{
				margin_area_exposed = 1 ;
				y = 0 ;
			}
			else
			{
				y = ps.rcPaint.top - win->vmargin ;
			}

			if( ps.rcPaint.right > win->width - win->hmargin)
			{
				margin_area_exposed = 1 ;
				width = win->width - win->hmargin - x ;
			}
			else
			{
				/* +1 is not necessary ? */
				width = ps.rcPaint.right - x /* + 1 */ ;
			}

			if( ps.rcPaint.bottom > win->height - win->vmargin)
			{
				margin_area_exposed = 1 ;
				height = win->height - win->vmargin - y ;
			}
			else
			{
				/* +1 is not necessary ? */
				height = ps.rcPaint.bottom - y /* + 1 */ ;
			}

			if( margin_area_exposed)
			{
				clear_margin_area( win) ;
			}

			(*win->window_exposed)( win, x, y, width, height) ;

			EndPaint( win->my_window, &ps) ;
			x_set_gc( win->gc, None) ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG "WM_PAINT_END\n") ;
		#endif
		}
		
        	return  1 ;

	case WM_ERASEBKGND:
		{
			RECT  rt ;
			HBRUSH  old ;

			if( win->parent != NULL)	/* XXX Hack for not flashing. */
			{
				old = SelectObject( (HDC)event->wparam,
						x_acquire_brush( win->bg_color.pixel)) ;
				GetClientRect( win->my_window, &rt) ;
				PatBlt( (HDC)event->wparam, rt.left, rt.top,
					rt.right-rt.left, rt.bottom-rt.top, PATCOPY) ;
				x_release_brush( SelectObject( (HDC)event->wparam, old)) ;
			}
			
			return  1 ;
		}

	case WM_KEYDOWN:
		/*
		 * Dispatch event to all windows in case key was pressed in the
		 * scrollbar window but should be shown in the text area.
		 */
		x_xic_filter_event( x_get_root_window( win), event) ;

	case WM_SYSKEYDOWN:
		if( win->key_pressed)
		{
			XKeyEvent  kev ;

			kev.state = get_key_state() ;

			if( ( 0x30 <= event->wparam && event->wparam <= VK_DIVIDE) ||
			    event->wparam == VK_BACK || event->wparam == VK_RETURN ||
			    event->wparam == VK_ESCAPE || event->wparam == VK_SPACE)
			{
				kev.ch = event->wparam ;

				if( kev.state & ModMask)
				{
					if( event->msg == WM_SYSKEYDOWN)
					{
						/*
						 * Alt+key (which doesn't cause WM_*_CHAR message)
						 */

						if( 'A' <= kev.ch && kev.ch <= 'Z')
						{
							/*
							 * Upper case => Lower case.
							 * event->wparam is always upper case
							 * if alt key is pressed together.
							 */
							kev.ch += 0x20 ;
						}
					}
					else
					{
						/* wait for WM_*_CHAR message. */
						break ;
					}
				}
				else if( ( kev.state & ControlMask) &&
				         '0' <= kev.ch && kev.ch <= '9')
				{
					/*
					 * - See x_xic_get_str() in win32/x_xic.c.
					 * - Control+0-9 doesn't cause WM_*_CHAR message.
					 */
				}
				else
				{
					/* wait for WM_*_CHAR message. */
					break ;
				}
			}
			else if( event->wparam == VK_TAB)
			{
				kev.ch = event->wparam ;

				if( kev.state & ShiftMask)
				{
					event->wparam = XK_ISO_Left_Tab ;
					x_xic_filter_event( x_get_root_window( win), event) ;
				}

				if( kev.state & ControlMask)
				{
					/* (Shift+)Control+Tab doesn't cause WM_*_CHAR message. */
				}
				else
				{
					/* wait for WM_*_CHAR message. */
					break ;
				}
			}
			else if( event->msg == WM_SYSKEYDOWN)
			{
				if( kev.state & ModMask)
				{
					/*
					 * VK_OEM_XXX doesn't cause WM_*_CHAR message
					 * in WM_SYSKEYDOWN.
					 */
					if( ( kev.ch = oem_key_to_char( event->wparam)))
					{
						event->wparam = kev.ch ;
						x_xic_filter_event( x_get_root_window( win) ,
							event) ;
					}
				}
				else
				{
					kev.ch = 0 ;
				}

				if( kev.ch == 0 && event->wparam != VK_F10 /* Menu */)
				{
					break ;
				}
			}
			else if( VK_SHIFT <= event->wparam && event->wparam <= VK_MENU)
			{
				/* Don't call key_pressed event */
				break ;
			}
			else
			{
				kev.ch = 0 ;

				/*
				 * - See x_xic_get_str() in win32/x_xic.c.
				 * - Ctrl+Alt+key(AltGr+key) can cause WM_*_CHAR message later.
				 */
				if( ! ( kev.state & ModMask) &&
				    ( kev.state & ControlMask))
				{
					if( ( kev.ch = oem_key_to_char( event->wparam)))
					{
						/*
						 * VK_OEM_1 <= event->wparam &&
						 * event->wparam <= VK_OEM_102
						 */
						int  orig_wparam ;

						orig_wparam = event->wparam ;
						event->wparam = kev.ch ;
						x_xic_filter_event( x_get_root_window( win) ,
							event) ;

						if( orig_wparam == VK_OEM_4 ||	/* Ctl+[ */
						    orig_wparam == VK_OEM_5 ||	/* Ctl+\ */
						    orig_wparam == VK_OEM_6 ||	/* Ctl+] */
						    ( orig_wparam == VK_OEM_102 && /* Ctl+_ */
						      ! (kev.state & ShiftMask)) ||
						    ( orig_wparam == VK_OEM_MINUS && /* Ctrl+- */
						      (kev.state & ShiftMask)))
						{
							break ;
						}
						else
						{
							/*
							 * keys except VK_OEM_{4|5|6} and
							 * Shift+VK_OEM_102 don't
							 * cause WM_*_CHAR message.
							 */
						}
					}
				}
			}

			if( event->msg == WM_SYSKEYDOWN)
			{
				event->msg = WM_KEYDOWN ;
				x_xic_filter_event( x_get_root_window( win), event) ;
			}

			(*win->key_pressed)( win , &kev) ;

			return  1 ;
		}

		/* Continue default processing. */
		break ;

	case WM_IME_CHAR:
        case WM_CHAR:
		if( win->key_pressed)
		{
			XKeyEvent  kev ;

			kev.state = get_key_state() ;

			if( ( kev.state & (ControlMask|Mod1Mask)) == (ControlMask|Mod1Mask) &&
			    0x20 < event->wparam && event->wparam != 0x7f)
			{
				/* AltGr+key sends Ctrl+Alt+char */
				kev.state &= ~(ControlMask|Mod1Mask) ;
			}

		#if  0
			/*
			 * XXX
			 * For cmd.exe. If not converted '\r' to '\n', cmd prompt
			 * never goes to next line.
			 */
			if( event->wparam == '\r')
			{
				kev.ch = '\n' ;
			}
			else
		#endif
			{
				kev.ch = event->wparam ;
			}

			(*win->key_pressed)( win , &kev) ;
		}
		
		return  1 ;

	case WM_SETFOCUS:
	#if  0
		kik_debug_printf( "FOCUS IN %p\n" , win->my_window) ;
	#endif

		urgent_bell( win , 0) ;
		notify_focus_in_to_children( x_get_root_window( win)) ;

		break ;

	case WM_KILLFOCUS:
	#if  0
		kik_debug_printf( "FOCUS OUT %p\n" , win->my_window) ;
	#endif

		notify_focus_out_to_children( x_get_root_window( win)) ;

		break ;

	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		if( ! win->button_pressed)
		{
			return  1 ;
		}
		
		goto  BUTTON_MSG ;
		
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		if( ! win->button_released)
		{
			return  1 ;
		}

		BUTTON_MSG:
		{
			XButtonEvent  bev ;

			bev.time = GetMessageTime() ;

			bev.state = get_key_state() ;
		#if  0
			if( event->wparam & MK_LBUTTON)
			{
				bev.state |= Button1Mask ;
			}
			if( event->wparam & MK_MBUTTON)
			{
				bev.state |= Button2Mask ;
			}
			if( event->wparam & MK_RBUTTON)
			{
				bev.state |= Button3Mask ;
			}
		#endif
			if( event->wparam & MK_SHIFT)
			{
				bev.state |= ShiftMask ;
			}
			if( event->wparam & MK_CONTROL)
			{
				bev.state |= ControlMask ;
			}

			if( event->msg == WM_MOUSEWHEEL ||
			    event->msg == WM_MOUSEHWHEEL)
			{
				/* Note that WM_MOUSEWHEEL event is reported to top window. */

				POINT  p ;

				p.x = LOWORD(event->lparam) ;
				p.y = HIWORD(event->lparam) ;

				ScreenToClient( win->my_window , &p) ;

				bev.x = p.x - win->hmargin ;
				bev.y = p.y - win->vmargin ;

				if( ((SHORT)HIWORD(event->wparam)) > 0)
				{
					if( event->msg == WM_MOUSEHWHEEL)
					{
						/* Right */
						bev.button = Button7 ;
					}
					else
					{
						/* Up */
						bev.button = Button4 ;
					}
				}
				else
				{
					if( event->msg == WM_MOUSEHWHEEL)
					{
						/* Left */
						bev.button = Button6 ;
					}
					else
					{
						/* Down */
						bev.button = Button5 ;
					}
				}
			}
			else
			{
				bev.x = LOWORD(event->lparam) - win->hmargin ;
				bev.y = HIWORD(event->lparam) - win->vmargin ;

				if( event->msg == WM_LBUTTONDOWN || event->msg == WM_LBUTTONUP)
				{
					bev.button = Button1 ;
				}
				else if( event->msg == WM_MBUTTONDOWN ||
					event->msg == WM_MBUTTONUP)
				{
					bev.button = Button2 ;
				}
				else /* if( event->msg == WM_RBUTTONDOWN ||
					event->msg == WM_RBUTTONUP) */
				{
					bev.button = Button3 ;
				}
			}

			/* XXX grandchild windows aren't regarded for now. */
			for( count = 0 ; count < win->num_of_children ; count++)
			{
				if( bev.x <= win->children[count]->x &&
				    bev.y <= win->children[count]->y &&
				    win->children[count]->x + ACTUAL_WIDTH(win->children[count])
				          <= bev.x + 1 &&
				    win->children[count]->y + ACTUAL_HEIGHT(win->children[count])
				          <= bev.y + 1)
				{
					win = win->children[count] ;
					bev.x -= win->x ;
					bev.y -= win->y ;
					break ;
				}
			}

			if( event->msg == WM_MOUSEWHEEL || event->msg == WM_LBUTTONDOWN ||
				event->msg == WM_RBUTTONDOWN || event->msg == WM_MBUTTONDOWN)
			{
				if( win->click_num == MAX_CLICK)
				{
					win->click_num = 0 ;
				}

				if( win->prev_clicked_time + click_interval >= bev.time &&
					bev.button == win->prev_clicked_button)
				{
					win->click_num ++ ;
					win->prev_clicked_time = bev.time ;
				}
				else
				{
					win->click_num = 1 ;
					win->prev_clicked_time = bev.time ;
					win->prev_clicked_button = bev.button ;
				}

				(*win->button_pressed)( win , &bev , win->click_num) ;
				
				win->button_is_pressing = 1 ;
				win->prev_button_press_event = bev ;
				
			#if  0
				kik_debug_printf( KIK_DEBUG_TAG
					" mouse pressed btn %d stat %d x %d y %d click_num %d\n",
					bev.button, bev.state, bev.x, bev.y, win->click_num) ;
			#endif
			}
			else /* if( event->msg == WM_LBUTTONUP || event->msg == WM_RBUTTONUP ||
				event->msg == WM_MBUTTONUP) */
			{
				(*win->button_released)( win , &bev) ;
				
				win->button_is_pressing = 0 ;
				
			#if  0
				kik_debug_printf( KIK_DEBUG_TAG
					" mouse released... state %d x %d y %d\n",
					bev.state, bev.x, bev.y) ;
			#endif
			}

			if( ! win->is_focused && win->inputtable &&
			    bev.button == Button1 && ! bev.state)
			{
				x_window_set_input_focus( win) ;
			}
		}

		return  1 ;

	case  WM_MOUSEMOVE:
		if( win->button_is_pressing || win->pointer_motion)
		{
			XMotionEvent  mev ;

			mev.time = GetMessageTime() ;

			mev.x = LOWORD(event->lparam) - win->hmargin ;
			mev.y = HIWORD(event->lparam) - win->vmargin ;

			mev.state = get_key_state() ;

			if( event->wparam & MK_LBUTTON)
			{
				mev.state |= Button1Mask ;
			}
			if( event->wparam & MK_MBUTTON)
			{
				mev.state |= Button2Mask ;
			}
			if( event->wparam & MK_RBUTTON)
			{
				mev.state |= Button3Mask ;
			}

			if( ! mev.state)
			{
				win->button_is_pressing = 0 ;
			}
			
			if( event->wparam & MK_SHIFT)
			{
				mev.state |= ShiftMask ;
			}
			if( event->wparam & MK_CONTROL)
			{
				mev.state |= ControlMask ;
			}

			if( win->button_is_pressing)
			{
				/*
				 * prev_button_press_event.{x|y} can be the same
				 * as mev.{x|y} when window is pressed and focused.
				 */
				if( win->button_motion &&
				    ( win->prev_button_press_event.x != mev.x ||
				      win->prev_button_press_event.y != mev.y))
				{
					(*win->button_motion)( win , &mev) ;
				}

				/* following button motion ... */
				win->prev_button_press_event.x = mev.x ;
				win->prev_button_press_event.y = mev.y ;
				win->prev_button_press_event.time = mev.time ;

			#if  0
				kik_debug_printf( KIK_DEBUG_TAG
					" button motion... state %d x %d y %d\n" ,
					mev.state , mev.x , mev.y) ;
			#endif
			}
			/*
			 * win->pointer_motion should be checked again here because
			 * win->button_is_pressing was changed from 1 to 0 above.
			 */
			else if( win->pointer_motion)
			{
				(*win->pointer_motion)( win , &mev) ;

			#if  0
				kik_debug_printf( KIK_DEBUG_TAG
					" pointer motion... state %d x %d y %d\n" ,
					mev.state , mev.x , mev.y) ;
			#endif
			}
		}
		
		return  1 ;

	case  WM_SETCURSOR:
		{
			Cursor  cursor ;

			if( win->cursor_shape == XC_nil)
			{
				SetCursor( NULL) ;
			}
			else if( ( cursor = x_display_get_cursor( win->disp , win->cursor_shape)))
			{
				SetCursor( cursor) ;
			}
			else
			{
				break ;
			}

			return  1 ;
		}

	case  WM_RENDERALLFORMATS:
		OpenClipboard( win->my_window) ;
		EmptyClipboard() ;

	case  WM_RENDERFORMAT:
		if( event->wparam == CF_UNICODETEXT)
		{
			if( win->utf_selection_requested)
			{
				(*win->utf_selection_requested)( win, NULL, CF_UNICODETEXT) ;

			#if  0
				kik_debug_printf( KIK_DEBUG_TAG "utf_selection_requested\n") ;
			#endif
			}
		}
		else if( event->wparam == CF_TEXT)
		{
			if( win->xct_selection_requested)
			{
				(*win->xct_selection_requested)( win, NULL, CF_TEXT) ;
			}
		}

		if( event->msg == WM_RENDERALLFORMATS)
		{
			CloseClipboard() ;
		}

		return  1 ;

	case  WM_DESTROYCLIPBOARD:
		if( win->is_sel_owner == 1)
		{
			/*
			 * Call win->selection_cleared and win->is_sel_owner is set 0
			 * in x_display_clear_selection.
			 */
			x_display_clear_selection( win->disp , win) ;
		}
		else if( win->is_sel_owner > 1)
		{
			win->is_sel_owner -- ;
		}

		return  1 ;

	case  WM_MOVE:
		if( win->parent == NULL)
		{
			win->x = LOWORD(event->lparam) ;
			win->y = HIWORD(event->lparam) ;

		#if  0
			kik_debug_printf( "WM_MOVE x %d y %d\n" , win->x , win->y) ;
		#endif

			notify_move_to_children( win) ;
		}

		return  1 ;
	
	case  WM_SIZE:
		if( win->window_resized && ! IsIconic( win->my_window))
		{
			/*
			 * Assume that win == root.
			 */
			 
			u_int  width ;
			u_int  height ;
			u_int  min_width ;
			u_int  min_height ;

			width = LOWORD(event->lparam) ;
			height = HIWORD(event->lparam) ;

			min_width = total_min_width(win) ;
			min_height = total_min_height(win) ;
		
			if( width < min_width + win->hmargin * 2 ||
			    height < min_height + win->vmargin * 2)
			{
				x_window_resize( win,
					K_MAX(min_width + win->hmargin * 2 , width) -
						win->hmargin * 2 ,
					K_MAX(min_height + win->vmargin * 2 , height) -
						win->vmargin * 2 ,
					NOTIFY_TO_MYSELF) ;
			}
			else if( width != ACTUAL_WIDTH(win) ||
			         height != ACTUAL_HEIGHT(win))
			{
				u_int  width_surplus ;
				u_int  height_surplus ;

				if( IsZoomed( x_get_root_window( win)->my_window))
				{
					width_surplus = height_surplus = 0 ;
				}
				else
				{
					width_surplus = (width - min_width -
							 win->hmargin * 2) %
							total_width_inc(win) ;

					height_surplus = (height - min_height -
							  win->vmargin * 2) %
							total_height_inc(win) ;
				}

				win->width = width - win->hmargin * 2 ;
				win->height = height - win->vmargin * 2 ;

				if( width_surplus > 0 || height_surplus > 0)
				{
					x_window_resize( win,
						width - win->hmargin * 2 - width_surplus,
						height - win->vmargin * 2 - height_surplus,
						NOTIFY_TO_MYSELF) ;
				}
				else
				{
					x_window_clear_all( win) ;

					(*win->window_resized)( win) ;
				}
			}

			notify_move_to_children( win) ;
		}

		return  1 ;

	/* Not necessary */
#if  0
	case  WM_THEMECHANGED:
		update_decorate_size( win) ;

		return  1 ;
#endif
	}

	return  -1 ;
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

	BitBlt( win->gc->gc, win->hmargin, win->vmargin + boundary_start,		/* dst */
		win->width, boundary_end - boundary_start - height,		/* size */
		win->gc->gc, win->hmargin, win->vmargin + boundary_start + height,	/* src */
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

	BitBlt( win->gc->gc, win->hmargin, win->vmargin + boundary_start + height,	/* dst */
		win->width, boundary_end - boundary_start - height,		/* size */
		win->gc->gc, win->hmargin , win->vmargin + boundary_start,		/* src */
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

	BitBlt( win->gc->gc , win->hmargin + boundary_start , win->vmargin ,	/* dst */
		boundary_end - boundary_start - width , win->height ,		/* size */
		win->gc->gc , win->hmargin + boundary_start + width , win->vmargin ,/* src */
		SRCCOPY) ;

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

	BitBlt( win->gc->gc , win->hmargin + boundary_start + width , win->vmargin ,/* dst */
		boundary_end - boundary_start - width , win->height ,		/* size */
		win->gc->gc , win->hmargin + boundary_start , win->vmargin ,	/* src */
		SRCCOPY) ;

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
#ifndef  DONT_OPTIMIZE_DRAWING_PICTURE
	int  tmp_gc ;
#endif

	if( dst_x >= win->width || dst_y >= win->height)
	{
		return  0 ;
	}

	/* for xterm_show_picture() */
#ifndef  DONT_OPTIMIZE_DRAWING_PICTURE
	if( win->gc->gc == None)
	{
		x_set_gc( win->gc , GetDC( win->my_window)) ;
		tmp_gc = 1 ;
	}
	else
	{
		tmp_gc = 0 ;
	}
#endif

	if( dst_x + width > win->width)
	{
		width = win->width - dst_x ;
	}

	if( dst_y + height > win->height)
	{
		height = win->height - dst_y ;
	}

	if( mask)
	{
		MaskBlt( win->gc->gc , win->hmargin + dst_x , win->vmargin + dst_y ,
			width , height , src , src_x , src_y , mask , src_x , src_y ,
			MAKEROP4(SRCCOPY,0x00aa0029)) ;
	}
	else
	{
		BitBlt( win->gc->gc , win->hmargin + dst_x , win->vmargin + dst_y ,
			width , height , src , src_x , src_y , SRCCOPY) ;
	}

#ifndef  DONT_OPTIMIZE_DRAWING_PICTURE
	if( tmp_gc)
	{
		ReleaseDC( win->my_window , win->gc->gc) ;
		x_set_gc( win->gc , None) ;
	}
#endif

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
	HRGN  r = CreateRectRgn( x + win->hmargin , y + win->vmargin ,
			x + width + win->hmargin , y + height + win->vmargin) ;

	SelectClipRgn( win->gc->gc , r) ;
	DeleteObject( r) ;
}

void
x_window_unset_clip(
	x_window_t *  win
	)
{
	SelectClipRgn( win->gc->gc , NULL) ;
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
	return  x_window_draw_string( win , font , fg_color , x , y , str , len) ;
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
	return  x_window_draw_image_string( win , font , fg_color , bg_color ,
			x , y , str , len) ;
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
	if( win->gc->gc == None)
	{
		return  0 ;
	}

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

	x_gc_set_fid( win->gc, font->fid) ;
	x_gc_set_fg_color( win->gc, fg_color->pixel) ;
	
	/*
	 * XXX Hack
	 * In case US_ASCII characters is drawn by Unicode font.
	 * 8 bit charcter => 16 bit character.
	 */
	if( FONT_CS(font->id) == ISO10646_UCS4_1)
	{
		u_char *  dbl_str ;

		if( ( dbl_str = alloca( len * 2)))
		{
			u_int  count ;

			for( count = 0 ; count < len ; count++)
			{
				/* Little Endian */
				dbl_str[count * 2] = str[count] ;
				dbl_str[count * 2 + 1] = 0x0 ;
			}

			draw_string( win , font , x , y , dbl_str , len * 2 , 1) ;
		}
	}
	else
	{
		draw_string( win , font , x , y , str , len , 1) ;
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
	if( win->gc->gc == None)
	{
		return  0 ;
	}

	x_gc_set_fid( win->gc, font->fid) ;
	x_gc_set_fg_color( win->gc, fg_color->pixel) ;
	
	draw_string( win , font , x , y , (u_char*)str , len * 2 , 1) ;

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
	if( win->gc->gc == None)
	{
		return  0 ;
	}

	x_gc_set_fid( win->gc, font->fid) ;
	x_gc_set_fg_color( win->gc, fg_color->pixel) ;
	x_gc_set_bg_color( win->gc, bg_color->pixel) ;

	/*
	 * XXX Hack
	 * In case US_ASCII characters is drawn by Unicode font.
	 * 8 bit charcter => 16 bit character.
	 */
	if( FONT_CS(font->id) == ISO10646_UCS4_1)
	{
		u_char *  dbl_str ;

		if( ( dbl_str = alloca( len * 2)))
		{
			u_int  count ;

			for( count = 0 ; count < len ; count++)
			{
				/* Little Endian */
				dbl_str[count * 2] = str[count] ;
				dbl_str[count * 2 + 1] = 0x0 ;
			}

			draw_string( win , font , x , y , dbl_str , len * 2 , 0) ;
		}
	}
	else
	{
		draw_string( win , font , x , y , str , len , 0) ;
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
	if( win->gc->gc == None)
	{
		return  0 ;
	}

	x_gc_set_fid( win->gc, font->fid) ;
	x_gc_set_fg_color( win->gc, fg_color->pixel) ;
	x_gc_set_bg_color( win->gc, bg_color->pixel) ;

	draw_string( win, font, x, y, (u_char*)str, len * 2 , 0) ;

	return  1 ;
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
	if( win->gc->gc == None)
	{
		return  0 ;
	}

	x_release_pen( x_gc_set_pen( win->gc, x_acquire_pen( win->fg_color.pixel))) ;
	x_release_brush( x_gc_set_brush( win->gc, GetStockObject(NULL_BRUSH))) ;
	
	Rectangle( win->gc->gc, x1 + win->hmargin , y1 + win->vmargin ,
			x2 + win->hmargin , y2 + win->vmargin) ;

	return  1 ;
}

int
x_set_use_clipboard_selection(
	int  use_it
	)
{
	return  0 ;
}

int
x_is_using_clipboard_selection(void)
{
	return  0 ;
}

int
x_window_set_selection_owner(
	x_window_t *  win ,
	Time  time
	)
{
#if  0
	kik_debug_printf( KIK_DEBUG_TAG " x_window_set_selection_owner.\n") ;
#endif

	if( ( ! win->is_sel_owner &&
	      ! x_display_own_selection( win->disp , win)) ||
	    OpenClipboard( win->my_window) == FALSE)
	{
		return  0 ;
	}

	/*
	 * If win->is_sel_owner is already 1, win->is_sel_owner++ prevents
	 * WM_DESTROYCLIPBOARD by EmtpyClipboard() from calling
	 * x_display_clear_selection().
	 */
	win->is_sel_owner ++ ;

	EmptyClipboard() ;	/* Own clipboard. Create WM_DESTROYCLIPBOARD message */
	SetClipboardData( CF_TEXT, NULL) ;
	SetClipboardData( CF_UNICODETEXT, NULL) ;

	CloseClipboard() ;

	return  1 ;
}

int
x_window_xct_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
	return  invoke_selection_request( win , CF_TEXT , xct_selection_request) ;
}

int
x_window_utf_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
	return  invoke_selection_request( win , CF_UNICODETEXT , utf_selection_request) ;
}

int
x_window_send_picture_selection(
	x_window_t *  win ,
	Pixmap  pixmap ,
	u_int  width ,
	u_int  height
	)
{
	HBITMAP  hbmp ;
	HGDIOBJ old ;
	HDC  hdc ;

	if( MessageBox( win->my_window , "Set this picture to the clipboard." ,
		"" , MB_OKCANCEL) != IDOK)
	{
		return  0 ;
	}

	hbmp = CreateCompatibleBitmap( pixmap , width , height) ;
	hdc = CreateCompatibleDC( pixmap) ;
	old = SelectObject( hdc , hbmp) ;
	BitBlt( hdc , 0 , 0 , width , height , pixmap , 0 , 0 , SRCCOPY) ;
	SelectObject( hdc , old) ;
	DeleteDC( hdc) ;

	if( ( ! win->is_sel_owner &&
	      ! x_display_own_selection( win->disp , win)) ||
	    OpenClipboard( win->my_window) == FALSE)
	{
		return  0 ;
	}

	/*
	 * If win->is_sel_owner is already 1, win->is_sel_owner++ prevents
	 * WM_DESTROYCLIPBOARD by EmtpyClipboard() from calling
	 * x_display_clear_selection().
	 */
	win->is_sel_owner ++ ;

	EmptyClipboard() ;
	SetClipboardData( CF_BITMAP , hbmp) ;
	CloseClipboard() ;

	DeleteObject( hbmp) ;

	return  1 ;
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
	HGLOBAL  hmem ;
	u_char *  g_data ;
	size_t  count ;

	if( sel_data == NULL || sel_len == 0)
	{
		return  0 ;
	}
	
	/* +1 for 0x00, +2 for 0x0000(UTF16) */
	if( ( hmem = GlobalAlloc( GHND, sel_len + (sel_type == CF_UNICODETEXT ? 2 : 1))) == NULL)
	{
		return  0 ;
	}

	if( ( g_data = GlobalLock( hmem)) == NULL)
	{
		GlobalFree( hmem) ;

		return  0 ;
	}

	for( count = 0 ; count < sel_len ; count ++)
	{
		*(g_data++) = *(sel_data++) ;
	}
	/* *(g_data++) = 0x0 is not necessary because GlobalAlloc already cleared memory. */

	GlobalUnlock( hmem) ;

	SetClipboardData( sel_type, hmem) ;

#if  0
	kik_debug_printf( KIK_DEBUG_TAG " x_window_send_selection.\n") ;
#endif

	return  1 ;
}

int
x_set_window_name(
	x_window_t *  win ,
	u_char *  name
	)
{
	x_window_t *  root ;

	root = x_get_root_window( win) ;

	if( name == NULL)
	{
		name = root->app_name ;

	#ifndef  UTF16_IME_CHAR
		SetWindowTextA( root->my_window , name) ;

		return  1 ;
	#endif
	}

	SetWindowTextW( root->my_window , name) ;

	return  1 ;
}

int
x_set_icon_name(
	x_window_t *  win ,
	u_char *  name
	)
{
	return  0 ;
}

int
x_window_set_icon(
	x_window_t *  win ,
	x_icon_picture_t *  icon
	)
{
	return  0 ;
}

int
x_window_remove_icon(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_window_reset_group(
	x_window_t *  win
	)
{
	return  0 ;
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
	return  ~0 ;
}

u_int
x_window_get_mod_meta_mask(
	x_window_t *  win ,
	char *  mod_key
	)
{
	return  ModMask ;
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
		int  count ;

		x_set_gc( win->gc, GetDC( win->my_window)) ;

		/* win->gc is used in x_window_blank(). */
		x_window_blank( win) ;

		for( count = 0 ; count < 10 ; count++)
		{
			Sleep( 10) ;
		}

		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;

		ReleaseDC( win->my_window, win->gc->gc) ;
		x_set_gc( win->gc, None) ;
	}

	if( mode & BEL_SOUND)
	{
		Beep( 800 , 200) ;
	}

	return  1 ;
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
	POINT  p ;

	p.x = x ;
	p.y = y ;

	ClientToScreen( win->my_window , &p) ;

	*global_x = p.x ;
	*global_y = p.y ;
	*child = None ;
	
	return  0 ;
}

void
x_window_set_input_focus(
	x_window_t *  win
	)
{
	reset_input_focus( x_get_root_window( win)) ;
	win->inputtable = 1 ;
	SetFocus( win->my_window) ;
}

#ifdef  DEBUG
void
x_window_dump_children(
	x_window_t *  win
	)
{
	u_int  count ;

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
