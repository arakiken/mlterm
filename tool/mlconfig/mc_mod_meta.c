/*
 *	$Id$
 */

#include  "mc_mod_meta.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  is_none_checked = 0 ;
static int  is_esc_checked = 0 ;
static int  is_8bit_checked = 0 ;


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
button_esc_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_esc_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}

static gint
button_8bit_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_8bit_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_mod_meta_config_widget_new(
	ml_mod_meta_mode_t  mod_meta_mode
	)
{
	GtkWidget *  label ;
	GtkWidget *  hbox ;
	GtkWidget *  radio ;
	GSList *  group ;

	hbox = gtk_hbox_new(FALSE , 0) ;

	label = gtk_label_new( "Mod Meta mode") ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , TRUE , TRUE , 0) ;
	
	group = NULL ;

	radio = gtk_radio_button_new_with_label( group , "None") ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_none_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;

	if( mod_meta_mode == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	radio = gtk_radio_button_new_with_label( group , "Esc") ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_esc_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( mod_meta_mode == 1)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}
	
	radio = gtk_radio_button_new_with_label( group , "8bit") ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_8bit_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( mod_meta_mode == 2)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}
	
	return  hbox ;
}

ml_mod_meta_mode_t
mc_get_mod_meta_mode(void)
{
	if( is_esc_checked)
	{
		return  MOD_META_OUTPUT_ESC ;
	}
	else if( is_8bit_checked)
	{
		return  MOD_META_SET_MSB ;
	}
	else /* if( is_none_checked) */
	{
		return  MOD_META_NONE ;
	}
}
