/*
 *	$Id$
 */

#include  "mc_bel.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  is_visual_checked = 0 ;
static int  is_sound_checked = 0 ;
static int  is_none_checked = 0 ;


/* --- static functions --- */

static gint
button_none_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_none_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}

static gint
button_visual_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_visual_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}

static gint
button_sound_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_sound_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_bel_config_widget_new(
	ml_bel_mode_t  bel_mode
	)
{
	GtkWidget *  label ;
	GtkWidget *  hbox ;
	GtkWidget *  radio ;
	GSList *  group ;

	hbox = gtk_hbox_new(FALSE , 0) ;

	label = gtk_label_new( " Bel mode ") ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , TRUE , TRUE , 0) ;
	
	group = NULL ;

	radio = gtk_radio_button_new_with_label( group , "None") ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_none_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;

	if( bel_mode == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	radio = gtk_radio_button_new_with_label( group , "Sound") ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_sound_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( bel_mode == 1)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}
	
	radio = gtk_radio_button_new_with_label( group , "Visual") ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_visual_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( bel_mode == 2)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}
	
	return  hbox ;
}

ml_bel_mode_t
mc_get_bel_mode(void)
{
	if( is_visual_checked)
	{
		return  BEL_VISUAL ;
	}
	else if( is_sound_checked)
	{
		return  BEL_SOUND ;
	}
	else /* if( is_none_checked) */
	{
		return  BEL_NONE ;
	}
}
