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
#include  "mc_color.h"
#include  "mc_bgtype.h"
#include  "mc_brightness.h"
#include  "mc_contrast.h"
#include  "mc_gamma.h"
#include  "mc_fade.h"
#include  "mc_tabsize.h"
#include  "mc_logsize.h"
#include  "mc_fontsize.h"
#include  "mc_line_space.h"
#include  "mc_screen_ratio.h"
#include  "mc_mod_meta.h"
#include  "mc_bel.h"
#include  "mc_vertical.h"
#include  "mc_sb.h"
#include  "mc_xim.h"
#include  "mc_check.h"
#include  "mc_iscii_lang.h"
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

/*
 *  ********  procedures when buttons are clicked  ********
 */

static int
update(
	mc_io_t  io
	)
{
    mc_update_char_encoding() ;
    mc_update_iscii_lang() ;
    mc_update_color(MC_COLOR_FG) ;
    mc_update_bgtype() ;
    mc_update_color(MC_COLOR_SBFG) ;
    mc_update_color(MC_COLOR_SBBG) ;
    mc_update_tabsize() ;
    mc_update_logsize() ;
    mc_update_fontsize() ;
    mc_update_line_space() ;
    mc_update_screen_width_ratio() ;
    mc_update_screen_height_ratio() ;
    mc_update_mod_meta_mode() ;
    mc_update_bel_mode() ;
    mc_update_vertical_mode() ;
    mc_update_sb_mode() ;
    mc_update_brightness() ;
    mc_update_contrast() ;
    mc_update_gamma() ;
    mc_update_fade_ratio() ;
    mc_update_sb_view_name() ;
    mc_update_xim() ;

    mc_update_flag_mode(MC_FLAG_AA);
    mc_update_flag_mode(MC_FLAG_VCOL);
    mc_update_flag_mode(MC_FLAG_COMB);
    mc_update_flag_mode(MC_FLAG_DYNCOMB);
    mc_update_flag_mode(MC_FLAG_RECVUCS);
    mc_update_flag_mode(MC_FLAG_MCOL);
    mc_update_flag_mode(MC_FLAG_BIDI);

    mc_flush(io) ;
    
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
	mc_set_str_value( "full_reset" , "") ;
	mc_flush(mc_io_set);

	return  1 ;
}

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
addbutton(char *label, gint (func)(), GtkWidget *box)
{
	GtkWidget *button;
	button = gtk_button_new_with_label(label);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(func), NULL);
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

	frame = gtk_frame_new(_("Font size (temporal)")) ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	addbutton(_("Larger"),  larger_clicked,  hbox);
	addbutton(_("Smaller"), smaller_clicked, hbox);

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
	GtkWidget *  separator ;
	
	window = gtk_window_new( GTK_WINDOW_TOPLEVEL) ;
	gtk_signal_connect( GTK_OBJECT(window) , "delete_event" ,
		GTK_SIGNAL_FUNC(end_application) , NULL) ;
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


	if (!(config_widget = mc_iscii_lang_config_widget_new())) return 0;
#ifndef USE_IND
	gtk_widget_set_sensitive(config_widget, 0);
#endif
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_xim_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	hbox = gtk_hbox_new(TRUE, 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);	


	if (!(config_widget = mc_flag_config_widget_new(MC_FLAG_BIDI)))
	    return 0;
#ifndef USE_FRIBIDI
	gtk_widget_set_sensitive(config_widget, 0);
#endif
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, TRUE, TRUE, 0);


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


	if (!(config_widget = mc_fontsize_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_line_space_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


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

	if (!(config_widget = mc_fade_config_widget_new())) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);	
	
	
	if (!(config_widget = mc_flag_config_widget_new(MC_FLAG_AA)))
	    return 0;
#ifndef ANTI_ALIAS
	gtk_widget_set_sensitive(config_widget, 0);
#endif
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, TRUE, TRUE, 0) ;


	if (!(config_widget = mc_flag_config_widget_new(MC_FLAG_VCOL)))
	    return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, TRUE, TRUE, 0);


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
#if !defined(USE_IMLIB) && !defined(USE_GDK_PIXBUF)
	gtk_widget_set_sensitive(config_widget, 0);
#endif
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_contrast_config_widget_new())) return 0;
#if !defined(USE_IMLIB) && !defined(USE_GDK_PIXBUF)
	gtk_widget_set_sensitive(config_widget, 0);
#endif
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_gamma_config_widget_new())) return 0;
#if !defined(USE_IMLIB) && !defined(USE_GDK_PIXBUF)
	gtk_widget_set_sensitive(config_widget, 0);
#endif
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox2), config_widget, FALSE, FALSE, 0);


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


	if (!(config_widget = mc_vertical_config_widget_new())) return 0;
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


	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
	gtk_window_set_policy(GTK_WINDOW(window), 0, 0, 0);
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
	gtk_set_locale ();

	bindtextdomain( "mlconfig" , LOCALEDIR) ;
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
