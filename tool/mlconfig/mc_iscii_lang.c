/*
 *	$Id$
 */

#include  "mc_iscii_lang.h"

#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_iscii_lang = NULL;
static char *  old_iscii_lang = NULL;
static int is_changed;


/* --- static functions --- */

static gint
iscii_lang_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	free( new_iscii_lang);
	new_iscii_lang = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s iscii_lang is selected.\n" , new_iscii_lang) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_iscii_lang_config_widget_new(void)
{
	char *  iscii_langs[] =
	{
		"Assamese" ,
		"Bengali",
		"Gujarati",
		"Hindi",
		"Kannada",
		"Malayalam",
		"Oriya",
		"Punjabi",
		"Roman",
		"Tamil",
		"Telugu",

	} ;
	
	new_iscii_lang = strdup( old_iscii_lang = mc_get_str_value( "iscii_lang")) ;
	is_changed = 0;

	return  mc_combo_new( _("ISCII language") , iscii_langs ,
		sizeof(iscii_langs) / sizeof(iscii_langs[0]) ,
		new_iscii_lang , 1 , iscii_lang_selected , NULL) ;
}

void
mc_update_iscii_lang(void)
{
	if (strcmp(new_iscii_lang, old_iscii_lang)) is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "iscii_lang" , new_iscii_lang) ;
		free( old_iscii_lang) ;
		old_iscii_lang = strdup( new_iscii_lang) ;
	}
}
