/*
 *	$Id$
 */

#include  <stdio.h>		/* sprintf */
#include  <gtk/gtk.h>
#include  <glib.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>

#include  <c_intl.h>

#include  "mc_char_encoding.h"
#include  "mc_auto_detect.h"
#include  "mc_ctl.h"
#include  "mc_color.h"
#include  "mc_bgtype.h"
#include  "mc_alpha.h"
#include  "mc_tabsize.h"
#include  "mc_logsize.h"
#include  "mc_wordsep.h"
#include  "mc_font.h"
#include  "mc_space.h"
#include  "mc_im.h"
#include  "mc_sb_view.h"
#include  "mc_io.h"
#include  "mc_pty.h"
#include  "mc_flags.h"
#include  "mc_ratio.h"
#include  "mc_radio.h"
#include  "mc_char_width.h"
#include  "mc_geometry.h"


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static void
end_application(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	gtk_main_quit() ;
}

/*
 *  ********  procedures when buttons are clicked  ********
 */

static int
update(
	mc_io_t  io
	)
{
	mc_update_char_encoding() ;
	mc_update_auto_detect() ;
	mc_update_color(MC_COLOR_FG) ;
	if( mc_is_color_bg())
	{
		mc_update_bgtype() ;
		mc_update_alpha() ;
	}
	else
	{
		/*
		 * alpha must be updated before bgtype because transparent or wall picture
		 * bgtype could change alpha from 255 to 0.
		 */
		mc_update_alpha() ;
		mc_update_bgtype() ;
	}
	mc_update_tabsize() ;
	mc_update_logsize() ;
	mc_update_wordsep() ;
	mc_update_geometry() ;
	mc_update_font_misc() ;
	mc_update_space(MC_SPACE_LINE) ;
	mc_update_space(MC_SPACE_LETTER) ;
	mc_update_ratio(MC_RATIO_SCREEN_WIDTH) ;
	mc_update_ratio(MC_RATIO_SCREEN_HEIGHT) ;
	mc_update_radio(MC_RADIO_MOD_META_MODE) ;
	mc_update_radio(MC_RADIO_BELL_MODE) ;
	mc_update_radio(MC_RADIO_LOG_VTSEQ) ;
	mc_update_ratio(MC_RATIO_BRIGHTNESS) ;
	mc_update_ratio(MC_RATIO_CONTRAST) ;
	mc_update_ratio(MC_RATIO_GAMMA) ;
	mc_update_ratio(MC_RATIO_FADE) ;
	mc_update_im() ;
	mc_update_cursor_color() ;
	mc_update_substitute_color() ;
	mc_update_ctl() ;
	mc_update_char_width() ;

	mc_update_flag_mode(MC_FLAG_COMB) ;
	mc_update_flag_mode(MC_FLAG_DYNCOMB) ;
	mc_update_flag_mode(MC_FLAG_RECVUCS) ;
	mc_update_flag_mode(MC_FLAG_CLIPBOARD) ;
	mc_update_flag_mode(MC_FLAG_LOCALECHO) ;
	mc_update_flag_mode(MC_FLAG_BLINKCURSOR) ;
	mc_update_flag_mode(MC_FLAG_STATICBACKSCROLL) ;
	mc_update_flag_mode(MC_FLAG_EXTSCROLLSHORTCUT) ;
	mc_update_flag_mode(MC_FLAG_REGARDURIASWORD) ;

	mc_update_radio(MC_RADIO_SB_MODE) ;
#ifndef  USE_QUARTZ
	mc_update_color(MC_COLOR_SBFG) ;
	mc_update_color(MC_COLOR_SBBG) ;
	mc_update_sb_view_name() ;
#endif

	mc_flush(io) ;

	if( io == mc_io_set)
	{
		mc_update_font_name( mc_io_set_font) ;
		mc_update_vtcolor( mc_io_set_color) ;
	}
	else if( io == mc_io_set_save)
	{
		mc_update_font_name( mc_io_set_save_font) ;
		mc_update_vtcolor( mc_io_set_save_color) ;
	}

	return  1 ;
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
	update( mc_io_set) ;
	return  1 ;
}

