/*
 *	$Id$
 */

#include  "mc_font_present.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static ml_font_present_t  new_font_present ;


/* --- static functions --- */

static gint
button_var_col_width_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_font_present |= FONT_VAR_WIDTH ;
	}
	else
	{
		new_font_present &= ~FONT_VAR_WIDTH ;
	}
	
	return  1 ;
}

static gint
button_aa_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_font_present |= FONT_AA ;
	}
	else
	{
		new_font_present &= ~FONT_AA ;
	}
	
	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_font_present_config_widget_new(
	ml_font_present_t  font_present
	)
{
	GtkWidget *  hbox ;
	GtkWidget *  check ;

	hbox = gtk_hbox_new(FALSE , 0) ;

	check = gtk_check_button_new_with_label( "Variable column width") ;
	gtk_signal_connect(GTK_OBJECT(check) , "toggled" ,
		GTK_SIGNAL_FUNC(button_var_col_width_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(check)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , check , TRUE , TRUE , 0) ;
	
	if( font_present & FONT_VAR_WIDTH)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(check) , TRUE) ;
	}
	
	check = gtk_check_button_new_with_label( "Anti Alias") ;
	gtk_signal_connect(GTK_OBJECT(check) , "toggled" , GTK_SIGNAL_FUNC(button_aa_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(check)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , check , TRUE , TRUE , 0) ;
	
	if( font_present & FONT_AA)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(check) , TRUE) ;
	}

	new_font_present = font_present ;
	
	return  hbox ;
}

ml_font_present_t
mc_get_font_present(void)
{
	return  new_font_present ;
}
