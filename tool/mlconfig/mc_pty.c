/*
 *	$Id$
 */

#include  "mc_pty.h"

#include  <stdio.h>
#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>	/* alloca */

#include  "mc_io.h"


/* --- static variables --- */

static char *  new_pty ;
static char *  old_pty ;


/* --- static functions --- */

static gint
button_toggled(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_pty = (char *)data ;
	}

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_pty_config_widget_new(void)
{
	GtkWidget *  hbox ;
	GtkWidget *  radio ;
	GSList *  group ;
	char *  my_pty ;
	char *  pty_list ;
	char *  dev ;

	my_pty = mc_get_str_value( "pty_name") ;
	pty_list = mc_get_str_value( "pty_list") ;

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
	
	new_pty = old_pty = dev ;

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

void
mc_select_pty(void)
{
	if( strcmp( new_pty , old_pty) != 0)
	{
		char *  dev ;

		if( ( dev = alloca( 5 + strlen( new_pty) + 1)) == NULL)
		{
			return ;
		}

		sprintf( dev , "/dev/%s" , new_pty) ;

		mc_set_str_value( "select_pty" , dev , 0) ;

		old_pty = new_pty ;
	}
}
