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

/*
 * !!! Notice !!!
 * the order should be the same as ml_iscii_lang_t in ml_iscii.h
 */
static char *  iscii_langs[] =
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

static ml_iscii_lang_t  selected_iscii_lang = 0 ;


/* --- static functions --- */

static gint
iscii_lang_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	int  counter ;
	char *  text ;

	text = gtk_entry_get_text(GTK_ENTRY(widget)) ;

	for( counter = 0 ; counter < sizeof( iscii_langs) / sizeof( iscii_langs[0]) ; counter ++)
	{
		if( strcmp( text , iscii_langs[counter]) == 0)
		{
			break ;
		}
	}
	
	selected_iscii_lang = counter ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s iscii_lang is selected.\n" , selected_iscii_lang) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_iscii_lang_config_widget_new(
	ml_iscii_lang_t  iscii_lang
	)
{
	selected_iscii_lang = iscii_lang ;

	return  mc_combo_new( "ISCII lang" , iscii_langs , sizeof(iscii_langs) / sizeof(iscii_langs[0]) ,
		iscii_langs[selected_iscii_lang] , 1 , iscii_lang_selected , NULL) ;
}

ml_iscii_lang_t
mc_get_iscii_lang(void)
{
	return  selected_iscii_lang ;
}
