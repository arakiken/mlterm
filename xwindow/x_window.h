/*
 *	$Id$
 */

/*
 * ANTI_ALIAS macro should be concerned.
 */

#ifndef  __X_WINDOW_H__
#define  __X_WINDOW_H__


#include  <X11/Xlib.h>
#include  <X11/cursorfont.h>	/* for cursor shape */

#ifdef  ANTI_ALIAS
#include  <X11/Xft/Xft.h>
#endif

#include  <kiklib/kik_types.h>
#include  <mkf/mkf_parser.h>

#include  "x_font_manager.h"
#include  "x_color_manager.h"
#include  "ml_char.h"
#include  "x_picture.h"
#include  "ml_logical_visual.h"		/* ml_vertical_mode_t */


#define  ACTUAL_WIDTH(win)  ((win)->width + (win)->margin * 2)
#define  ACTUAL_HEIGHT(win)  ((win)->height + (win)->margin * 2)


typedef enum  x_event_dispatch
{
	NOTIFY_TO_NONE = 0x0 ,
	NOTIFY_TO_CHILDREN = 0x01 ,
	NOTIFY_TO_PARENT = 0x02 ,
	NOTIFY_TO_MYSELF = 0x04 ,

} x_event_dispatch_t ;

typedef struct  x_xim_event_listener
{
	void *  self ;

	int  (*get_spot)( void *  , int * , int *) ;
	XFontSet  (*get_fontset)( void *) ;
	u_long  (*get_fg_color)( void *) ;
	u_long  (*get_bg_color)( void *) ;

} x_xim_event_listener_t ;

typedef struct x_window_manager *  x_window_manager_ptr_t ;

typedef struct x_xic *  x_xic_ptr_t ;

typedef struct x_xim *  x_xim_ptr_t ;

typedef struct  x_window
{
	Display *  display ;
	int  screen ;

	Window  parent_window ;
	Window  my_window ;

	Drawable  drawable ;

	Pixmap  pixmap ;

#ifdef  ANTI_ALIAS
	XftDraw *  xft_draw ;
#endif

	GC  gc ;	/* for generic use */
	GC  ch_gc ;	/* for drawing string */
	
	u_long  fg_color ;
	u_long  bg_color ;

	x_window_manager_ptr_t  win_man ;
	
	struct x_window *  parent ;
	struct x_window **  children ;
	u_int  num_of_children ;
	
	u_int  cursor_shape ;

	long  event_mask ;

	int  x ;
	int  y ;
	u_int  width ;
	u_int  height ;
	u_int  min_width ;
	u_int  min_height ;
	u_int  width_inc ;
	u_int  height_inc ;
	
	/* actual window size is +margin on north/south/east/west */
	u_int  margin ;
	
	/* used by x_xim */
	x_xic_ptr_t  xic ;
	x_xim_ptr_t  xim ;
	x_xim_event_listener_t *  xim_listener ;

	/* button */
	Time  prev_clicked_time ;
	int  prev_clicked_button ;
	XButtonEvent  prev_button_press_event ;

	x_picture_modifier_t *  pic_mod ;
	
	/*
	 * XDND
	 */
	Window  dnd_source ;
	Atom  is_dnd_accepting ;

	/*
	 * WMHints Icon
	 */
	Pixmap icon ;
	Pixmap mask ;

	/*
	 * flags etc.
	 */
	 
	int8_t  use_pixmap ;
	int8_t  wall_picture_is_set ;
	int8_t  is_transparent ;
	int8_t  is_scrollable ;
	int8_t  is_mapped ;

	/* button */
	int8_t  button_is_pressing ;
	int8_t  click_num ;

	void (*window_realized)( struct x_window *) ;
	void (*window_finalized)( struct x_window *) ;
	void (*window_exposed)( struct x_window * , int , int , u_int , u_int) ;
	void (*window_focused)( struct x_window *) ;
	void (*window_unfocused)( struct x_window *) ;
	void (*key_pressed)( struct x_window * , XKeyEvent *) ;
	void (*button_motion)( struct x_window * , XMotionEvent *) ;
	void (*button_released)( struct x_window * , XButtonEvent *) ;
	void (*button_pressed)( struct x_window * , XButtonEvent * , int) ;
	void (*button_press_continued)( struct x_window * , XButtonEvent *) ;
	void (*window_resized)( struct x_window *) ;
	void (*child_window_resized)( struct x_window * , struct x_window *) ;
	void (*selection_cleared)( struct x_window * , XSelectionClearEvent *) ;
	void (*xct_selection_requested)( struct x_window * , XSelectionRequestEvent * , Atom) ;
	void (*utf8_selection_requested)( struct x_window * , XSelectionRequestEvent * , Atom) ;
	void (*xct_selection_notified)( struct x_window * , u_char * , size_t) ;
	void (*utf8_selection_notified)( struct x_window * , u_char * , size_t) ;
	void (*xct_selection_request_failed)( struct x_window * , XSelectionEvent *) ;
	void (*utf8_selection_request_failed)( struct x_window * , XSelectionEvent *) ;
	void (*window_deleted)( struct x_window *) ;
	
} x_window_t ;


int  x_window_init( x_window_t *  win ,
	u_int  width , u_int  height , u_int  min_width , u_int  min_height ,
	u_int  width_inc , u_int  height_inc , u_int  margin) ;

int  x_window_final( x_window_t *  win) ;

int  x_window_set_wall_picture( x_window_t *  win , Pixmap  pic) ;

int  x_window_unset_wall_picture( x_window_t *  win) ;

int  x_window_set_transparent( x_window_t *  win , x_picture_modifier_t *  pic_mod) ;

