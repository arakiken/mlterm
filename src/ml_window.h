/*
 *	$Id$
 */

#ifndef  __ML_WINDOW_H__
#define  __ML_WINDOW_H__


#include  <X11/Xlib.h>
#include  <X11/cursorfont.h>	/* for cursor shape */

#ifdef  ANTI_ALIAS
#include  <X11/Xft/Xft.h>
#endif

#include  <kiklib/kik_types.h>
#include  <mkf/mkf_parser.h>

#include  "ml_font.h"
#include  "ml_color_manager.h"
#include  "ml_char.h"


#define  ACTUAL_WIDTH(win)  ((win)->width + (win)->margin * 2)
#define  ACTUAL_HEIGHT(win)  ((win)->height + (win)->margin * 2)


typedef enum  ml_event_dispatch
{
	NOTIFY_TO_CHILDREN = 0x01 ,
	NOTIFY_TO_PARENT = 0x02 ,
	NOTIFY_TO_MYSELF = 0x04 ,

} ml_event_dispatch_t ;

typedef struct  ml_xim_event_listener
{
	void *  self ;

	int  (*get_spot)( void *  , int * , int *) ;
	XFontSet  (*get_fontset)( void *) ;

} ml_xim_event_listener_t ;

typedef struct ml_window_manager *  ml_window_manager_ptr_t ;

typedef struct ml_xic *  ml_xic_ptr_t ;

typedef struct  ml_window
{
	Display *  display ;
	int  screen ;

	Window  parent_window ;
	Window  my_window ;

	Drawable  drawable ;

	Pixmap  pixmap ;
	int  use_pixmap ;

#ifdef  ANTI_ALIAS
	XftDraw *  xft_draw ;
#endif

	GC  gc ;

	struct ml_window *  parent ;

	ml_window_manager_ptr_t  win_man ;
	
	struct
	{
		struct ml_window *  window ;
		int  x ;
		int  y ;
		
	} *  children ;

	u_int  num_of_children ;
	
	int  wall_picture_is_set ;
	int  is_transparent ;

	u_int  cursor_shape ;

	long  event_mask ;

	int  is_scrollable ;

	ml_font_t *  font ;

	u_int  width ;
	u_int  height ;
	u_int  min_width ;
	u_int  min_height ;
	u_int  width_inc ;
	u_int  height_inc ;
	
	/* actual window size is +margin on north/south/east/west */
	u_int  margin ;

	ml_color_table_t  color_table ;

	/* used by ml_xim */
	int  use_xim ;
	ml_xic_ptr_t  xic ;
	ml_xim_event_listener_t *  xim_listener ;

	Time  prev_clicked_time ;
	int  prev_clicked_button ;
	int  click_num ;
	int  button_is_pressing ;
	XButtonEvent  prev_button_press_event ;

	void (*window_realized)( struct ml_window *) ;
	void (*window_finalized)( struct ml_window *) ;
	void (*window_exposed)( struct ml_window * , int , int , u_int , u_int) ;
	void (*key_pressed)( struct ml_window * , XKeyEvent *) ;
	void (*button_motion)( struct ml_window * , XMotionEvent *) ;
	void (*button_released)( struct ml_window * , XButtonEvent *) ;
	void (*button_pressed)( struct ml_window * , XButtonEvent * , int) ;
	void (*button_press_continued)( struct ml_window * , XButtonEvent *) ;
	void (*window_resized)( struct ml_window *) ;
	void (*child_window_resized)( struct ml_window * , struct ml_window *) ;
	void (*selection_cleared)( struct ml_window * , XSelectionClearEvent *) ;
	void (*string_selection_requested)( struct ml_window * , XSelectionRequestEvent * , Atom) ;
	void (*text_selection_requested)( struct ml_window * , XSelectionRequestEvent * , Atom) ;
	void (*xct_selection_requested)( struct ml_window * , XSelectionRequestEvent * , Atom) ;
	void (*utf8_selection_requested)( struct ml_window * , XSelectionRequestEvent * , Atom) ;
	void (*xct_selection_notified)( struct ml_window * , u_char * , size_t) ;
	void (*utf8_selection_notified)( struct ml_window * , u_char * , size_t) ;
	void (*selection_request_failed)( struct ml_window * , XSelectionEvent *) ;
	void (*window_deleted)( struct ml_window *) ;
	
} ml_window_t ;


int  ml_window_init( ml_window_t *  win , ml_color_table_t  color_table ,
	u_int  width , u_int  height , u_int  min_width , u_int  min_height ,
	u_int  width_inc , u_int  height_inc , u_int  margin) ;

