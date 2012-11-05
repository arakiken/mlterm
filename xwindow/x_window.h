/*
 *	$Id$
 */

#ifndef  __X_WINDOW_H__
#define  __X_WINDOW_H__


#include  "x.h"

#include  <kiklib/kik_types.h>
#include  <mkf/mkf_parser.h>

#include  "x_display.h"
#include  "x_font.h"
#include  "x_color.h"
#include  "x_gc.h"


#define  ACTUAL_WIDTH(win)  ((win)->width + (win)->margin * 2)
#define  ACTUAL_HEIGHT(win)  ((win)->height + (win)->margin * 2)

/*
 * Don't use win->parent in xlib to check if win is root window or not
 * because mlterm can work as libvte.
 *   vte window
 *      |
 *   mlterm window ... x_window_t::parent == NULL
 *                     x_window_t::parent_window == vte window
 */
#define  PARENT_WINDOWID_IS_TOP(win)  ((win)->parent_window == (win)->disp->my_window)


typedef enum  x_resize_flag
{
	NOTIFY_TO_NONE = 0x0 ,
	NOTIFY_TO_CHILDREN = 0x01 ,
	NOTIFY_TO_PARENT = 0x02 ,
	NOTIFY_TO_MYSELF = 0x04 ,

	LIMIT_RESIZE = 0x08 ,

} x_resize_flag_t ;

typedef struct  x_xim_event_listener
{
	void *  self ;

	int  (*get_spot)( void *  , int * , int *) ;
	XFontSet  (*get_fontset)( void *) ;
	x_color_t *  (*get_fg_color)( void *) ;
	x_color_t *  (*get_bg_color)( void *) ;

} x_xim_event_listener_t ;

/* Defined in x_xic.h */
typedef struct x_xic *  x_xic_ptr_t ;

/* Defined in x_xim.h */
typedef struct x_xim *  x_xim_ptr_t ;

/* Defined in x_dnd.h */
typedef struct x_dnd_context *  x_dnd_context_ptr_t ;

/* Defined in x_picture.h */
typedef struct x_picture_modifier *  x_picture_modifier_ptr_t ;
typedef struct x_icon_picture *  x_icon_picture_ptr_t ;
typedef struct _XftDraw *  xft_draw_ptr_t ;
typedef struct _cairo *  cairo_ptr_t ;

typedef struct  x_window
{
	x_display_t *  disp ;
	
	Window  my_window ;

	/*
	 * Don't remove if USE_XFT and USE_CAIRO are not defined to keep the size of x_window_t
	 * for x_im_xxx_screen_t.
	 */
	xft_draw_ptr_t  xft_draw ;
	cairo_ptr_t  cairo_draw ;

	x_color_t  fg_color ;
	x_color_t  bg_color ;

	x_gc_t *  gc ;

	Window  parent_window ;		/* This member of root window is DefaultRootWindow */
	struct x_window *  parent ;	/* This member of root window is NULL */
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
	u_int  base_width ;
	u_int  base_height ;
	u_int  width_inc ;
	u_int  height_inc ;

	/* actual window size is +margin on north/south/east/west */
	u_int  margin ;

	/* used by x_xim */
	x_xim_ptr_t  xim ;
	x_xim_event_listener_t *  xim_listener ;
	x_xic_ptr_t  xic ;	/* Only root window manages xic in win32 */

#ifdef  USE_WIN32GUI
	WORD  update_window_flag ;
	int  cmd_show ;
#endif

	/* button */
	Time  prev_clicked_time ;
	int  prev_clicked_button ;
	XButtonEvent  prev_button_press_event ;

	x_picture_modifier_ptr_t  pic_mod ;

	/*
	 * XDND
	 */
	/*
	 * Don't remove if DISABLE_XDND is defined to keep the size of x_window_t for
	 * x_im_xxx_screen_t.
	 */
	x_dnd_context_ptr_t  dnd ;

	/*
	 * XClassHint
	 */
	char *  app_name ;

	
	/*
	 * flags etc.
	 */

#if  defined(USE_WIN32GUI) || defined(USE_FRAMEBUFFER)
	Pixmap  wall_picture ;
#else
	int8_t  wall_picture_is_set ;	/* Actually set picture (including transparency) or not. */
	int8_t  wait_copy_area_response ;	/* Used for XCopyArea() */
#endif
	int8_t  is_sel_owner ;
	int8_t  is_transparent ;
	int8_t  is_scrollable ;
	int8_t  is_focused ;
	int8_t  is_mapped ;
	int8_t  create_gc ;

	/* button */
	int8_t  button_is_pressing ;
	int8_t  click_num ;

	void (*window_realized)( struct x_window *) ;
	void (*window_finalized)( struct x_window *) ;
	void (*window_deleted)( struct x_window *) ;
	void (*mapping_notify)( struct x_window *) ;
	/* Win32: gc->gc is not None. */
	void (*window_exposed)( struct x_window * , int , int , u_int , u_int) ;
	/* Win32: gc->gc is not None. */
	void (*update_window)( struct x_window * , int) ;
	void (*window_focused)( struct x_window *) ;
	void (*window_unfocused)( struct x_window *) ;
	void (*key_pressed)( struct x_window * , XKeyEvent *) ;
	void (*pointer_motion)( struct x_window * , XMotionEvent *) ;
	void (*button_motion)( struct x_window * , XMotionEvent *) ;
	void (*button_released)( struct x_window * , XButtonEvent *) ;
	void (*button_pressed)( struct x_window * , XButtonEvent * , int) ;
	void (*button_press_continued)( struct x_window * , XButtonEvent *) ;
	void (*window_resized)( struct x_window *) ;
	void (*child_window_resized)( struct x_window * , struct x_window *) ;
	void (*selection_cleared)( struct x_window *) ;
	void (*xct_selection_requested)( struct x_window * , XSelectionRequestEvent * , Atom) ;
	void (*utf_selection_requested)( struct x_window * , XSelectionRequestEvent * , Atom) ;
	void (*xct_selection_notified)( struct x_window * , u_char * , size_t) ;
	void (*utf_selection_notified)( struct x_window * , u_char * , size_t) ;
	/*
	 * Don't remove if DISABLE_XDND is defined to keep the size of x_window_t
	 * for x_im_xxx_screen_t.
	 */
	void (*set_xdnd_config)( struct x_window * , char * ,  char * , char * ) ;
	void (*idling)( struct x_window *) ;

} x_window_t ;


