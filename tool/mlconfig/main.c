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
#include  "mc_color.h"
#include  "mc_bgtype.h"
#include  "mc_alpha.h"
#include  "mc_brightness.h"
#include  "mc_contrast.h"
#include  "mc_gamma.h"
#include  "mc_fade.h"
#include  "mc_tabsize.h"
#include  "mc_logsize.h"
#include  "mc_font.h"
#include  "mc_line_space.h"
#include  "mc_letter_space.h"
#include  "mc_screen_ratio.h"
#include  "mc_mod_meta.h"
#include  "mc_bel.h"
#include  "mc_sb.h"
#include  "mc_im.h"
#include  "mc_sb_view.h"
#include  "mc_io.h"
#include  "mc_pty.h"
#include  "mc_flags.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */


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
bidi_flag_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	GtkWidget *  ind_flag ;

	ind_flag = data ;

	gtk_widget_set_sensitive( ind_flag ,
		! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget))) ;

	return  1 ;
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
    /*
     * alpha must be updated before bgtype because transparent or wall picture
     * bgtype could change alpha from 255 to 0.
     */
    mc_update_alpha() ;
    mc_update_bgtype() ;
    mc_update_color(MC_COLOR_SBFG) ;
    mc_update_color(MC_COLOR_SBBG) ;
    mc_update_tabsize() ;
    mc_update_logsize() ;
    mc_update_font_misc() ;
    mc_update_line_space() ;
    mc_update_letter_space() ;
    mc_update_screen_width_ratio() ;
    mc_update_screen_height_ratio() ;
    mc_update_mod_meta_mode() ;
    mc_update_bel_mode() ;
    mc_update_sb_mode() ;
    mc_update_brightness() ;
    mc_update_contrast() ;
    mc_update_gamma() ;
    mc_update_fade_ratio() ;
    mc_update_sb_view_name() ;
    mc_update_im() ;

    mc_update_flag_mode(MC_FLAG_COMB);
    mc_update_flag_mode(MC_FLAG_DYNCOMB);
    mc_update_flag_mode(MC_FLAG_RECVUCS);
    mc_update_flag_mode(MC_FLAG_MCOL);
    mc_update_flag_mode(MC_FLAG_BIDI);
    mc_update_flag_mode(MC_FLAG_IND);
    mc_update_flag_mode(MC_FLAG_AWIDTH);
    mc_update_flag_mode(MC_FLAG_CLIPBOARD);

    mc_flush(io) ;

    if( io == mc_io_set)
    {
	mc_update_font_name( mc_io_set_font) ;
    }
    else if( io == mc_io_set_save)
    {
	mc_update_font_name( mc_io_set_save_font) ;
    }
    
    return  1 ;
}

static gint
cancel_clicked(GtkWidget *widget, gpointer data)
{
	gtk_main_quit(); return FALSE;
}

static gint
apply_clicked(GtkWidget *widget, gpointer data)
{
	update(mc_io_set); return 1;
}

static gint
saveexit_clicked(GtkWidget *widget, gpointer data)
{
	update(mc_io_set_save); gtk_main_quit(); return 1;
}

static gint
applyexit_clicked(GtkWidget * widget, gpointer data)
{
	update(mc_io_set); gtk_main_quit(); return 1;
}

static gint
larger_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "fontsize" , "larger") ;
	mc_flush(mc_io_set);
	
	return  1 ;
}

