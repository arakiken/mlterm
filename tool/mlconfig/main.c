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
#include  "mc_wall_pic.h"
#include  "mc_io.h"
#include  "mc_pty.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static GtkWidget *  use_aa_check ;
static GtkWidget *  use_vcol_check ;
static GtkWidget *  use_comb_check ;
static GtkWidget *  use_dynamic_comb_check ;
static GtkWidget *  use_multi_col_char_check ;
static GtkWidget *  use_bidi_check ;
static GtkWidget *  receive_string_via_ucs_check ;


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

static int
update(
	int  save
	)
{
    mc_set_str_value("encoding", mc_get_char_encoding(), save);
    mc_set_str_value("iscii_lang", mc_get_iscii_lang(), save);
    mc_set_str_value("fg_color", mc_get_fg_color(), save);
    mc_set_str_value("sb_fg_color", mc_get_sb_fg_color(), save);
    mc_set_str_value("sb_bg_color", mc_get_sb_bg_color(), save);
    mc_set_str_value("tabsize", mc_get_tabsize(), save);
    mc_set_str_value("logsize", mc_get_logsize(), save);
    mc_set_str_value("fontsize", mc_get_fontsize(), save);
    mc_set_str_value("line_space", mc_get_line_space(), save);
    mc_set_str_value("screen_width_ratio", mc_get_screen_width_ratio(), save);
    mc_set_str_value("screen_height_ratio", mc_get_screen_height_ratio(), save);
    mc_set_str_value("mod_meta_mode", mc_get_mod_meta_mode(), save);
    mc_set_str_value("bel_mode", mc_get_bel_mode(), save);
    mc_set_str_value("vertical_mode", mc_get_vertical_mode(), save);
    mc_set_str_value("scrollbar_mode", mc_get_sb_mode(), save);
    mc_set_str_value("brightness", mc_get_brightness(), save);
    mc_set_str_value("contrast", mc_get_contrast(), save);
    mc_set_str_value("gamma", mc_get_gamma(), save);
    mc_set_str_value("fade_ratio", mc_get_fade_ratio(), save);
    mc_set_str_value("scrollbar_view_name", mc_get_sb_view_name(), save);

    {
	char *  xim ;
	char *  locale ;

	if ((xim = mc_get_xim_name()) && (locale = mc_get_xim_locale()))
	{
	    char *  val ;

	    if ((val = malloc(strlen(xim) + 1 + strlen(locale) + 1)))
	    {
		sprintf(val, "%s:%s", xim, locale);
		mc_set_str_value("xim", val, save);
		free( val) ;
	    }
	}
    }

    {
	int bgtype_ischanged = mc_bgtype_ischanged();
	char *bgtype = mc_get_bgtype_mode();
	if (bgtype == NULL) {
	    ;
	} else if (!strcmp(bgtype, "color")) {
	    if (bgtype_ischanged) {
		mc_set_flag_value("use_transbg", 0, save);
		mc_set_str_value("wall_picture", "none", save);
	    }
	    if (bgtype_ischanged || mc_bg_color_ischanged())
		mc_set_str_value("bg_color", mc_get_bg_color(), save);
	} else if (!strcmp(bgtype, "picture")) {
	    if (bgtype_ischanged)
		mc_set_flag_value("use_transbg", 0, save);
	    if (bgtype_ischanged || mc_wall_pic_ischanged()) 
		mc_set_str_value("wall_picture", mc_get_wall_pic(), save);
	} else if (!strcmp(bgtype, "transparent")) {
	    if (bgtype_ischanged)
		mc_set_flag_value("use_transbg", 1, save);
	}
    }

    mc_set_flag_value("use_anti_alias", GTK_TOGGLE_BUTTON(use_aa_check)->active, save);
    mc_set_flag_value("use_variable_column_width", GTK_TOGGLE_BUTTON(use_vcol_check)->active, save);
    mc_set_flag_value("use_combining", GTK_TOGGLE_BUTTON(use_comb_check)->active, save);
    mc_set_flag_value("use_dynamic_comb", GTK_TOGGLE_BUTTON(use_dynamic_comb_check)->active, save);
    mc_set_flag_value("receive_string_via_ucs", GTK_TOGGLE_BUTTON(receive_string_via_ucs_check)->active, save);
    mc_set_flag_value("use_multi_column_char", GTK_TOGGLE_BUTTON(use_multi_col_char_check)->active, save);
    mc_set_flag_value("use_bidi", GTK_TOGGLE_BUTTON(use_bidi_check)->active, save);

	return  1 ;
}

