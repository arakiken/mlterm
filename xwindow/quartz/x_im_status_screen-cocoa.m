/*
 *	$Id$
 */

#import  <Cocoa/Cocoa.h>
#include  "../x_im_status_screen.h"

#ifdef  USE_IM_PLUGIN

#include  <kiklib/kik_mem.h>
#include  <ml_char_encoding.h>


/* --- static functions --- */

/*
 * methods of x_im_status_screen_t
 */

static int
delete(
	x_im_status_screen_t *  stat_screen
	)
{
	NSWindow *  window = stat_screen->window.my_window ;

	[window release] ;

	free( stat_screen) ;

	return  1 ;
}

static int
show(
	x_im_status_screen_t *  stat_screen
	)
{
	NSWindow *  window = stat_screen->window.my_window ;

	[window orderFront:window] ;

	return  1 ;
}

static int
hide(
	x_im_status_screen_t *  stat_screen
	)
{
	NSWindow *  window = stat_screen->window.my_window ;

	[window orderOut:window] ;

	return  1 ;
}

static int
set_spot(
	x_im_status_screen_t *  stat_screen ,
	int  x ,
	int  y
	)
{
	NSWindow *  window = stat_screen->window.my_window ;

	[window setFrameOrigin:NSMakePoint( x ,
			[[window screen] visibleFrame].size.height -
			[window.contentView frame].size.height - y)] ;

	stat_screen->x = x ;
	stat_screen->y = y ;

	return  1 ;
}

static int
set(
	x_im_status_screen_t *  stat_screen ,
	mkf_parser_t *  parser ,
	u_char *  str
	)
{
	size_t  len ;
	u_char *  utf8 ;
	mkf_char_t  ch ;
	u_int  count ;

	len = strlen( str) ;

	(*parser->init)( parser) ;
	(*parser->set_str)( parser , str , len) ;

	for( count = 0 ; (*parser->next_char)( parser , &ch) ; count++) ;

	if( ! ( utf8 = alloca( count * UTF_MAX_SIZE + 1)))
	{
		return  0 ;
	}

	(*parser->init)( parser) ;
	(*parser->set_str)( parser , str , len) ;
	len = ml_char_encoding_convert_with_parser( utf8 , count * UTF_MAX_SIZE ,
			ML_UTF8 , parser) ;
	utf8[len] = '\0' ;

	NSWindow *  window = stat_screen->window.my_window ;
	NSTextField *  label = window.contentView ;

	label.stringValue = [NSString stringWithCString:utf8 encoding:NSUTF8StringEncoding] ;
	[label sizeToFit] ;
	[window setContentSize:label.frame.size] ;

	/* XXX (see x_im_status_screen_new()) */
	if( stat_screen->filled_len == 1)
	{
		[window orderFront:window] ;
		set_spot( stat_screen , stat_screen->x , stat_screen->y) ;
		stat_screen->filled_len = 0 ;
	}

	return  1 ;
}


/* --- global functions --- */

x_im_status_screen_t *
x_im_status_screen_new(
	x_display_t *  disp ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	int  is_vertical ,
	u_int  line_height ,
	int  x ,
	int  y
	)
{
	x_im_status_screen_t *  stat_screen ;

	if( ( stat_screen = calloc( 1 , sizeof( x_im_status_screen_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		return  NULL ;
	}

	stat_screen->font_man = font_man ;
	stat_screen->color_man = color_man ;

	stat_screen->x = x ;
	stat_screen->y = y ;
	stat_screen->line_height = line_height ;

	stat_screen->is_vertical = is_vertical ;

	NSWindow *  window = [[NSWindow alloc] initWithContentRect:NSMakeRect( 0 , 100 , 0 , 0)
				styleMask:NSBorderlessWindowMask
				backing:NSBackingStoreBuffered
				defer:YES] ;
	[window setLevel:NSStatusWindowLevel] ;

	NSTextField *  label = [[NSTextField alloc] initWithFrame:NSMakeRect( 0 , 0 , 10 , 10)] ;
	label.drawsBackground = YES ;
	[label setEditable:NO] ;
	[label setSelectable:NO] ;

	x_color_t *  fg = x_get_xcolor( stat_screen->color_man , ML_FG_COLOR) ;
	x_color_t *  bg = x_get_xcolor( stat_screen->color_man , ML_BG_COLOR) ;
	[label setTextColor:[NSColor colorWithCalibratedRed:
					((fg->pixel >> 16) & 0xff) / 255.0
					green: ((fg->pixel >> 8) & 0xff) / 255.0
					blue: (fg->pixel & 0xff) / 255.0
					alpha: 1.0]] ;
	[label setBackgroundColor:[NSColor colorWithCalibratedRed:
					((bg->pixel >> 16) & 0xff) / 255.0
					green: ((bg->pixel >> 8) & 0xff) / 255.0
					blue: (bg->pixel & 0xff) / 255.0
					alpha: 1.0]] ;
	[window setContentView:label] ;

	stat_screen->window.my_window = window ;

	/* methods of x_im_status_screen_t */
	stat_screen->delete = delete ;
	stat_screen->show = show ;
	stat_screen->hide = hide ;
	stat_screen->set_spot = set_spot ;
	stat_screen->set = set ;

	/* XXX (see set()) */
	stat_screen->filled_len = 1 ;

	return  stat_screen ;
}

#else /* ! USE_IM_PLUGIN */

x_im_status_screen_t *
x_im_status_screen_new(
	x_display_t *  disp ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	int  is_vertical ,
	u_int  line_height ,
	int  x ,
	int  y
	)
{
	return  NULL ;
}

#endif