static gint
smaller_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "fontsize" , "smaller") ;
	mc_flush(mc_io_set);
	
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

	uris = g_uri_list_extract_uris( data->data) ;
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

	gtk_widget_hide_all( gtk_widget_get_toplevel( widget)) ;
	
	dialog = gtk_dialog_new() ;
	gtk_window_set_title( GTK_WINDOW(dialog) , "mlconfig") ;
	gtk_dialog_add_button( GTK_DIALOG(dialog) , _("Send") , MY_RESPONSE_SEND) ;
	gtk_dialog_add_button( GTK_DIALOG(dialog) , _("Recv") , MY_RESPONSE_RECV) ;
	gtk_dialog_add_button( GTK_DIALOG(dialog) , _("Return") , MY_RESPONSE_RETURN) ;
	gtk_dialog_add_button( GTK_DIALOG(dialog) , _("Exit") , MY_RESPONSE_EXIT) ;

	content_area = GTK_DIALOG(dialog)->vbox ;
	
	hbox = gtk_hbox_new(FALSE , 0) ;
	gtk_widget_show(hbox) ;

	label = gtk_label_new( _("Local")) ;
	gtk_widget_show(label) ;
	gtk_widget_set_usize( label , 70 , 0) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , FALSE , TRUE , 1) ;
	
	local_entry = gtk_entry_new() ;
	gtk_widget_show(local_entry) ;
	gtk_widget_set_usize( local_entry , 280 , 0) ;
	gtk_drag_dest_set( local_entry , GTK_DEST_DEFAULT_ALL ,
		local_targets , 1 , GDK_ACTION_COPY) ;
	g_signal_connect( local_entry , "drag-data-received" ,
			G_CALLBACK(drag_data_received) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , local_entry , FALSE , TRUE , 1) ;

	gtk_container_add( GTK_CONTAINER(content_area) , hbox) ;

	hbox = gtk_hbox_new(FALSE , 0) ;
	gtk_widget_show(hbox) ;

	label = gtk_label_new( _("Remote")) ;
	gtk_widget_show(label) ;
	gtk_widget_set_usize( label , 70 , 0) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , FALSE , TRUE , 1) ;

	remote_entry = gtk_entry_new() ;
	gtk_widget_show(remote_entry) ;
	gtk_widget_set_usize( remote_entry , 280 , 0) ;
	gtk_box_pack_start(GTK_BOX(hbox) , remote_entry , FALSE , TRUE , 1) ;

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
	mc_set_str_value("open_pty" , "") ;
	mc_flush(mc_io_set);
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
addbutton(const char *label, gint (func)(GtkWidget *, gpointer), GtkWidget *box)
{
	GtkWidget *button;
	button = gtk_button_new_with_label(label);
	g_signal_connect(button, "clicked",
			   G_CALLBACK(func), NULL);
	gtk_widget_show(button);
	gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
}

static GtkWidget *
apply_cancel_button(
	void
	)
{
	GtkWidget * hbox ;

	hbox = gtk_hbox_new(FALSE , 5) ;
	gtk_widget_show(hbox) ;

	addbutton(_("Save&Exit"),  saveexit_clicked,  hbox);
	addbutton(_("Apply&Exit"), applyexit_clicked, hbox);
	addbutton(_("Apply"),      apply_clicked,     hbox);
	addbutton(_("Cancel"),     cancel_clicked,    hbox);

	return hbox;
}

static GtkWidget *
font_large_small(void)
{
	GtkWidget * frame;
	GtkWidget * hbox;

	frame = gtk_frame_new(_("Font size (temporary)")) ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	addbutton(_("Larger"),  larger_clicked,  hbox);
	addbutton(_("Smaller"), smaller_clicked, hbox);

#if  GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_text(frame ,
		"If you change fonts from \"Select\" button in \"Font\" tab, "
		"it is not recommended to change font size here.") ;