static gint
apply_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	update( 0) ;
	
	return  1 ;
}

static gint
ok_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	update( 1) ;

	gtk_main_quit() ;
	
	return  1 ;
}

static gint
larger_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "fontsize" , "larger" , 0) ;
	
	return  1 ;
}

static gint
smaller_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "fontsize" , "smaller" , 0) ;
		
	return  1 ;
}

static gint
full_reset_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "full_reset" , "" , 0) ;

	return  1 ;
}

static gint
pty_button_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "select_pty" , mc_get_pty_dev() , 0) ;
	
	gtk_main_quit() ;
	
	return  1 ;
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

	button = gtk_button_new_with_label(_("OK")) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(ok_clicked) , NULL) ;
	gtk_widget_show(button) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 5) ;

	button = gtk_button_new_with_label(_("Apply")) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(apply_clicked) , NULL) ;
	gtk_widget_show(button) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 5) ;

	button = gtk_button_new_with_label(_("Cancel")) ;
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

	frame = gtk_frame_new(_("Font size")) ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	button = gtk_button_new_with_label(_("Larger")) ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(larger_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	button = gtk_button_new_with_label(_("Smaller")) ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(smaller_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	return frame;
}

static GtkWidget *
full_reset(void)
{
	GtkWidget *  frame ;
	GtkWidget *  hbox ;
	GtkWidget *  button ;

	frame = gtk_frame_new( _("Full reset")) ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	button = gtk_button_new_with_label( _("Full reset")) ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(full_reset_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;
	
	return frame;
}

static GtkWidget *
pty_list(void)
{
	GtkWidget *  frame ;
	GtkWidget *  hbox ;
	GtkWidget *  button ;
	GtkWidget *  config_widget ;

	frame = gtk_frame_new( _("PTY List")) ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;
	
	button = gtk_button_new_with_label( "Select") ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(pty_button_clicked) , NULL) ;
	gtk_widget_show(button) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , FALSE , FALSE , 0) ;
	
	config_widget = mc_pty_config_widget_new( mc_get_str_value( "pty_name") ,
				mc_get_str_value( "pty_list")) ;
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start( GTK_BOX(hbox) , config_widget , FALSE , FALSE , 0) ;

	return  frame ;
}

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
	GtkWidget *  bgcolor ;
	GtkWidget *  bgpicture ;
	
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


	if (!(config_widget = mc_char_encoding_config_widget_new( 
		   mc_get_str_value("encoding")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_iscii_lang_config_widget_new(
		   mc_get_str_value( "iscii_lang")))) return 0;
#ifndef USE_IND
	gtk_widget_set_sensitive(config_widget, 0);
#endif
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_xim_config_widget_new(
		   mc_get_str_value("xim"), mc_get_str_value("locale"))))
	    return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	hbox = gtk_hbox_new(TRUE, 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);	


	if (!(use_bidi_check = mc_check_config_widget_new(
		   _("Bidi (UTF8 only)"), mc_get_flag_value( "use_bidi"))))
	    return 0;
#ifndef USE_FRIBIDI
	gtk_widget_set_sensitive(use_bidi_check, 0);