static gint
saveexit_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	update( mc_io_set_save) ;
	gtk_main_quit() ;
	return 1 ;
}

static gint
applyexit_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	update( mc_io_set) ;
	gtk_main_quit() ;
	return 1 ;
}

static gint
larger_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "fontsize" , "larger") ;
	mc_flush(mc_io_set) ;

	return  1 ;
}

static gint
smaller_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "fontsize" , "smaller") ;
	mc_flush( mc_io_set) ;

	return  1 ;
}

static gint
full_reset_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_exec( "full_reset") ;

	return  1 ;
}

#ifdef  USE_LIBSSH2

#define  MY_RESPONSE_RETURN  1
#define  MY_RESPONSE_EXIT    2
#define  MY_RESPONSE_SEND    3
#define  MY_RESPONSE_RECV    4

static void
drag_data_received(
	GtkWidget *  widget ,
	GdkDragContext *  context ,
	gint  x ,
	gint  y ,
	GtkSelectionData *  data ,
	guint  info ,
	guint  time
	)
{
	gchar **  uris ;
	gchar *  filename ;

#if  GTK_CHECK_VERSION(2,14,0)
	uris = g_uri_list_extract_uris( gtk_selection_data_get_data( data)) ;
#else
	uris = g_uri_list_extract_uris( data->data) ;
#endif
	filename = g_filename_from_uri( uris[0] , NULL , NULL) ;
	gtk_entry_set_text( GTK_ENTRY(widget) , filename) ;
	g_free( filename) ;
	g_strfreev( uris) ;
}

static gint
ssh_scp_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	GtkWidget *  dialog ;
	GtkWidget *  content_area ;
	GtkWidget *  hbox ;
	GtkWidget *  label ;
	GtkWidget *  local_entry ;
	GtkWidget *  remote_entry ;
	gint  res ;
	GtkTargetEntry  local_targets[] =
	{
		{ "text/uri-list" , 0 , 0 } ,
	} ;

	gtk_widget_hide( gtk_widget_get_toplevel( widget)) ;
	
	dialog = gtk_dialog_new() ;
	gtk_window_set_title( GTK_WINDOW(dialog) , "mlconfig") ;
	gtk_dialog_add_button( GTK_DIALOG(dialog) , _("Send") , MY_RESPONSE_SEND) ;
	gtk_dialog_add_button( GTK_DIALOG(dialog) , _("Recv") , MY_RESPONSE_RECV) ;
	gtk_dialog_add_button( GTK_DIALOG(dialog) , _("Return") , MY_RESPONSE_RETURN) ;
	gtk_dialog_add_button( GTK_DIALOG(dialog) , _("Exit") , MY_RESPONSE_EXIT) ;

#if  GTK_CHECK_VERSION(2,14,0)
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog)) ;
#else
	content_area = GTK_DIALOG(dialog)->vbox ;
