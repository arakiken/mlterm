/*
 *	$Id$
 */

#include  "mc_pty.h"

#include  <kiklib/kik_str.h>


/* --- static variables --- */

static char *  selected_dev ;

#include  <stdio.h>

/* --- static functions --- */

static gint
button_toggled(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		selected_dev = (char *)data ;
	}

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_pty_config_widget_new(
	char *  my_pty ,
	char *  pty_list
	)
{
	GtkWidget *  hbox ;
	GtkWidget *  radio ;
	GSList *  group ;
	char *  dev ;

	if( my_pty == NULL || ( dev = kik_str_sep( &my_pty , ":")) == NULL || strlen( dev) <= 5)
	{
		return  NULL ;
	}

	dev = strdup( dev + 5) ;
	
	hbox = gtk_hbox_new( FALSE , 0) ;
	group = NULL ;

	radio = gtk_radio_button_new_with_label( group , dev) ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" ,
		GTK_SIGNAL_FUNC(button_toggled) , dev) ;
	gtk_widget_show(radio) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , FALSE , FALSE , 0) ;
	
	selected_dev = dev ;

	while( pty_list && ( dev = kik_str_sep( &pty_list , ":")) && pty_list)
	{
		if( *pty_list == '0' && strlen( dev) > 5)
		{
			dev = strdup( dev + 5) ;
			radio = gtk_radio_button_new_with_label( group , dev) ;
			group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
			gtk_signal_connect(GTK_OBJECT(radio) , "toggled" ,
				GTK_SIGNAL_FUNC(button_toggled) , dev) ;
			gtk_widget_show(radio) ;
			gtk_box_pack_start(GTK_BOX(hbox) , radio , FALSE , FALSE , 0) ;
		}

		pty_list += 2 ;
	}

	return  hbox ;
}

char *
mc_get_pty_dev(void)
{
	char *  dev ;

	if( ( dev = malloc( 5 + strlen( selected_dev) + 1)) == NULL)
	{
		return  NULL ;
	}

	sprintf( dev , "/dev/%s" , selected_dev) ;

	return  dev ;
}
