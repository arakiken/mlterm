/*
 *	update: <2001/11/26(22:50:20)>
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
#include  "mc_char_combining.h"
#include  "mc_pre_conv.h"
#include  "mc_transparent.h"
#include  "mc_aa.h"
#include  "mc_mod_meta.h"
#include  "mc_xim.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static FILE *  out ;


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
	 * [combining char] [pre conv xct to ucs] [is transparent] [is aa] [xim] [locale][LF]
	 */
	fprintf( out , "CONFIG:%d %d %d %d %d %s %d %d %d %d %d %s %s\n" ,
		mc_get_encoding() ,
		mc_get_fg_color() ,
		mc_get_bg_color() ,
		mc_get_tabsize() ,
		mc_get_logsize() ,
		mc_get_fontsize() ,
		mc_get_mod_meta_mode() ,
		mc_is_combining_char() ,
		mc_is_pre_conv_xct_to_ucs() ,
		mc_is_transparent() ,
		mc_is_aa() ,
		mc_get_xim_name() ,
		mc_get_xim_locale()) ;

	gtk_main_quit() ;
	
	return  1 ;
}

static gint
larger_pressed(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	fprintf( out , "FONT:larger\n") ;

	gtk_main_quit() ;
	
	return  FALSE ;
}

static gint
smaller_pressed(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	fprintf( out , "FONT:smaller\n") ;
	
	gtk_main_quit() ;
	
	return  FALSE ;
}