#endif

	hbox = gtk_hbox_new(FALSE , 0) ;
	gtk_widget_show(hbox) ;

	label = gtk_label_new( _("Local")) ;
	gtk_widget_show(label) ;
	gtk_widget_set_size_request( label , 70 , -1) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , FALSE , FALSE , 1) ;
	
	local_entry = gtk_entry_new() ;
	gtk_widget_show(local_entry) ;
	gtk_widget_set_size_request( local_entry , 280 , -1) ;
	gtk_drag_dest_set( local_entry , GTK_DEST_DEFAULT_ALL ,
		local_targets , 1 , GDK_ACTION_COPY) ;
	g_signal_connect( local_entry , "drag-data-received" ,
			G_CALLBACK(drag_data_received) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , local_entry , FALSE , FALSE , 1) ;

	gtk_container_add( GTK_CONTAINER(content_area) , hbox) ;

	hbox = gtk_hbox_new(FALSE , 0) ;
	gtk_widget_show(hbox) ;

	label = gtk_label_new( _("Remote")) ;
	gtk_widget_show(label) ;
	gtk_widget_set_size_request( label , 70 , -1) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , FALSE , FALSE , 1) ;

	remote_entry = gtk_entry_new() ;
	gtk_widget_show(remote_entry) ;
	gtk_widget_set_size_request( remote_entry , 280 , -1) ;

	gtk_box_pack_start(GTK_BOX(hbox) , remote_entry , FALSE , FALSE , 1) ;

	gtk_container_add( GTK_CONTAINER(content_area) , hbox) ;

	while( (res = gtk_dialog_run( GTK_DIALOG(dialog))) >= MY_RESPONSE_SEND)
	{
		char *  cmd ;
		const gchar *  local_path ;
		const gchar *  remote_path ;

		local_path = gtk_entry_get_text( GTK_ENTRY(local_entry)) ;
		remote_path = gtk_entry_get_text( GTK_ENTRY(remote_entry)) ;

		if( ( cmd = alloca( 28 + strlen( local_path) + strlen( remote_path))))
		{
			char *  p ;

			if( res == MY_RESPONSE_SEND)
			{
				if( ! *local_path)
				{
					kik_msg_printf( "Local file path to send "
						"is not specified.\n") ;

					continue ;
				}

				sprintf( cmd , "scp \"local:%s\" \"remote:%s\" UTF8" ,
						local_path , remote_path) ;
			}
			else /* if( res == MY_RESPONSE_RECV) */
			{
				if( ! *remote_path)
				{
					kik_msg_printf( "Remote file path to receive "
						"is not specified.\n") ;

					continue ;
				}

				sprintf( cmd , "scp \"remote:%s\" \"local:%s\" UTF8" ,
						remote_path , local_path) ;
			}

			p = cmd + strlen(cmd) - 2 ;
			if( *p == '\\')
			{
				/*
				 * Avoid to be parsed as follows.
				 * "local:c:\foo\bar\" => local:c:\foo\bar"
				 */
				*(p++) = '\"' ;
				*p = '\0' ;
			}
			
			mc_exec( cmd) ;
		}
	}

	if( res == MY_RESPONSE_EXIT)
	{
		gtk_main_quit() ;

		return  FALSE ;
	}
	else /* if( res == MY_RESPONSE_RETURN) */
	{
		gtk_widget_destroy( dialog) ;
		gtk_widget_show_all( gtk_widget_get_toplevel( widget)) ;

		return  TRUE ;
	}
}

#endif	/* USE_LIBSSH2 */

static gint
pty_new_button_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_exec( "open_pty") ;
	mc_flush( mc_io_set) ;
	gtk_main_quit() ;
	
	return  1 ;
}	

static gint
pty_button_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_select_pty() ;

	/*
	 * As soon as  pty changed, stdout is also changed, but mlconfig couldn't
	 * follow it.
	 */
	gtk_main_quit() ;
	
	return  1 ;
}	

/*
 *  ********  Building GUI (lower part, independent buttons)  ********
 */

static void
addbutton(
	const char *  label ,
	gint (func)(GtkWidget * , gpointer) ,
	GtkWidget *  box
	)
{
	GtkWidget *  button ;
	button = gtk_button_new_with_label( label) ;
	g_signal_connect( button , "clicked" , G_CALLBACK(func) , NULL) ;
	gtk_widget_show( button) ;
	gtk_box_pack_start( GTK_BOX(box) , button , TRUE , TRUE , 0) ;
}

static GtkWidget *
apply_cancel_button(void)
{
	GtkWidget * hbox ;

	hbox = gtk_hbox_new(FALSE , 5) ;
	gtk_widget_show(hbox) ;

	addbutton(_("Save&Exit"),  saveexit_clicked,  hbox) ;
	addbutton(_("Apply&Exit"), applyexit_clicked, hbox) ;
	addbutton(_("Apply"),      apply_clicked,     hbox) ;
	addbutton(_("Cancel"),     cancel_clicked,    hbox) ;

	return hbox ;
}

static GtkWidget *
font_large_small(void)
{
	GtkWidget * frame ;
	GtkWidget * hbox ;

	frame = gtk_frame_new(_("Font size (temporary)")) ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	addbutton(_("Larger"),  larger_clicked,  hbox) ;
	addbutton(_("Smaller"), smaller_clicked, hbox) ;

#if  GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_text(frame ,
		"If you change fonts from \"Select\" button in \"Font\" tab, "
		"it is not recommended to change font size here.") ;
#endif

	return frame ;
}