#endif
	gtk_widget_show(use_bidi_check);
	gtk_box_pack_start(GTK_BOX(hbox), use_bidi_check, TRUE, TRUE, 0);


	if(!(use_comb_check = mc_check_config_widget_new(
		  _("Combining"), mc_get_flag_value("use_combining"))))
	    return 0;
	gtk_widget_show(use_comb_check);
	gtk_box_pack_start(GTK_BOX(hbox), use_comb_check, TRUE, TRUE , 0);


	if (!(receive_string_via_ucs_check = mc_check_config_widget_new(
		   _("Process received strings via Unicode") , 
		   mc_get_flag_value( "receive_string_via_ucs"))))
	    return 0;
	gtk_widget_show(receive_string_via_ucs_check);
	gtk_box_pack_start(GTK_BOX(vbox), receive_string_via_ucs_check,
			   FALSE, FALSE, 0);


	/* contents of the "Font" tab */

	label = gtk_label_new(_("Font"));
	gtk_widget_show(label);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
	gtk_widget_show(vbox);


	if (!(config_widget = mc_fontsize_config_widget_new(
		   mc_get_str_value("fontsize")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_line_space_config_widget_new(
		   mc_get_str_value("line_space")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	frame = gtk_frame_new(_("Screen ratio against font size"));
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(frame), hbox) ;

	if (!(config_widget = mc_screen_width_ratio_config_widget_new(
		   mc_get_str_value("screen_width_ratio")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_screen_height_ratio_config_widget_new(
		   mc_get_str_value( "screen_height_ratio")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_fg_color_config_widget_new(
		   mc_get_str_value( "fg_color")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_fade_config_widget_new(
		   mc_get_str_value( "fade_ratio")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);	
	

	if (!(use_aa_check = mc_check_config_widget_new(
		   _("Anti Alias"), mc_get_flag_value("use_anti_alias"))))
	    return 0;
#ifndef ANTI_ALIAS
	gtk_widget_set_sensitive(use_aa_check, 0);
#endif
	gtk_widget_show(use_aa_check);
	gtk_box_pack_start(GTK_BOX(hbox), use_aa_check, TRUE, TRUE, 0) ;


	if (!(use_vcol_check = mc_check_config_widget_new(
		   _("Variable column width") , 
		   mc_get_flag_value( "use_variable_column_width"))))
	    return 0;
	gtk_widget_show(use_vcol_check);
	gtk_box_pack_start(GTK_BOX(hbox), use_vcol_check, TRUE, TRUE, 0);


	/* contents of the "Background" tab */
	
	label = gtk_label_new(_("Background"));
	gtk_widget_show(label);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
	gtk_widget_show(vbox);


	/* prepare bg_color and wall_picture but pack later */
	if (!(bgcolor = mc_bg_color_config_widget_new(
		   mc_get_str_value( "bg_color")))) return 0;
	gtk_widget_show(bgcolor);

	if (!(bgpicture = mc_wall_pic_config_widget_new(
		   mc_get_str_value( "wall_picture")))) return 0;
	gtk_widget_show(bgpicture);

	if (!(config_widget = mc_bgtype_config_widget_new(
		  mc_get_bgtype(), bgcolor, bgpicture)))
	    return 0;
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

	if (!(config_widget = mc_brightness_config_widget_new(
		   mc_get_str_value( "brightness")))) return 0;
#if !defined(USE_IMLIB) && !defined(USE_GDK_PIXBUF)
	gtk_widget_set_sensitive(config_widget, 0);
#endif
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_contrast_config_widget_new(
		   mc_get_str_value( "contrast")))) return 0;
#if !defined(USE_IMLIB) && !defined(USE_GDK_PIXBUF)
	gtk_widget_set_sensitive(config_widget, 0);
#endif
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

	if (!(config_widget = mc_gamma_config_widget_new(
		   mc_get_str_value( "gamma")))) return 0;
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


	if (!(config_widget = mc_sb_config_widget_new(
		   mc_get_str_value( "scrollbar_mode")))) return 0;
	gtk_widget_show(config_widget) ;
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_sb_view_config_widget_new(
		   mc_get_str_value( "scrollbar_view_name")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_sb_fg_color_config_widget_new(
		   mc_get_str_value( "sb_fg_color")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);
	
	if (!(config_widget = mc_sb_bg_color_config_widget_new(
		   mc_get_str_value("sb_bg_color")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);
		
	
	/* contents of the "Others" tab */

	label = gtk_label_new(_("Others"));
	gtk_widget_show(label);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
	gtk_widget_show(vbox);

	if (!(config_widget = mc_tabsize_config_widget_new(
		   mc_get_str_value("tabsize")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_logsize_config_widget_new(
		   mc_get_str_value("logsize")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_mod_meta_config_widget_new(
		   mc_get_str_value("mod_meta_mode")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_vertical_config_widget_new(
		   mc_get_str_value("vertical_mode")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(config_widget = mc_bel_config_widget_new(
		   mc_get_str_value("bel_mode")))) return 0;
	gtk_widget_show(config_widget);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);


	if (!(use_dynamic_comb_check = mc_check_config_widget_new(
		   _("Combining = 1 (or 0) logical column(s)"),
		   mc_get_flag_value("use_dynamic_comb")))) return 0;
	gtk_widget_show(use_dynamic_comb_check);
	gtk_box_pack_start(GTK_BOX(vbox), use_dynamic_comb_check,
			   FALSE, FALSE, 0);


	if (!(use_multi_col_char_check = mc_check_config_widget_new(
		   _("Fullwidth = 2 (or 1) logical column(s)"),
		   mc_get_flag_value("use_multi_column_char")))) return 0;
	gtk_widget_show(use_multi_col_char_check);
	gtk_box_pack_start(GTK_BOX(vbox), use_multi_col_char_check,
			   FALSE, FALSE, 0);


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