int  x_window_unset_transparent( x_window_t *  win) ;

int  x_window_use_pixmap( x_window_t *  win) ;

int  x_window_set_cursor( x_window_t *  win , u_int  cursor_shape) ;

int  x_window_set_fg_color( x_window_t *  win , u_long  fg_color) ;

int  x_window_set_bg_color( x_window_t *  win , u_long  bg_color) ;

int  x_window_add_child( x_window_t *  win , x_window_t *  child , int  x , int  y , int  map) ;

x_window_t *  x_get_root_window( x_window_t *  win) ;

int  x_window_init_event_mask( x_window_t *  win , long  event_mask) ;

int  x_window_add_event_mask( x_window_t *  win , long  event_mask) ;

int  x_window_remove_event_mask( x_window_t *  win , long  event_mask) ;

int  x_window_show( x_window_t *  win , int  hint) ;

int  x_window_map( x_window_t *  win) ;

int  x_window_unmap( x_window_t *  win) ;

int  x_window_resize( x_window_t *  win , u_int  width , u_int  height , x_event_dispatch_t  flag) ;

int  x_window_remaximize( x_window_t *  win) ;

int  x_window_resize_with_margin( x_window_t *  win , u_int  width , u_int  height ,
	x_event_dispatch_t  flag) ;

int  x_window_set_normal_hints( x_window_t *  win , u_int  min_width , u_int  min_height ,
	u_int  width_inc , u_int  height_inc) ;

int  x_window_move( x_window_t *  win , int  x , int  y) ;

int  x_window_clear( x_window_t *  win , int  x , int  y , u_int  width , u_int  height) ;

int  x_window_clear_margin_area( x_window_t *  win) ;

int  x_window_clear_all( x_window_t *  win) ;

int  x_window_fill( x_window_t *  win , int  x , int  y , u_int  width , u_int  height) ;

int  x_window_fill_all( x_window_t *  win) ;

int  x_window_update_view( x_window_t *  win , int  x , int  y , u_int  width , u_int  height) ;

int  x_window_update_view_all( x_window_t *  win) ;

void  x_window_idling( x_window_t *  win) ;

int  x_window_receive_event( x_window_t *  win , XEvent *  event) ;

size_t  x_window_get_str( x_window_t *  win , u_char *  seq , size_t  seq_len , mkf_parser_t **  parser ,
	KeySym *  keysym , XKeyEvent *  event) ;

int  x_window_is_scrollable( x_window_t *  win) ;

int  x_window_scroll_upward( x_window_t *  win , u_int  height) ;

int  x_window_scroll_upward_region( x_window_t *  win , int  start_y , int  end_y , u_int  height) ;

int  x_window_scroll_downward( x_window_t *  win , u_int  height) ;

int  x_window_scroll_downward_region( x_window_t *  win , int  start_y , int  end_y , u_int  height) ;

int  x_window_draw_decsp_string( x_window_t *  win , x_font_t *  font ,
	x_color_t *  fg_color , x_color_t *  bg_color , int  x , int  y ,
	u_char *  str , u_int  len) ;

int  x_window_draw_string( x_window_t *  win , x_font_t *  font , x_color_t *  fg_color ,
	int  x , int  y , u_char *  str , u_int  len) ;

int  x_window_draw_string16( x_window_t *  win , x_font_t *  font , x_color_t *  fg_color ,
	int  x , int  y , XChar2b *  str , u_int  len) ;

int  x_window_draw_image_string( x_window_t *  win , x_font_t *  font ,
	x_color_t *  fg_color , x_color_t *  bg_color , int  x , int  y ,
	u_char *  str , u_int  len) ;

int  x_window_draw_image_string16( x_window_t *  win , x_font_t *  font ,
	x_color_t *  fg_color , x_color_t *  bg_color , int  x , int  y ,
	XChar2b *  str , u_int  len) ;

#ifdef  ANTI_ALIAS
int  x_window_xft_draw_string8( x_window_t *  win , x_font_t *  font ,
	x_color_t *  fg_color , int  x , int  y , u_char *  str , size_t  len) ;

int  x_window_xft_draw_string32( x_window_t *  win , x_font_t *  font ,
	x_color_t *  fg_color , int  x , int  y , XftChar32 *  str , u_int  len) ;
#endif

int  x_window_draw_rect_frame( x_window_t *  win , int  x1 , int  y1 , int  x2 , int  y2) ;

int  x_window_set_selection_owner( x_window_t *  win , Time  time) ;
	
int  x_window_string_selection_request( x_window_t *  win , Time  time) ;

int  x_window_xct_selection_request( x_window_t *  win , Time  time) ;

int  x_window_utf8_selection_request( x_window_t *  win , Time  time) ;

int  x_window_send_selection( x_window_t *  win , XSelectionRequestEvent *  event ,
	u_char *  sel_str , size_t  sel_len , Atom  sel_type) ;

int  x_set_window_name( x_window_t *  win , u_char *  name) ;

int  x_set_icon_name( x_window_t *  win , u_char *  name) ;

int  x_window_set_icon( x_window_t *  win, char * file_path) ;

int  x_window_get_visible_geometry( x_window_t *  win ,
	int *  x , int *  y , int *  my_x , int *  my_y , u_int *  width , u_int *  height) ;

#if  0
/* not used */
int  x_window_paste( x_window_t *  win , Drawable  src , int  src_x , int  src_y ,
	u_int  src_width , u_int  src_height , int  dst_x , int  dst_y) ;
#endif

#ifdef  DEBUG
void  x_window_dump_children( x_window_t *  win) ;
#endif


#endif