int  ml_window_init_atom( Display *  display) ;

int  ml_window_final( ml_window_t *  win) ;

int  ml_window_set_wall_picture( ml_window_t *  win , Pixmap  pic) ;

int  ml_window_unset_wall_picture( ml_window_t *  win) ;

int  ml_window_set_transparent( ml_window_t *  win) ;

int  ml_window_unset_transparent( ml_window_t *  win) ;

int  ml_window_use_pixmap( ml_window_t *  win) ;

int  ml_window_set_cursor( ml_window_t *  win , u_int  cursor_shape) ;

int  ml_window_set_fg_color( ml_window_t *  win , ml_color_t  fg_color) ;

int  ml_window_set_bg_color( ml_window_t *  win , ml_color_t  bg_color) ;

int  ml_window_get_fg_color( ml_window_t *  win) ;

int  ml_window_get_bg_color( ml_window_t *  win) ;

int  ml_window_add_child( ml_window_t *  win , ml_window_t *  child , int  x , int  y) ;

ml_window_t *  ml_get_root_window( ml_window_t *  win) ;

int  ml_window_init_event_mask( ml_window_t *  win , long  event_mask) ;

int  ml_window_add_event_mask( ml_window_t *  win , long  event_mask) ;

int  ml_window_remove_event_mask( ml_window_t *  win , long  event_mask) ;

int  ml_window_show( ml_window_t *  win , int  x , int  y) ;

int  ml_window_reset_font( ml_window_t *  win) ;

int  ml_window_resize( ml_window_t *  win , u_int  width , u_int  height , ml_event_dispatch_t  flag) ;

int  ml_window_set_normal_hints( ml_window_t *  win , u_int  width_inc , u_int  height_inc ,
	u_int  min_width , u_int  min_height) ;

void  ml_window_clear( ml_window_t *  win , int  x , int  y , u_int  width , u_int  height) ;

void  ml_window_clear_all( ml_window_t *  win) ;

void  ml_window_fill( ml_window_t *  win , int  x , int  y , u_int  width , u_int  height) ;

void  ml_window_fill_all( ml_window_t *  win) ;

int  ml_window_update_view( ml_window_t *  win , int  x , int  y , u_int  width , u_int  height) ;

int  ml_window_update_view_all( ml_window_t *  win) ;

void  ml_window_idling( ml_window_t *  win) ;

int  ml_window_receive_event( ml_window_t *  win , XEvent *  event) ;

size_t  ml_window_get_str( ml_window_t *  win , u_char *  seq , size_t  seq_len , mkf_parser_t **  parser ,
	KeySym *  keysym , XKeyEvent *  event) ;

inline int  ml_window_is_scrollable( ml_window_t *  win) ;

int  ml_window_scroll_upward( ml_window_t *  win , u_int  height) ;

int  ml_window_scroll_upward_region( ml_window_t *  win , int  start_y , int  end_y , u_int  height) ;

int  ml_window_scroll_downward( ml_window_t *  win , u_int  height) ;

int  ml_window_scroll_downward_region( ml_window_t *  win , int  start_y , int  end_y , u_int  height) ;

int  ml_window_draw_str( ml_window_t *  win , ml_char_t *  chars , u_int  num_of_chars ,
	int  x , int  y , u_int  height , u_int  height_to_baseline) ;

int  ml_window_draw_cursor( ml_window_t *  win , ml_char_t *  ch ,
	int  x , int  y , u_int  height , u_int  height_to_baseline) ;
	
int  ml_window_draw_str_to_eol( ml_window_t *  win , ml_char_t *  chars , u_int  num_of_chars ,
	int  x , int  y , u_int  height , u_int  height_to_baseline) ;

int  ml_window_set_selection_owner( ml_window_t *  win , Time  time) ;
	
int  ml_window_xct_selection_request( ml_window_t *  win , Time  time) ;

int  ml_window_utf8_selection_request( ml_window_t *  win , Time  time) ;

int  ml_window_send_selection( ml_window_t *  win , XSelectionRequestEvent *  event ,
	u_char *  sel_str , size_t  sel_len , Atom  sel_type) ;

int  ml_set_window_name( ml_window_t *  win , u_char *  name) ;

int  ml_set_icon_name( ml_window_t *  win , u_char *  name) ;


#if  0
/* not used */
int  ml_window_paste( ml_window_t *  win , Drawable  src , int  src_x , int  src_y ,
	u_int  src_width , u_int  src_height , int  dst_x , int  dst_y) ;
#endif

#ifdef  DEBUG
void  ml_window_dump_children( ml_window_t *  win) ;
#endif


#endif