static GtkWidget *
full_reset(void)
{
	GtkWidget *  frame ;
	GtkWidget *  hbox ;

	frame = gtk_frame_new( _("Full reset")) ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	addbutton(_("Full reset"), full_reset_clicked, hbox) ;
	
	return frame ;
}

#ifdef  USE_LIBSSH2
static GtkWidget *
ssh_scp(void)
{
	GtkWidget *  frame ;
	GtkWidget *  hbox ;

	frame = gtk_frame_new( _("SSH SCP")) ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	addbutton(_("SSH SCP"), ssh_scp_clicked, hbox) ;

	return frame ;
}
#endif	/* USE_LIBSSH2 */

static GtkWidget *
pty_list(void)
{
	GtkWidget *  frame ;
	GtkWidget *  hbox ;
	GtkWidget *  config_widget ;

	if( ( config_widget = mc_pty_config_widget_new()) == NULL)
	{
		return  NULL ;
	}
	gtk_widget_show(config_widget) ;

	frame = gtk_frame_new( _("PTY List")) ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	addbutton( _(" New ") , pty_new_button_clicked , hbox) ;
	addbutton( _("Select") , pty_button_clicked , hbox) ;

	gtk_box_pack_start(GTK_BOX(hbox) , config_widget , TRUE , TRUE , 0) ;

	return  frame ;
}


/*
 *  ********  Building GUI (main part, page (tab)-separated widgets)  ********
 */

static int
show(void)
{
	GtkWidget *  window ;
	GtkWidget *  vbox ;
	GtkWidget *  vbox2 ;
	GtkWidget *  hbox ;
	GtkWidget *  notebook ;
	GtkWidget *  frame ;
	GtkWidget *  label ;
	GtkWidget *  config_widget ;
	GtkWidget *  separator ;
	
	window = gtk_window_new( GTK_WINDOW_TOPLEVEL) ;
	g_signal_connect( window , "destroy" , G_CALLBACK(end_application) , NULL) ;
	gtk_window_set_title( GTK_WINDOW(window) , _("mlterm configuration")) ;
	gtk_container_set_border_width( GTK_CONTAINER(window) , 0) ;

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
	gtk_box_pack_start( GTK_BOX(vbox) , separator , FALSE , FALSE , 0) ;

	hbox = apply_cancel_button() ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	
	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	frame = font_large_small() ;
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;
	frame = full_reset() ;
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;
#ifdef  USE_LIBSSH2
	frame = ssh_scp() ;
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;
#endif

	frame = pty_list() ;
	gtk_box_pack_start( GTK_BOX(vbox) , frame , FALSE , FALSE , 0) ;
	
	/* contents of the "Encoding" tab */

	label = gtk_label_new( _("Encoding")) ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;


	config_widget = mc_char_encoding_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	config_widget = mc_auto_detect_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_im_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_ctl_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

	config_widget = mc_flag_config_widget_new( MC_FLAG_COMB) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;

	config_widget = mc_flag_config_widget_new( MC_FLAG_DYNCOMB) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_flag_config_widget_new( MC_FLAG_RECVUCS) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	/* contents of the "Font" tab */

	label = gtk_label_new( _("Font")) ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 0) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;


	config_widget = mc_font_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

	config_widget = mc_space_config_widget_new( MC_SPACE_LINE) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;

	config_widget = mc_space_config_widget_new( MC_SPACE_LETTER) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;


	frame = gtk_frame_new( _("Screen ratio against font size")) ;
	gtk_widget_show( frame) ;
	gtk_box_pack_start( GTK_BOX(vbox) , frame , FALSE , FALSE , 0) ;
	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_container_set_border_width( GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show( hbox) ;
	gtk_container_add( GTK_CONTAINER(frame) , hbox) ;

	config_widget = mc_ratio_config_widget_new( MC_RATIO_SCREEN_WIDTH) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;

	config_widget = mc_ratio_config_widget_new( MC_RATIO_SCREEN_HEIGHT) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;


	/* contents of the "Background" tab */
	
	label = gtk_label_new( _("Background")) ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;


	config_widget = mc_bgtype_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	frame = gtk_frame_new( _("Picture/Transparent")) ;
	gtk_widget_show( frame) ;
	gtk_box_pack_start( GTK_BOX(vbox) , frame , FALSE , FALSE , 0) ;
	vbox2 = gtk_vbox_new( FALSE , 3) ;
	gtk_widget_show( vbox2) ;
	gtk_container_add( GTK_CONTAINER(frame) , vbox2) ;
	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_container_set_border_width( GTK_CONTAINER(hbox) , 2) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox2) , hbox , FALSE , FALSE , 0) ;

	config_widget = mc_ratio_config_widget_new( MC_RATIO_BRIGHTNESS) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;

	config_widget = mc_ratio_config_widget_new( MC_RATIO_GAMMA) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;

	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_container_set_border_width( GTK_CONTAINER(hbox) , 2) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox2) , hbox , FALSE , FALSE , 0) ;

	config_widget = mc_ratio_config_widget_new( MC_RATIO_CONTRAST) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;

	config_widget = mc_alpha_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_ratio_config_widget_new( MC_RATIO_FADE) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	/* contents of the "Color" tab */

	label = gtk_label_new( _("Color")) ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;


	config_widget = mc_cursor_color_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_substitute_color_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_vtcolor_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	/* contents of the "Scrollbar" tab */
	
	label = gtk_label_new( _("Scrollbar")) ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;


	config_widget = mc_radio_config_widget_new( MC_RADIO_SB_MODE) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


