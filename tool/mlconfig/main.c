/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <gtk/gtk.h>
#include  <glib.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_str.h>

#include  "mc_char_encoding.h"
#include  "mc_color.h"
#include  "mc_brightness.h"
#include  "mc_fade.h"
#include  "mc_tabsize.h"
#include  "mc_logsize.h"
#include  "mc_fontsize.h"
#include  "mc_line_space.h"
#include  "mc_screen_ratio.h"
#include  "mc_mod_meta.h"
#include  "mc_bel.h"
#include  "mc_font_present.h"
#include  "mc_xim.h"
#include  "mc_check.h"
#include  "mc_iscii_lang.h"
#include  "mc_vertical.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static FILE *  out ;

static GtkWidget *  is_comb_check ;
static GtkWidget *  use_bidi_check ;
static GtkWidget *  copy_paste_via_ucs_check ;
static GtkWidget *  is_tp_check ;


/* --- static functions --- */

static gint
end_application(
	GtkWidget *  widget ,
	GdkEvent *  event ,
	gpointer  data
	)
{
	gtk_main_quit() ;

	return  FALSE ;
}

static gint
cancel_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	gtk_main_quit() ;

	return  FALSE ;
}

static gint
apply_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	/*
	 * CONFIG:[encoding] [iscii lang] [fg color] [bg color] [tabsize] [logsize] [fontsize] \
	 * [screen width ratio] [screen height ratio] [mod meta mode] [bel mode] [vertical mode] \
	 * [combining char] [copy paste via ucs] [is transparent] [brightness] [fade ratio] \
	 * [font present] [is bidi] [xim] [locale][LF]
	 */
	fprintf( out , "CONFIG:%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %s %s\n" ,
		mc_get_char_encoding() ,
		mc_get_iscii_lang() ,
		mc_get_fg_color() ,
		mc_get_bg_color() ,
		mc_get_tabsize() ,
		mc_get_logsize() ,
		mc_get_fontsize() ,
		mc_get_line_space() ,
		mc_get_screen_width_ratio() ,
		mc_get_screen_height_ratio() ,
		mc_get_mod_meta_mode() ,
		mc_get_bel_mode() ,
		mc_get_vertical_mode() ,
		GTK_TOGGLE_BUTTON(is_comb_check)->active ,
		GTK_TOGGLE_BUTTON(copy_paste_via_ucs_check)->active ,
		GTK_TOGGLE_BUTTON(is_tp_check)->active ,
		mc_get_brightness() ,
		mc_get_fade_ratio() ,
		mc_get_font_present() ,
		GTK_TOGGLE_BUTTON(use_bidi_check)->active ,
		mc_get_xim_name() ,
		mc_get_xim_locale()) ;

	gtk_main_quit() ;
	
	return  1 ;
}

static gint
larger_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	fprintf( out , "FONT:larger\n") ;

	gtk_main_quit() ;
	
	return  FALSE ;
}

static gint
smaller_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	fprintf( out , "FONT:smaller\n") ;
	
	gtk_main_quit() ;
	
	return  FALSE ;
}

static gint
file_sel_ok_clicked(
	GtkObject *  object
	)
{
	char *  file_path ;

	file_path = gtk_file_selection_get_filename( GTK_FILE_SELECTION( object ) ) ;

	if( strcmp( file_path , "") == 0)
	{
		return  TRUE ;
	}
	else
	{
		fprintf( out , "WALL_PIC:%s\n" , file_path) ;

		gtk_main_quit() ;

		return FALSE ;
	}
}

static gint
file_sel_cancel_clicked(
	GtkObject *  object
	)
{
	gtk_widget_destroy( GTK_WIDGET(object)) ;
	
	return  FALSE ;
}

static gint
wall_pic_clicked(
	GtkObject *  object
	)
{
	GtkWidget *  file_sel ;

	file_sel = gtk_file_selection_new( "Wall Picture") ;
	gtk_widget_show( GTK_WIDGET(file_sel)) ;

	gtk_signal_connect_object( GTK_OBJECT( GTK_FILE_SELECTION(file_sel)->ok_button ) ,
		"clicked" , GTK_SIGNAL_FUNC(file_sel_ok_clicked) , GTK_OBJECT(file_sel)) ;

	gtk_signal_connect_object( GTK_OBJECT( GTK_FILE_SELECTION(file_sel)->cancel_button ) ,
		"clicked" , GTK_SIGNAL_FUNC(file_sel_cancel_clicked) , GTK_OBJECT(file_sel)) ;
		
	return  TRUE ;
}

