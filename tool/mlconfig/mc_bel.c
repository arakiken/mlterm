/*
 *	$Id$
 */

#include  "mc_bel.h"

#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  old_bel_mode ;
static char *  new_bel_mode ;
static int is_changed;


/* --- static functions --- */

static gint
button_none_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_bel_mode = "none" ;
	}
	
	return  1 ;
}

static gint
button_visual_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_bel_mode = "visual" ;
	}
	
	return  1 ;
}

static gint
button_sound_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_bel_mode = "sound" ;
	}
	
	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_bel_config_widget_new(void)
{
	GtkWidget *  label ;
	GtkWidget *  hbox ;
	GtkWidget *  radio ;
	GSList *  group ;
	char *  bel_mode ;

	bel_mode = mc_get_str_value( "bel_mode") ;

	hbox = gtk_hbox_new(FALSE , 0) ;

	label = gtk_label_new( _(" Bel mode ")) ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , TRUE , TRUE , 0) ;
	
	group = NULL ;

	radio = gtk_radio_button_new_with_label( group , _("None")) ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_none_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;

	if( strcmp( bel_mode , "none") == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	radio = gtk_radio_button_new_with_label( group , _("Sound")) ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_sound_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( strcmp( bel_mode , "sound") == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}
	
	radio = gtk_radio_button_new_with_label( group , _("Visual")) ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_visual_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( strcmp( bel_mode , "visual") == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	old_bel_mode = new_bel_mode = bel_mode ;
	is_changed = 0;
	
	return  hbox ;
}

void
mc_update_bel_mode(void)
{
	if (strcmp(new_bel_mode, old_bel_mode)) is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "bel_mode" , new_bel_mode) ;
		free( old_bel_mode) ;
		old_bel_mode = strdup( new_bel_mode) ;
	}
}