int  x_window_init( x_window_t *  win ,
	u_int  width , u_int  height , u_int  min_width , u_int  min_height ,
	u_int  base_width , u_int  base_height , u_int  width_inc ,
	u_int  height_inc , u_int  margin , int  create_gc) ;

int  x_window_final( x_window_t *  win) ;

int  x_window_set_type_engine( x_window_t *  win , x_type_engine_t  type_engine) ;

int  x_window_init_event_mask( x_window_t *  win , long  event_mask) ;

int  x_window_add_event_mask( x_window_t *  win , long  event_mask) ;

int  x_window_remove_event_mask( x_window_t *  win , long  event_mask) ;

/* int  x_window_grab_pointer( x_window_t *  win) ; */

int  x_window_ungrab_pointer( x_window_t *  win) ;

int  x_window_set_wall_picture( x_window_t *  win , Pixmap  pic) ;

int  x_window_unset_wall_picture( x_window_t *  win) ;

#if  defined(USE_WIN32GUI) || defined(USE_FRAMEBUFFER)
#define  x_window_has_wall_picture( win)  ((win)->wall_picture != None)
#else
#define  x_window_has_wall_picture( win)  ((win)->wall_picture_is_set)
#endif

int  x_window_set_transparent( x_window_t *  win , x_picture_modifier_ptr_t  pic_mod) ;

int  x_window_unset_transparent( x_window_t *  win) ;

int  x_window_set_cursor( x_window_t *  win , u_int  cursor_shape) ;

int  x_window_set_fg_color( x_window_t *  win , x_color_t *  fg_color) ;

int  x_window_set_bg_color( x_window_t *  win , x_color_t *  bg_color) ;

int  x_window_add_child( x_window_t *  win , x_window_t *  child , int  x , int  y , int  map) ;

x_window_t *  x_get_root_window( x_window_t *  win) ;

GC  x_window_get_fg_gc( x_window_t *  win) ;

GC  x_window_get_bg_gc( x_window_t *  win) ;

int  x_window_show( x_window_t *  win , int  hint) ;

int  x_window_map( x_window_t *  win) ;

int  x_window_unmap( x_window_t *  win) ;

int  x_window_resize( x_window_t *  win , u_int  width , u_int  height , x_resize_flag_t  flag) ;

int  x_window_resize_with_margin( x_window_t *  win , u_int  width , u_int  height ,
	x_resize_flag_t  flag) ;

int  x_window_set_normal_hints( x_window_t *  win , u_int  min_width , u_int  min_height ,
	u_int  base_width , u_int  base_height , u_int  width_inc , u_int  height_inc) ;

int  x_window_set_override_redirect( x_window_t *  win , int  flag) ;

int  x_window_set_borderless_flag( x_window_t *  win , int  flag) ;

int  x_window_move( x_window_t *  win , int  x , int  y) ;

int  x_window_clear( x_window_t *  win , int  x , int  y , u_int  width , u_int  height) ;

int  x_window_clear_all( x_window_t *  win) ;

int  x_window_fill( x_window_t *  win , int  x , int  y , u_int  width , u_int  height) ;

int  x_window_fill_with( x_window_t *  win , x_color_t *  color ,
	int  x , int  y , u_int  width , u_int  height) ;