#endif

	return frame;
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

	addbutton(_("Full reset"), full_reset_clicked, hbox);
	
	return frame;
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

	addbutton(_("SSH SCP"), ssh_scp_clicked, hbox);

	return frame;
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

	addbutton(_(" New "),    pty_new_button_clicked, hbox);
	addbutton(_(" Select "), pty_button_clicked,     hbox);
	
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
	GtkWidget *  bidi_flag ;
	GtkWidget *  separator ;
	
	window = gtk_window_new( GTK_WINDOW_TOPLEVEL) ;
	g_signal_connect( window , "delete_event" , G_CALLBACK(end_application) , NULL) ;
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

	hbox = apply_cancel_button();
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	
	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	frame = font_large_small();
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;
	frame = full_reset();
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;
#ifdef  USE_LIBSSH2
	frame = ssh_scp() ;
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;
#endif

	frame = pty_list() ;
	gtk_box_pack_start( GTK_BOX(vbox) , frame , FALSE , FALSE , 0) ;
	
	/* contents of the "Encoding" tab */

	label = gtk_label_new(_("Encoding"));
	gtk_widget_show(label);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
	gtk_widget_show(vbox);


	if (!(config_widget = mc_char_encoding_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_auto_detect_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_im_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	hbox = gtk_hbox_new(TRUE, 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	if (!(bidi_flag = mc_flag_config_widget_new(MC_FLAG_BIDI)))
	    return 0;
	gtk_widget_show(bidi_flag);
	gtk_box_pack_start(GTK_BOX(hbox), bidi_flag, TRUE, TRUE, 0);

	if (!(config_widget = mc_flag_config_widget_new(MC_FLAG_IND)))
	    return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, TRUE, TRUE, 0);

	g_signal_connect(bidi_flag, "toggled" ,
		G_CALLBACK(bidi_flag_checked), config_widget);
	gtk_widget_set_sensitive(config_widget,
		! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(bidi_flag)));

	if(!(config_widget = mc_flag_config_widget_new(MC_FLAG_COMB)))
	    return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, TRUE, TRUE , 0);


	if (!(config_widget = mc_flag_config_widget_new(MC_FLAG_RECVUCS)))
	    return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

	/* contents of the "Font" tab */

	label = gtk_label_new(_("Font"));
	gtk_widget_show(label);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
	gtk_widget_show(vbox);


	if (!(config_widget = mc_font_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);
	

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	if (!(config_widget = mc_line_space_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, TRUE, TRUE, 0);

	if (!(config_widget = mc_letter_space_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, TRUE, TRUE, 0);


	frame = gtk_frame_new(_("Screen ratio against font size"));
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(frame), hbox) ;

	if (!(config_widget = mc_screen_width_ratio_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_screen_height_ratio_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_color_config_widget_new(MC_COLOR_FG)))
		return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	/* contents of the "Background" tab */
	
	label = gtk_label_new(_("Background"));
	gtk_widget_show(label);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
	gtk_widget_show(vbox);


	if (!(config_widget = mc_bgtype_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	frame = gtk_frame_new(_("Picture/Transparent"));
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	vbox2 = gtk_vbox_new(FALSE, 3);
	gtk_widget_show(vbox2);
	gtk_container_add(GTK_CONTAINER(frame), vbox2) ;
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);

	if (!(config_widget = mc_brightness_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_contrast_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);

	if (!(config_widget = mc_gamma_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_alpha_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_fade_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	/* contents of the "Scrollbar" tab */
	
	label = gtk_label_new(_("Scrollbar"));
	gtk_widget_show(label);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
	gtk_widget_show(vbox);


	if (!(config_widget = mc_sb_config_widget_new())) return 0;
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_sb_view_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_color_config_widget_new(MC_COLOR_SBFG)))
		return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);
	
	if (!(config_widget = mc_color_config_widget_new(MC_COLOR_SBBG)))
		return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);
		
	
	/* contents of the "Others" tab */

	label = gtk_label_new(_("Others"));
	gtk_widget_show(label);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
	gtk_widget_show(vbox);

	if (!(config_widget = mc_tabsize_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_logsize_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_mod_meta_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_bel_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_flag_config_widget_new(MC_FLAG_DYNCOMB)))
	    return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_flag_config_widget_new(MC_FLAG_MCOL)))
	    return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_flag_config_widget_new(MC_FLAG_AWIDTH)))
	    return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_flag_config_widget_new(MC_FLAG_CLIPBOARD)))
	    return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
#if ! GTK_CHECK_VERSION(2,90,0)
	gtk_window_set_policy(GTK_WINDOW(window), 0, 0, 0);
#endif
	gtk_widget_show(window);

	gtk_main();

	return  1;
}


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
#if ! GTK_CHECK_VERSION(2,90,0)
	gtk_set_locale ();
#endif

	bindtextdomain( "mlconfig" , LOCALEDIR) ;
	bind_textdomain_codeset ( "mlconfig", "UTF-8") ;
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
