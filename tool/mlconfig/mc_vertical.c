/*
 *	$Id$
 */

#include  "mc_vertical.h"

#include  <kiklib/kik_str.h>
#include  <stdlib.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_vertical_mode ;
static char *  old_vertical_mode ;
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
		new_vertical_mode = "none" ;
	}
	
	return  1 ;
}

static gint
button_cjk_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_vertical_mode = "cjk" ;
	}
	
	return  1 ;
}

static gint
button_mongol_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_vertical_mode = "mongol" ;
	}
	
	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_vertical_config_widget_new(void)
{
	GtkWidget *  label ;
	GtkWidget *  hbox ;
	GtkWidget *  radio ;
	GSList *  group ;
	char *  vertical_mode ;

	vertical_mode = mc_get_str_value( "vertical_mode") ;

	hbox = gtk_hbox_new(FALSE , 0) ;

	label = gtk_label_new( _(" Vertical mode ")) ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , TRUE , TRUE , 0) ;
	
	group = NULL ;

	radio = gtk_radio_button_new_with_label( group , _("None")) ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_none_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;

	if( strcmp( vertical_mode , "none") == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	radio = gtk_radio_button_new_with_label( group , _("CJK")) ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_cjk_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( strcmp( vertical_mode , "cjk") == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}
	
	radio = gtk_radio_button_new_with_label( group , _("Mongol")) ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_mongol_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( strcmp( vertical_mode , "mongol") == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	new_vertical_mode = old_vertical_mode = vertical_mode ;
	is_changed = 0;
	
	return  hbox ;
}

void
mc_update_vertical_mode(void)
{
	if (strcmp(new_vertical_mode, old_vertical_mode)) is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "vertical_mode" , new_vertical_mode) ;
		free( old_vertical_mode) ;
		old_vertical_mode = strdup( new_vertical_mode) ;
	}
}
