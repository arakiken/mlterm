/*
 *	$Id$
 */

#include  "mc_iscii_lang.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_iscii_lang ;
static int  is_changed ;


/* --- static functions --- */

static gint
iscii_lang_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_iscii_lang = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	is_changed = 1 ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s iscii_lang is selected.\n" , selected_iscii_lang) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_iscii_lang_config_widget_new(
	char *  iscii_lang
	)
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
	
	selected_iscii_lang = iscii_lang ;

	return  mc_combo_new( "ISCII language" , iscii_langs , sizeof(iscii_langs) / sizeof(iscii_langs[0]) ,
		selected_iscii_lang , 1 , iscii_lang_selected , NULL) ;
}

char *
mc_get_iscii_lang(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  selected_iscii_lang ;
}