int  x_window_blank( x_window_t *  win) ;

#if  0
/* Not used */
int  x_window_blank_with( x_window_t *  win , x_color_t *  color) ;
#endif

/* if flag is 0, no update. */
int  x_window_update( x_window_t *  win , int  flag) ;

int  x_window_update_all( x_window_t *  win) ;

void  x_window_idling( x_window_t *  win) ;

int  x_window_receive_event( x_window_t *  win , XEvent *  event) ;

size_t  x_window_get_str( x_window_t *  win , u_char *  seq , size_t  seq_len ,
	mkf_parser_t **  parser , KeySym *  keysym , XKeyEvent *  event) ;

#define  x_window_is_scrollable(win)  ((win)->is_scrollable)

int  x_window_scroll_upward( x_window_t *  win , u_int  height) ;

int  x_window_scroll_upward_region( x_window_t *  win ,
	int  boundary_start , int  boundary_end , u_int  height) ;

int  x_window_scroll_downward( x_window_t *  win , u_int  height) ;

int  x_window_scroll_downward_region( x_window_t *  win ,
	int  boundary_start , int  boundary_end , u_int  height) ;

int  x_window_scroll_leftward( x_window_t *  win , u_int  width) ;

int  x_window_scroll_leftward_region( x_window_t *  win ,
	int  boundary_start , int  boundary_end , u_int  width) ;

int  x_window_scroll_rightward_region( x_window_t *  win ,
	int  boundary_start , int  boundary_end , u_int  width) ;

int  x_window_scroll_rightward( x_window_t *  win , u_int  width) ;

int  x_window_copy_area( x_window_t *  win , Pixmap  src , int  src_x , int  src_y ,
	u_int  width , u_int  height , int  dst_x , int  dst_y) ;

int  x_window_draw_decsp_string( x_window_t *  win , x_font_t *  font ,
	x_color_t *  fg_color , int  x , int  y , u_char *  str , u_int  len) ;

int  x_window_draw_decsp_image_string( x_window_t *  win , x_font_t *  font ,
	x_color_t *  fg_color , x_color_t *  bg_color , int  x , int  y ,
	u_char *  str , u_int  len) ;

/*
 * x_window_draw_*_string functions are used by x_draw_str.[ch].
 * Use x_draw_str* functions usually.
 */

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
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
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
int  x_window_ft_draw_string8( x_window_t *  win , x_font_t *  font ,
	x_color_t *  fg_color , int  x , int  y , u_char *  str , size_t  len) ;

int  x_window_ft_draw_string32( x_window_t *  win , x_font_t *  font ,
	x_color_t *  fg_color , int  x , int  y , /* FcChar32 */ u_int32_t *  str , u_int  len) ;
#endif

int  x_window_draw_rect_frame( x_window_t *  win , int  x1 , int  y1 , int  x2 , int  y2) ;

int  x_window_draw_line( x_window_t *  win, int  x1, int  y1, int  x2, int  y2) ;

int  x_set_use_clipboard_selection( int  use_it) ;

int  x_is_using_clipboard_selection( void) ;

int  x_window_set_selection_owner( x_window_t *  win , Time  time) ;

int  x_window_string_selection_request( x_window_t *  win , Time  time) ;

int  x_window_xct_selection_request( x_window_t *  win , Time  time) ;

int  x_window_utf_selection_request( x_window_t *  win , Time  time) ;

int  x_window_send_selection( x_window_t *  win , XSelectionRequestEvent *  event ,
	u_char *  sel_data , size_t  sel_len , Atom  sel_type , int sel_format) ;

int  x_set_window_name( x_window_t *  win , u_char *  name) ;

int  x_set_icon_name( x_window_t *  win , u_char *  name) ;

int  x_window_set_icon( x_window_t *  win , x_icon_picture_ptr_t  icon) ;

int  x_window_remove_icon( x_window_t *  win) ;

int  x_window_reset_group( x_window_t *  win) ;

int  x_window_get_visible_geometry( x_window_t *  win ,
	int *  x , int *  y , int *  my_x , int *  my_y , u_int *  width , u_int *  height) ;

int  x_set_click_interval( int  interval) ;

#define  x_window_get_modifier_mapping( win) \
	x_display_get_modifier_mapping( (win)->disp)

u_int  x_window_get_mod_ignore_mask( x_window_t *  win , KeySym *  keysyms) ;

u_int  x_window_get_mod_meta_mask( x_window_t *  win , char *  mod_key) ;

int  x_window_bell( x_window_t *  win , int  visual) ;

int  x_window_translate_coordinates( x_window_t *  win, int x, int y,
	int *  global_x, int *  global_y, Window *  child) ;

#ifdef  DEBUG
void  x_window_dump_children( x_window_t *  win) ;
#endif


#endif
