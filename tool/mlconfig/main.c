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

#include  "mc_encoding.h"
#include  "mc_fg_color.h"
#include  "mc_bg_color.h"
#include  "mc_tabsize.h"
#include  "mc_logsize.h"
#include  "mc_fontsize.h"
#include  "mc_mod_meta.h"
#include  "mc_bel.h"
#include  "mc_xim.h"
#include  "mc_utf8_sel.h"
#include  "mc_xct_proc.h"
#include  "mc_check.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static FILE *  out ;

static GtkWidget *  is_comb_check ;
static GtkWidget *  use_bidi_check ;

static GtkWidget *  is_tp_check ;
static GtkWidget *  is_aa_check ;


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
apply_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	/*
	 * CONFIG:[encoding] [fg color] [bg color] [tabsize] [logsize] [fontsize] [mod meta mode] \
	 * [bel mode] [combining char] [prefer utf8 selection request] [pre conv xct to ucs] \
	 * [is transparent] [is aa] [is bidi] [xim] [locale][LF]
	 */
	fprintf( out , "CONFIG:%d %d %d %d %d %s %d %d %d %d %d %d %d %d %s %s\n" ,
		mc_get_encoding() ,
		mc_get_fg_color() ,
		mc_get_bg_color() ,
		mc_get_tabsize() ,
		mc_get_logsize() ,
		mc_get_fontsize() ,
		mc_get_mod_meta_mode() ,
		mc_get_bel_mode() ,
		GTK_TOGGLE_BUTTON(is_comb_check)->active ,
		mc_get_utf8_sel_mode() ,
		mc_get_xct_proc_mode() ,
		GTK_TOGGLE_BUTTON(is_tp_check)->active ,
		GTK_TOGGLE_BUTTON(is_aa_check)->active ,
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

static GtkWidget *
apply_cancel_button(
	void
	)
{
	GtkWidget * button ;
	GtkWidget * hbox ;

	hbox = gtk_hbox_new(FALSE , 5) ;
	gtk_widget_show(hbox) ;

	button = gtk_button_new_with_label("apply") ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(apply_clicked) , NULL) ;
	gtk_widget_show(button) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 5) ;

	button = gtk_button_new_with_label("cancel") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(end_application) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 5) ;


	return hbox;
}

