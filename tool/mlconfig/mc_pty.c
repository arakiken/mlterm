/*
 *	$Id$
 */

#include  "mc_pty.h"

#include  <stdio.h>
#include  <string.h>
#include  <stdlib.h>	/* malloc */
#include  <kiklib/kik_str.h>	/* kik_str_sep */

#include  "mc_combo.h"
#include  "mc_io.h"

#define MAX_TERMS 32 /* this must coincide with xwindow/x_term_manager.c */

/* --- static variables --- */

static char *  new_pty ;
static char *  old_pty ;


/* --- static functions --- */

static gint
selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_pty = gtk_entry_get_text(GTK_ENTRY(widget));

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_pty_config_widget_new(void)
{
	char *  my_pty ;
	char *  pty_list ;
	char *  ptys[MAX_TERMS];
	int num;

	my_pty = mc_get_str_value( "pty_name") ;
	pty_list = mc_get_str_value( "pty_list") ;

	if (my_pty == NULL) return NULL;

	ptys[0] = my_pty + 5;
	num = 1;
	while( pty_list) {
		if (strlen(my_pty) <= 5) break;
		ptys[num] = pty_list + 5;
		pty_list = strchr(pty_list, ':');
		if (pty_list) *pty_list = 0; else break;
		if (*(pty_list + 1) == '0') num++;
		pty_list = strchr(pty_list + 1, ';');
		if (pty_list) pty_list++;
	}

	new_pty = old_pty = strdup(my_pty + 5);

	return mc_combo_new("", ptys, num, new_pty, 1, selected, NULL);
}

void
mc_select_pty(void)
{
	if( strcmp( new_pty , old_pty) != 0)
	{
		char *  dev ;

		if( ( dev = malloc( 5 + strlen( new_pty) + 1)) == NULL)
		{
			return ;
		}

		sprintf( dev , "/dev/%s" , new_pty) ;

		mc_set_str_value( "select_pty" , dev) ;
		mc_flush(mc_io_set);

		free( dev) ;

		free( old_pty) ;
		old_pty = strdup( new_pty) ;
	}
}