static gint
no_wall_pic_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	fprintf( out , "WALL_PIC:off\n") ;

	gtk_main_quit() ;

	return  FALSE ;
}

static gint
full_reset_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	fprintf( out , "FULL_RESET\n") ;

	gtk_main_quit() ;

	return  FALSE ;
}

static GtkWidget *
apply_cancel_button(
	void
	)
{
	GtkWidget * button ;
	GtkWidget * hbox ;

	hbox = gtk_hbox_new(FALSE , 5) ;
	gtk_widget_show(hbox) ;

	button = gtk_button_new_with_label("Apply") ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(apply_clicked) , NULL) ;
	gtk_widget_show(button) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 5) ;

	button = gtk_button_new_with_label("Cancel") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(cancel_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 5) ;


	return hbox;
}

static GtkWidget *
font_large_small(void)
{
	GtkWidget * frame;
	GtkWidget * hbox;
	GtkWidget * button;

	frame = gtk_frame_new("Font size") ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	button = gtk_button_new_with_label("Larger") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(larger_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	button = gtk_button_new_with_label("Smaller") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(smaller_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	return frame;
}

static GtkWidget *
wall_picture(
	GtkWidget * window
	)
{
	GtkWidget * frame;
	GtkWidget * hbox;
	GtkWidget * button;

	frame = gtk_frame_new("Wall picture") ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	button = gtk_button_new_with_label( "Select") ;
	gtk_widget_show(button) ;
	gtk_signal_connect_object(GTK_OBJECT(button) ,
		"clicked" , GTK_SIGNAL_FUNC(wall_pic_clicked) , GTK_OBJECT(window)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	button = gtk_button_new_with_label( "Off") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(no_wall_pic_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	return frame;
}

static GtkWidget *
full_reset(void)
{
	GtkWidget *  frame ;
	GtkWidget *  hbox ;
	GtkWidget *  button ;

	frame = gtk_frame_new("Full reset") ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	button = gtk_button_new_with_label("Full reset") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(full_reset_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;
	
	return frame;
}

static int
show(
	int  x ,
	int  y ,
	ml_char_encoding_t  encoding ,
	ml_iscii_lang_t  iscii_lang ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	char *  tabsize ,
	char *  logsize ,
	char *  fontsize ,
	u_int  min_fontsize ,
	u_int  max_fontsize ,
	char *  line_space ,
	char *  screen_width_ratio ,
	char *  screen_height_ratio ,
	ml_mod_meta_mode_t  mod_meta_mode ,
	ml_bel_mode_t  bel_mode ,
	ml_vertical_mode_t  vertical_mode ,
	int  is_combining_char ,
	int  copy_paste_via_ucs ,
	int  is_transparent ,
	char *  brightness ,
	char *  fade_ratio ,
	ml_font_present_t  font_present ,
	int  use_bidi ,
	char *  xim ,
	char *  locale
	)
{
	GtkWidget *  window ;
	GtkWidget *  vbox ;
	GtkWidget *  hbox ;
	GtkWidget *  notebook ;
	GtkWidget *  frame ;
	GtkWidget *  label ;
	GtkWidget *  config_widget ;
	GtkWidget *  separator ;
	
	window = gtk_window_new( GTK_WINDOW_TOPLEVEL) ;
	gtk_signal_connect( GTK_OBJECT(window) , "delete_event" ,
		GTK_SIGNAL_FUNC(end_application) , NULL) ;
	gtk_window_set_title( GTK_WINDOW(window) , "mlterm configuration") ;
	gtk_container_set_border_width( GTK_CONTAINER(window) , 0) ;
	gtk_widget_show( window) ;
	gdk_window_move( window->window , x , y) ;
	gtk_window_set_policy( GTK_WINDOW(window) , 0 , 0 , 0) ;

	vbox = gtk_vbox_new( FALSE , 10) ;
	gtk_widget_show( vbox) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_container_add( GTK_CONTAINER(window) , vbox) ;


	/* whole screen (except for the contents of notebook) */

	notebook = gtk_notebook_new() ;
	gtk_notebook_set_tab_pos( GTK_NOTEBOOK(notebook) , GTK_POS_TOP) ;
	gtk_widget_show( notebook) ;
	gtk_box_pack_start( GTK_BOX(vbox) , notebook , TRUE , TRUE , 0) ;

	separator = gtk_hseparator_new() ;
	gtk_widget_show( separator) ;
	gtk_box_pack_start( GTK_BOX(vbox) , separator , FALSE , FALSE , 2) ;

	hbox = apply_cancel_button();
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	
	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	frame = font_large_small();
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;
	frame = wall_picture( window);
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;
	frame = full_reset();
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;

	/* contents of the "Encoding" tab */

	label = gtk_label_new( "Encoding") ;
	gtk_widget_show( label) ;

	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;

	if( ! ( config_widget = mc_char_encoding_config_widget_new( encoding)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_iscii_lang_config_widget_new( iscii_lang)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_xim_config_widget_new( xim, locale)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_vertical_config_widget_new( vertical_mode)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	hbox = gtk_hbox_new( TRUE , 5) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;	

	if( ! ( use_bidi_check = mc_check_config_widget_new( "Bidi (UTF8 only)" , use_bidi)))
	{
		return  0 ;
	}
	gtk_widget_show( use_bidi_check) ;
	gtk_box_pack_start( GTK_BOX(hbox) , use_bidi_check , TRUE , TRUE , 0) ;
	
	if( ! ( is_comb_check = mc_check_config_widget_new( "Combining" , is_combining_char)))
	{
		return  0 ;
	}
	gtk_widget_show( is_comb_check) ;
	gtk_box_pack_start( GTK_BOX(hbox) , is_comb_check , TRUE , TRUE , 0) ;

	hbox = gtk_hbox_new( TRUE , 5) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;	


	/* contents of the "Copy&paste" tab */

	label = gtk_label_new( "Copy&paste") ;
	gtk_widget_show( label) ;
	
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;

	frame = gtk_frame_new( "For unicode-subset encodings") ;
	gtk_widget_show( frame) ;
	gtk_box_pack_start( GTK_BOX(vbox) , frame , FALSE , FALSE , 0) ;
	
	if( ! ( copy_paste_via_ucs_check = mc_check_config_widget_new(
		"Process received strings via Unicode" , copy_paste_via_ucs)))
	{
		return  0 ;
	}
	gtk_widget_show( copy_paste_via_ucs_check) ;
	gtk_container_add( GTK_CONTAINER(frame) , copy_paste_via_ucs_check) ;
		

	/* contents of the "Font" tab */
	
	label = gtk_label_new( "Font") ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;

	if( ! ( config_widget = mc_fontsize_config_widget_new( fontsize , min_fontsize , max_fontsize)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_font_present_config_widget_new( font_present)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	
	/* contents of the "Appearance" tab */

	label = gtk_label_new( "Appearance") ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;

	if ( ! ( config_widget = mc_fg_color_config_widget_new( fg_color)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_bg_color_config_widget_new( bg_color)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_line_space_config_widget_new( line_space)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_brightness_config_widget_new( brightness)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_fade_config_widget_new( fade_ratio)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	
	hbox = gtk_hbox_new( TRUE , 5) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

	if( ! ( is_tp_check = mc_check_config_widget_new( "Transparent" , is_transparent)))
	{
		return  0 ;
	}
	gtk_widget_show( is_tp_check) ;
	gtk_box_pack_start( GTK_BOX(hbox) , is_tp_check , TRUE , TRUE , 0) ;


	/* contents of the "Others" tab */

	label = gtk_label_new( "Others") ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;

	if( ! ( config_widget = mc_screen_width_ratio_config_widget_new( screen_width_ratio)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_screen_height_ratio_config_widget_new( screen_height_ratio)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_tabsize_config_widget_new( tabsize)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! (config_widget = mc_logsize_config_widget_new( logsize)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! (config_widget = mc_mod_meta_config_widget_new( mod_meta_mode)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_bel_config_widget_new( bel_mode)))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	gtk_main() ;

	return  1 ;
}

static int
start_application(
	int  x ,
	int  y ,
	int  in_fd ,
	int  out_fd
	)
{
	FILE *  in ;
	kik_file_t *  kin ;
	char *  p ;
	size_t  len ;
	char *  input_line ;
	
	int  encoding ;
	int  iscii_lang ;
	int  fg_color ;
	int  bg_color ;
	char *  tabsize ;
	char *  logsize ;
	char *  fontsize ;
	u_int  min_fontsize ;
	u_int  max_fontsize ;
	char *  line_space ;
	char *  screen_width_ratio ;
	char *  screen_height_ratio ;
	int  mod_meta_mode ;
	int  bel_mode ;
	int  vertical_mode ;
	int  is_combining_char ;
	int  copy_paste_via_ucs ;
	int  is_transparent ;
	char *  brightness ;
	char *  fade_ratio ;
	int  font_present ;
	int  use_bidi ;
	char *  locale ;
	char *  xim ;

	if( ( in = fdopen( in_fd , "r")) == NULL)
	{
		return  0 ;
	}
	
	if( ( out = fdopen( out_fd , "w")) == NULL)
	{
		return  0 ;
	}

	if( ( kin = kik_file_new( in)) == NULL)
	{
		return  0 ;
	}
	
	if( ( p = kik_file_get_line( kin , &len)) == NULL || len == 0)
	{
		return  0 ;
	}
	
	p[len - 1] = '\0' ;

	input_line = strdup( p) ;
	
	kik_file_delete( kin) ;
	fclose( in) ;

	/*
	 * [encoding] [iscii lang] [fg color] [bg color] [tabsize] [logsize] [fontsize] \
	 * [min font size] [max font size] [line space] [mod meta mode] [bel mode] [vertical mode] \
	 * [combining char] [copy paste via ucs] [is transparent] [font present] [is bidi] \
	 * [xim] [locale][LF]
	 */
	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &encoding , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &iscii_lang , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &fg_color , p))
	{
		return  0 ;
	}
	
	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &bg_color , p))
	{
		return  0 ;
	}
	
	if( ( tabsize = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}

	if( ( logsize = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}

	if( ( fontsize = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_uint( &min_fontsize , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_uint( &max_fontsize , p))
	{
		return  0 ;
	}
	
	if( ( line_space = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}
	
	if( ( screen_width_ratio = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}
	
	if( ( screen_height_ratio = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}
	
	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &mod_meta_mode , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &bel_mode , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &vertical_mode , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &is_combining_char , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &copy_paste_via_ucs , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &is_transparent , p))
	{
		return  0 ;
	}

	if( ( brightness = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}
	
	if( ( fade_ratio = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}
	
	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &font_present , p))
	{
		return  0 ;
	}
	
	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &use_bidi , p))
	{
		return  0 ;
	}
	
	if( ( xim = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}
	
	if( ( locale = input_line) == NULL)
	{
		return  0 ;
	}
	
	return  show( x , y , encoding , iscii_lang , fg_color , bg_color , tabsize ,
		logsize , fontsize , min_fontsize , max_fontsize , line_space ,
		screen_width_ratio , screen_height_ratio , mod_meta_mode , bel_mode , vertical_mode ,
		is_combining_char , copy_paste_via_ucs , is_transparent , brightness , fade_ratio ,
		font_present , use_bidi , xim , locale) ;
}


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
	int  x ;
	int  y ;
	int  in_fd ;
	int  out_fd ;
	
	gtk_init(&argc , &argv) ;

	if( argc != 5 ||
		! kik_str_to_int( &x , argv[1]) ||
		! kik_str_to_int( &y , argv[2]) ||
		! kik_str_to_int( &in_fd , argv[3]) ||
		! kik_str_to_int( &out_fd , argv[4]))
	{
		kik_msg_printf( "usage: (stdin 24) mlconfig [x] [y] [in] [out]\n") ;
		
		return  0 ;
	}

	if( start_application( x , y , in_fd , out_fd) == 0)
	{
		kik_msg_printf( "illegal command format.\n") ;
		
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}