#ifndef  USE_QUARTZ
	config_widget = mc_sb_view_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

	config_widget = mc_color_config_widget_new( MC_COLOR_SBFG) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 5) ;

	config_widget = mc_color_config_widget_new( MC_COLOR_SBBG) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;
#endif


	/* contents of the "Others" tab */

	label = gtk_label_new( _("Others")) ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;


	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

	config_widget = mc_tabsize_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;

	config_widget = mc_logsize_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;


	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

	config_widget = mc_geometry_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;

	config_widget = mc_wordsep_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_radio_config_widget_new( MC_RADIO_MOD_META_MODE) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_radio_config_widget_new( MC_RADIO_BELL_MODE) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_radio_config_widget_new( MC_RADIO_LOG_VTSEQ) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_char_width_config_widget_new() ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

#ifndef  USE_WIN32GUI
	config_widget = mc_flag_config_widget_new( MC_FLAG_CLIPBOARD) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;
#endif

	config_widget = mc_flag_config_widget_new( MC_FLAG_LOCALECHO) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;

	config_widget = mc_flag_config_widget_new( MC_FLAG_BLINKCURSOR) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_flag_config_widget_new( MC_FLAG_STATICBACKSCROLL) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_flag_config_widget_new( MC_FLAG_EXTSCROLLSHORTCUT) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	config_widget = mc_flag_config_widget_new( MC_FLAG_REGARDURIASWORD) ;
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;


	gtk_window_set_position( GTK_WINDOW(window) , GTK_WIN_POS_MOUSE) ;
#if ! GTK_CHECK_VERSION(2 ,90 ,0)
	gtk_window_set_policy( GTK_WINDOW(window) , 0 , 0 , 0) ;
#endif

	gtk_widget_show( window) ;

#if  defined(UES_WIN32GUI) || defined(USE_QUARTZ)
	gtk_window_set_keep_above( GTK_WINDOW(window) , TRUE) ;
#endif

	gtk_main() ;

	return  1 ;
}


/* --- global functions --- */

int
main( 
	int  argc ,
	char **  argv
	)
{
#if ! GTK_CHECK_VERSION(2 ,90 ,0)
	gtk_set_locale () ;
#endif

	bindtextdomain( "mlconfig" , LOCALEDIR) ;
	bind_textdomain_codeset ( "mlconfig" , "UTF-8") ;
	textdomain( "mlconfig") ;

#ifdef  __DEBUG
	{
		int  count ;

		for( count = 0 ; count < argc ; count ++)
		{
			fprintf( stderr , "%s\n" , argv[count]) ;
		}
	}
#endif

	kik_set_msg_log_file_name( "mlterm/msg.log") ;

	gtk_init( &argc , &argv) ;

 	if( show() == 0)
	{
		kik_msg_printf( "Starting mlconfig failed.\n") ;
		
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}
