/*
 *	$Id$
 */

#include  "mc_xct_proc.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  is_ucs_checked = 0 ;
static int  is_normal_checked = 0 ;
static int  is_raw_checked = 0 ;


/* --- static functions --- */

static gint
button_ucs_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_ucs_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}

static gint
button_raw_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_raw_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}

static gint
button_normal_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_normal_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_xct_proc_config_widget_new(
	ml_xct_proc_mode_t  xct_proc_mode
	)
{
	GtkWidget *  frame ;
	GtkWidget *  vbox ;
	GtkWidget *  hbox ;
	GtkWidget *  radio ;
	GSList *  group ;

	frame = gtk_frame_new( "How to process xct") ;
	gtk_widget_show(frame) ;
	gtk_container_set_border_width(GTK_CONTAINER(frame) , 5) ;
	
	vbox = gtk_vbox_new(FALSE , 0) ;
	gtk_widget_show(vbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , vbox) ;
	
	group = NULL ;

	
	radio = gtk_radio_button_new_with_label(group , "xct -> ucs -> pty encoding => pty") ;
	group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_ucs_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(vbox) , radio , TRUE , TRUE , 0) ;
	
	if( xct_proc_mode == XCT_PRECONV_UCS)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	
	hbox = gtk_hbox_new(FALSE , 0) ;
	gtk_widget_show(hbox) ;
	gtk_box_pack_start(GTK_BOX(vbox) , hbox , TRUE , TRUE , 0) ;
	
	radio = gtk_radio_button_new_with_label( group , "xct => pty") ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_raw_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( xct_proc_mode == XCT_RAW)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}
	

	radio = gtk_radio_button_new_with_label( group , "xct -> pty encoding => pty") ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_normal_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;

	if( xct_proc_mode == XCT_NORMAL)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	return  frame ;
}

ml_xct_proc_mode_t
mc_get_xct_proc_mode(void)
{
	if( is_ucs_checked)
	{
		return  XCT_PRECONV_UCS ;
	}
	else if( is_raw_checked)
	{
		return  XCT_RAW ;
	}
	else /* if( is_normal_checked) */
	{
		return  XCT_NORMAL ;
	}
}
