/*
 *	$Id$
 */

#include  "mc_utf8_sel.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  is_always_checked = 0 ;
static int  is_auto_checked = 0 ;
static int  is_never_checked = 0 ;


/* --- static functions --- */

static gint
button_never_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_never_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}

static gint
button_always_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_always_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}

static gint
button_auto_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_auto_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_utf8_sel_config_widget_new(
	int  prefer_utf8_selection
	)
{
	GtkWidget *  frame ;
	GtkWidget *  hbox ;
	GtkWidget *  radio ;
	GSList *  group ;

	frame = gtk_frame_new( "Prefer utf8 to xct in selection") ;
	gtk_widget_show(frame) ;
	gtk_container_set_border_width(GTK_CONTAINER(frame) , 5) ;
	
	hbox = gtk_hbox_new(FALSE , 0) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;
	
	group = NULL ;

	radio = gtk_radio_button_new_with_label(group , "always") ;
	group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_always_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( prefer_utf8_selection == 1)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}
	
	radio = gtk_radio_button_new_with_label( group , "when subset of unicode") ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_auto_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( prefer_utf8_selection == -1)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	radio = gtk_radio_button_new_with_label( group , "never") ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_never_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;

	if( prefer_utf8_selection == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	return  frame ;
}

int
mc_get_utf8_sel_mode(void)
{
	if( is_always_checked)
	{
		return  1 ;
	}
	else if( is_auto_checked)
	{
		return  -1 ;
	}
	else /* if( is_never_checked) */
	{
		return  0 ;
	}
}