static gint
file_sel_ok_pressed(
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
file_sel_cancel_pressed(
	GtkObject *  object
	)
{
	gtk_widget_destroy( GTK_WIDGET(object)) ;
	
	return  FALSE ;
}

static gint
wall_pic_pressed(
	GtkObject *  object
	)
{
	GtkWidget *  file_sel ;

	file_sel = gtk_file_selection_new( "Wall Picture") ;
	gtk_widget_show( GTK_WIDGET(file_sel)) ;

	gtk_signal_connect_object( GTK_OBJECT( GTK_FILE_SELECTION(file_sel)->ok_button ) ,
		"clicked" , GTK_SIGNAL_FUNC(file_sel_ok_pressed) , GTK_OBJECT(file_sel)) ;

	gtk_signal_connect_object( GTK_OBJECT( GTK_FILE_SELECTION(file_sel)->cancel_button ) ,
		"clicked" , GTK_SIGNAL_FUNC(file_sel_cancel_pressed) , GTK_OBJECT(file_sel)) ;
		
	return  TRUE ;
}

static gint
no_wall_pic_pressed(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	fprintf( out , "WALL_PIC:off\n") ;

	gtk_main_quit() ;

	return  FALSE ;
}

static int
show(
	int  x ,
	int  y ,
	ml_encoding_type_t  cur_encoding ,
	ml_color_t  cur_fg_color ,
	ml_color_t  cur_bg_color ,
	char *  cur_tabsize ,
	char *  cur_logsize ,
	char *  cur_fontsize ,
	u_int  min_fontsize ,
	u_int  max_fontsize ,
	ml_mod_meta_mode_t  mod_meta_mode ,
	int  is_combining_char ,
	int  pre_conv_xct_to_ucs ,
	int  is_transparent ,
	int  is_aa ,
	char *  cur_xim ,
	char *  cur_locale
	)
{
	GtkWidget *  window ;
	GtkWidget *  notebook ;
	GtkWidget *  frame ;
	GtkWidget *  label ;
	GtkWidget *  vbox ;
	GtkWidget *  config_widget ;
	GtkWidget *  hbox ;
	GtkWidget *  button ;
	GtkWidget *  separator ;
	
	window = gtk_window_new( GTK_WINDOW_TOPLEVEL) ;
	gtk_signal_connect(GTK_OBJECT(window) , "delete_event" ,
		GTK_SIGNAL_FUNC(end_application) , NULL) ;
	gtk_window_set_title(GTK_WINDOW(window) , "mlterm configuration") ;
	gtk_container_set_border_width(GTK_CONTAINER(window) , 10) ;
	gtk_widget_show(window) ;
	gdk_window_move(window->window , x , y) ;
	
	notebook = gtk_notebook_new() ;
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook) , GTK_POS_TOP) ;
	gtk_widget_show(notebook) ;
	
	gtk_container_add(GTK_CONTAINER(window) , notebook) ;
	
	label = gtk_label_new("mlterm") ;
	gtk_widget_show(label) ;

	frame = gtk_frame_new("current setting") ;
	gtk_container_set_border_width(GTK_CONTAINER(frame) , 5) ;
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook) , frame , label) ;
	gtk_widget_show(frame) ;
	
	vbox = gtk_vbox_new(FALSE , 5) ;
	gtk_widget_show(vbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , vbox) ;
	
	if( ( config_widget = mc_encoding_config_widget_new(cur_encoding)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , TRUE , TRUE , 0) ;

	if( ( config_widget = mc_fg_color_config_widget_new(cur_fg_color)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , TRUE , TRUE , 0) ;
	
	if( ( config_widget = mc_bg_color_config_widget_new(cur_bg_color)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , TRUE , TRUE , 0) ;
	
	if( ( config_widget = mc_tabsize_config_widget_new(cur_tabsize)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , TRUE , TRUE , 0) ;

	if( ( config_widget = mc_logsize_config_widget_new(cur_logsize)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , TRUE , TRUE , 0) ;

	if( ( config_widget = mc_fontsize_config_widget_new(cur_fontsize , min_fontsize , max_fontsize))
		== NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , TRUE , TRUE , 0) ;

	if( ( config_widget = mc_xim_config_widget_new(cur_xim , cur_locale)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , TRUE , TRUE , 0) ;

	
	if( ( config_widget = mc_mod_meta_config_widget_new(mod_meta_mode)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox) , config_widget , TRUE , TRUE , 0) ;
	
	
	hbox = gtk_hbox_new(FALSE , 0) ;
	gtk_widget_show(hbox) ;
	gtk_box_pack_start(GTK_BOX(vbox) , hbox , TRUE , TRUE , 0) ;

	if( ( config_widget = mc_char_combining_config_widget_new(is_combining_char)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(hbox) , config_widget , TRUE , TRUE , 0) ;
	
	if( ( config_widget = mc_pre_conv_config_widget_new(pre_conv_xct_to_ucs)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(hbox) , config_widget , TRUE , TRUE , 0) ;

	if( ( config_widget = mc_transparent_config_widget_new(is_transparent)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(hbox) , config_widget , TRUE , TRUE , 0) ;

	if( ( config_widget = mc_aa_config_widget_new(is_aa)) == NULL)
	{
		return  0 ;
	}
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(hbox) , config_widget , TRUE , TRUE , 0) ;
	
	hbox = gtk_hbox_new(FALSE , 5) ;
	gtk_widget_show(hbox) ;
	gtk_box_pack_start(GTK_BOX(vbox) , hbox , TRUE , TRUE , 0) ;
	
	
	label = gtk_label_new( "Change Setting") ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , FALSE , FALSE , 2) ;
	
	button = gtk_button_new_with_label("apply") ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(apply_clicked) , NULL) ;
	gtk_widget_show(button) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 2) ;
	
	button = gtk_button_new_with_label("cancel") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "pressed" , GTK_SIGNAL_FUNC(end_application) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 2) ;


	separator = gtk_hseparator_new() ;
	gtk_widget_show(separator) ;
	gtk_box_pack_start(GTK_BOX(vbox) , separator , FALSE , FALSE , 0) ;


	hbox = gtk_hbox_new(FALSE , 5) ;
	gtk_widget_show(hbox) ;
	gtk_box_pack_start(GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

	label = gtk_label_new( "Change Font Size") ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , FALSE , FALSE , 2) ;
	
	button = gtk_button_new_with_label("larger") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "pressed" , GTK_SIGNAL_FUNC(larger_pressed) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 2) ;

	button = gtk_button_new_with_label("smaller") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "pressed" , GTK_SIGNAL_FUNC(smaller_pressed) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 2) ;
	

	separator = gtk_hseparator_new() ;
	gtk_widget_show(separator) ;
	gtk_box_pack_start(GTK_BOX(vbox) , separator , FALSE , FALSE , 0) ;


	hbox = gtk_hbox_new(FALSE , 5) ;
	gtk_widget_show(hbox) ;
	gtk_box_pack_start(GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	
	label = gtk_label_new( "Change Wall Pic ") ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , FALSE , FALSE , 2) ;
	
	button = gtk_button_new_with_label( "select") ;
	gtk_widget_show(button) ;
	gtk_signal_connect_object(GTK_OBJECT(button) ,
		"pressed" , GTK_SIGNAL_FUNC(wall_pic_pressed) , GTK_OBJECT(window)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 2) ;

	button = gtk_button_new_with_label( "off") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "pressed" , GTK_SIGNAL_FUNC(no_wall_pic_pressed) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 2) ;


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
	
	int  cur_encoding ;
	int  cur_fg_color ;
	int  cur_bg_color ;
	char *  cur_tabsize ;
	char *  cur_logsize ;
	char *  cur_fontsize ;
	u_int  min_fontsize ;
	u_int  max_fontsize ;
	int  cur_mod_meta_mode ;
	int  is_combining_char ;
	int  pre_conv_xct_to_ucs ;
	int  is_transparent ;
	int  is_aa ;
	char *  cur_locale ;
	char *  cur_xim ;

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
	 * [max font size] [is combining char] [pre conv xct to cs] [is transparent] \
	 * [locale] [xim][LF]
	 */
	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &cur_encoding , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &cur_fg_color , p))
	{
		return  0 ;
	}
	
	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &cur_bg_color , p))
	{
		return  0 ;
	}
	
	if( ( cur_tabsize = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}

	if( ( cur_logsize = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}

	if( ( cur_fontsize = kik_str_sep( &input_line , " ")) == NULL)
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
		! kik_str_to_int( &cur_mod_meta_mode , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &is_combining_char , p))
	{
		return  0 ;
	}

	if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
		! kik_str_to_int( &pre_conv_xct_to_ucs , p))
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
	
	if( ( cur_xim = kik_str_sep( &input_line , " ")) == NULL)
	{
		return  0 ;
	}
	
	if( ( cur_locale = input_line) == NULL)
	{
		return  0 ;
	}
	
	return  show( x , y , cur_encoding , cur_fg_color , cur_bg_color , cur_tabsize ,
		cur_logsize , cur_fontsize , min_fontsize , max_fontsize , cur_mod_meta_mode ,
		is_combining_char , pre_conv_xct_to_ucs , is_transparent , is_aa ,
		cur_xim , cur_locale) ;
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