static GtkWidget *
font_large_small(
	void
	)
{
	GtkWidget * frame;
	GtkWidget * hbox;
	GtkWidget * button;

	frame = gtk_frame_new("Font Size") ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	button = gtk_button_new_with_label("larger") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(larger_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	button = gtk_button_new_with_label("smaller") ;
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

	frame = gtk_frame_new("Wall Picture") ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	button = gtk_button_new_with_label( "select") ;
	gtk_widget_show(button) ;
	gtk_signal_connect_object(GTK_OBJECT(button) ,
		"clicked" , GTK_SIGNAL_FUNC(wall_pic_clicked) , GTK_OBJECT(window)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	button = gtk_button_new_with_label( "off") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(no_wall_pic_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	return frame;
}

static int
show(
	int  x ,
	int  y ,
	ml_char_encoding_t  encoding ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	char *  tabsize ,
	char *  logsize ,
	char *  fontsize ,
	u_int  min_fontsize ,
	u_int  max_fontsize ,
	ml_mod_meta_mode_t  mod_meta_mode ,
	ml_bel_mode_t  bel_mode ,
	int  is_combining_char ,
	int  prefer_utf8_selection ,
	ml_xct_proc_mode_t  xct_proc_mode ,
	int  is_transparent ,
	int  is_aa ,
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
	gtk_signal_connect(GTK_OBJECT(window) , "delete_event" ,
		GTK_SIGNAL_FUNC(end_application) , NULL) ;
	gtk_window_set_title(GTK_WINDOW(window) , "mlterm configuration") ;
	gtk_container_set_border_width(GTK_CONTAINER(window) , 0) ;
	gtk_widget_show(window) ;
	gdk_window_move(window->window , x , y) ;
	gtk_window_set_policy(GTK_WINDOW(window) , 0 , 0 , 0) ;

	vbox = gtk_vbox_new( FALSE , 10) ;
	gtk_widget_show(vbox) ;
	gtk_container_set_border_width(GTK_CONTAINER(vbox) , 5) ;
	gtk_container_add(GTK_CONTAINER(window) , vbox) ;


	/* whole screen (except for the contents of notebook) */

	notebook = gtk_notebook_new() ;
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook) , GTK_POS_TOP) ;
	gtk_widget_show(notebook) ;
	gtk_box_pack_start(GTK_BOX(vbox) , notebook , TRUE , TRUE , 0) ;

	separator = gtk_hseparator_new() ;
	gtk_widget_show(separator) ;
	gtk_box_pack_start(GTK_BOX(vbox) , separator , FALSE , FALSE , 2) ;

	hbox = apply_cancel_button();
	gtk_box_pack_start(GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	
	hbox = gtk_hbox_new(FALSE , 0) ;
	gtk_widget_show(hbox) ;
	gtk_box_pack_start(GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	frame = font_large_small();
	gtk_box_pack_start(GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;
	frame = wall_picture(window);
	gtk_box_pack_start(GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;

	/* contents of the "encoding" tab */

	label = gtk_label_new("encoding") ;
	gtk_widget_show(label) ;
	
	vbox = gtk_vbox_new(FALSE , 3) ;
	gtk_container_set_border_width(GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show(vbox) ;

	if( ! ( config_widget = mc_encoding_config_widget_new(encoding)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_xim_config_widget_new(xim, locale)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	hbox = gtk_hbox_new(TRUE , 5) ;
	gtk_widget_show(hbox) ;
	gtk_box_pack_start(GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;	

	if( ! ( use_bidi_check = mc_check_config_widget_new( "bidi (only UTF8)" , use_bidi)))
	{
		return  0 ;
	}
	gtk_widget_show(use_bidi_check) ;
	gtk_box_pack_start(GTK_BOX(hbox) , use_bidi_check , TRUE , TRUE , 0) ;
	
	if( ! ( is_comb_check = mc_check_config_widget_new( "char combining" , is_combining_char)))
	{
		return  0 ;
	}
	gtk_widget_show(is_comb_check) ;
	gtk_box_pack_start(GTK_BOX(hbox) , is_comb_check , TRUE , TRUE , 0) ;

	
	/* contents of the "copy&paste" tab */

	label = gtk_label_new("copy&paste") ;
	gtk_widget_show(label) ;
	vbox = gtk_vbox_new(FALSE , 3) ;
	gtk_container_set_border_width(GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show(vbox) ;

	if( ! ( config_widget = mc_utf8_sel_config_widget_new(prefer_utf8_selection)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , TRUE , TRUE , 0) ;
	
	if( ! ( config_widget = mc_xct_proc_config_widget_new(xct_proc_mode)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , TRUE , TRUE , 0) ;

	
	/* contents of the "appearance" tab */

	label = gtk_label_new("appearance") ;
	gtk_widget_show(label) ;
	vbox = gtk_vbox_new(FALSE , 3) ;
	gtk_container_set_border_width(GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show(vbox) ;

	if ( ! ( config_widget = mc_fg_color_config_widget_new(fg_color)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! (config_widget = mc_bg_color_config_widget_new(bg_color)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! (config_widget = mc_fontsize_config_widget_new(fontsize , min_fontsize , max_fontsize)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! (config_widget = mc_bel_config_widget_new(bel_mode)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	hbox = gtk_hbox_new(TRUE , 5) ;
	gtk_widget_show(hbox) ;
	gtk_box_pack_start(GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

	if( ! ( is_aa_check = mc_check_config_widget_new( "anti alias font" , is_aa)))
	{
		return  0 ;
	}
	gtk_widget_show( is_aa_check) ;
	gtk_box_pack_start(GTK_BOX(hbox) , is_aa_check , TRUE , TRUE , 0) ;
	
	if( ! ( is_tp_check = mc_check_config_widget_new( "transparent" , is_transparent)))
	{
		return  0 ;
	}
	gtk_widget_show( is_tp_check) ;
	gtk_box_pack_start(GTK_BOX(hbox) , is_tp_check , TRUE , TRUE , 0) ;


	/* contents of the "others" tab */

	label = gtk_label_new("others") ;
	gtk_widget_show(label) ;
	vbox = gtk_vbox_new(FALSE , 3) ;
	gtk_container_set_border_width(GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show(vbox) ;

	if( ! ( config_widget = mc_tabsize_config_widget_new(tabsize)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! (config_widget = mc_logsize_config_widget_new(logsize)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! (config_widget = mc_mod_meta_config_widget_new(mod_meta_mode)))
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


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
	int  fg_color ;
	int  bg_color ;
	char *  tabsize ;
	char *  logsize ;
	char *  fontsize ;
	u_int  min_fontsize ;
	u_int  max_fontsize ;
	int  mod_meta_mode ;
	int  bel_mode ;
	int  is_combining_char ;
	int  prefer_utf8_selection ;
	int  xct_proc_mode ;
	int  is_transparent ;
	int  is_aa ;
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

	/* this memory area is leaked , but this makes almost no matter */
	input_line = strdup( p) ;
	
	kik_file_delete( kin) ;
	fclose( in) ;

	/*
	 * input_line format
	 * [encoding] [fg color] [bg color] [tabsize] [logsize] [font size] [min font size] \
	 * [max font size] [is combining char] [prefer utf8 selection request] [xct proc mode] \
	 * [auto detect utf8 selection] [is transparent] [locale] [xim][LF]
	 */
	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &encoding , p))
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
		! kik_str_to_int( &min_fontsize , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &max_fontsize , p))
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
		! kik_str_to_int( &is_combining_char , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &prefer_utf8_selection , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &xct_proc_mode , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &is_transparent , p))
	{
		return  0 ;
	}
	
	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &is_aa , p))
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
	
	return  show( x , y , encoding , fg_color , bg_color , tabsize ,
		logsize , fontsize , min_fontsize , max_fontsize ,
		mod_meta_mode , bel_mode , is_combining_char ,
		prefer_utf8_selection , xct_proc_mode , is_transparent , is_aa ,
		use_bidi , xim , locale) ;
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
		kik_msg_printf( "usage: (stdin 15) mlconfig [x] [y] [in] [out]\n") ;
		
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
