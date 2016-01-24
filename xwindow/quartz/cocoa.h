/*
 *	$Id$
 */

#ifndef  __COCOA_H__
#define  __COCOA_H__


#ifdef  __X_WINDOW_H__

/* for NSScroller */
void  scroller_update( void *  scroller , float  pos , float  knob) ;


/* for MLTermView */

void  view_alloc( x_window_t *  xwindow) ;

void  view_dealloc( void *  view) ;

void  view_update( void *  view , int  flag) ;

void  view_set_clip( void *  view , int  x , int  y , u_int  width , u_int  height) ;

void  view_unset_clip( void *  view) ;

void  view_draw_string( void *  view , x_font_t *  font , x_color_t *  fg_color ,
	int  x , int  y , char *  str , size_t  len) ;

void  view_draw_string16( void *  view , x_font_t *  font , x_color_t *  fg_color ,
	int  x , int  y , XChar2b *  str , size_t  len) ;

void  view_fill_with( void *  view , x_color_t *  color , int  x , int  y ,
	u_int  width , u_int  height) ;

void  view_draw_rect_frame( void *  view , x_color_t *  color , int  x1 , int  y1 ,
	int  x2 , int  y2) ;

void  view_copy_area( void *  view , Pixmap  src , int  src_x , int  src_y ,
	u_int  width , u_int  height , int  dst_x , int  dst_y) ;

void  view_scroll( void *  view , int  src_x , int  src_y , u_int  width , u_int  height ,
	int  dst_x , int  dst_y) ;

void  view_bg_color_changed( void *  view) ;

void  view_visual_bell( void *  view) ;


/* for NSView */

void  view_set_input_focus( void *  view) ;

void  view_set_rect( void *  view , int  x , int  y , u_int  width , u_int  height) ;

void  view_set_hidden( void *  view , int  flag) ;


/* for NSWindow */

void  window_alloc( x_window_t *  root) ;

void  window_dealloc( void *  window) ;

void  window_resize( void *  window , int  width , int  height) ;

void  window_accepts_mouse_moved_events( void *  window , int  accept) ;


/* for NSApp */
void  app_urgent_bell( int  on) ;


/* for NSScroller */

void  scroller_update( void *  scroller , float  pos , float  knob) ;


/* for Clipboard */

int  cocoa_clipboard_own( void *  view) ;

void  cocoa_clipboard_set( const u_char *  utf8 , size_t  len) ;

const char *  cocoa_clipboard_get(void) ;

void  cocoa_beep(void) ;

#endif	/* __X_WINDOW_H__ */

#ifdef  __X_FONT_H__

/* for CGFont */

void *  cocoa_create_font( const char *  font_family) ;

char *  cocoa_get_font_path( void *  cg_font) ;

void  cocoa_release_font( void *  cg_font) ;

#endif

#ifdef  __X_IMAGELIB_H__

/* for CGImage */

Pixmap  cocoa_load_image( const char *  path , u_int *  width , u_int *  height) ;

#endif


/* Utility */

int  cocoa_add_fd( int  fd , void  (*handler)(void)) ;

int  cocoa_remove_fd( int  fd) ;


#endif
