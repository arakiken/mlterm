/*
 *	$Id$
 */

#include  "mc_flags.h"

#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  old_flag_modes[MC_FLAG_MODES] ;
static int  new_flag_modes[MC_FLAG_MODES] ;
static int  is_changed[MC_FLAG_MODES] ;
static char *  config_keys[MC_FLAG_MODES] =
{
	"type_engine" ,
	"type_engine" ,
	"use_anti_alias",
	"use_variable_column_width",
	"use_combining",
	"use_dynamic_comb",
	"receive_string_via_ucs",
	"use_multi_column_char",
	"use_bidi" ,
	"use_ind" ,
	"col_size_of_width_a" ,
	"use_clipboard" ,
	"use_local_echo" ,
	"blink_cursor" ,
} ;

static char *labels[MC_FLAG_MODES] =
{
	N_("Xft"),
	N_("Cairo"),
	N_("Anti Alias"),
	N_("Variable column width"),
	N_("Combining"),
	N_("Combining = 1 (or 0) logical column(s)"),
	N_("Process received strings via Unicode"),
	N_("Fullwidth = 2 (or 1) logical column(s)"),
	N_("Bidi (UTF8 only)"),
	N_("Indic"),
	N_("Ambiguouswidth = fullwidth (UTF8 only)"),
	N_("CLIPBOARD Selection"),
	N_("Local echo"),
	N_("Blink cursor"),
} ;

static GtkWidget *  widgets[MC_FLAG_MODES];


/* --- static functions --- */

static gint
bidi_flag_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	GtkWidget *  ind_flag ;

	ind_flag = data ;

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget)))
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(ind_flag) , 0) ;
	}

	gtk_widget_set_sensitive( ind_flag ,
		! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget))) ;

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_flag_config_widget_new(
	int id
	)
{
	if( id == MC_FLAG_XFT)
	{
		old_flag_modes[id] = new_flag_modes[id] = 
			( strcmp( mc_get_str_value( config_keys[id]) , "xft") == 0) ;
	}
	else if( id == MC_FLAG_CAIRO)
	{
		old_flag_modes[id] = new_flag_modes[id] = 
			( strcmp( mc_get_str_value( config_keys[id]) , "cairo") == 0) ;
	}
	else if( id == MC_FLAG_AWIDTH)
	{
		old_flag_modes[id] = new_flag_modes[id] =
			( strcmp( mc_get_str_value( config_keys[id]) , "2") == 0) ;
	}
	else
	{
		old_flag_modes[id] = new_flag_modes[id] = mc_get_flag_value( config_keys[id]) ;
	}

	widgets[id] = gtk_check_button_new_with_label( _(labels[id])) ;

	if( old_flag_modes[id])
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(widgets[id]) , TRUE) ;
	}

	if( id == MC_FLAG_BIDI || id == MC_FLAG_IND)
	{
		if( widgets[MC_FLAG_BIDI] && widgets[MC_FLAG_IND])
		{
			g_signal_connect( widgets[MC_FLAG_BIDI] , "toggled" ,
				G_CALLBACK(bidi_flag_checked) , widgets[MC_FLAG_IND]) ;
			gtk_widget_set_sensitive( widgets[MC_FLAG_IND] ,
				! gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(widgets[MC_FLAG_BIDI]))) ;
		}
	}

	return widgets[id] ;
}

void
mc_update_flag_mode(
	int id
	)
{
	new_flag_modes[id] = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widgets[id])) ;

	if( old_flag_modes[id] != new_flag_modes[id])
	{
		is_changed[id] = 1 ;
	}

	if( is_changed[id])
	{
		if( id == MC_FLAG_XFT)
		{
			if( new_flag_modes[id])
			{
				mc_set_str_value( config_keys[id] , "xft") ;
			}
			else if( ! gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(widgets[MC_FLAG_CAIRO])))
			{
				mc_set_str_value( config_keys[id] , "xcore") ;
			}
		}
		else if( id == MC_FLAG_CAIRO)
		{
			if( new_flag_modes[id])
			{
				mc_set_str_value( config_keys[id] , "cairo") ;
			}
			else if( ! gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(widgets[MC_FLAG_XFT])))
			{
				mc_set_str_value( config_keys[id] , "xcore") ;
			}
		}
		else if( id == MC_FLAG_AWIDTH)
		{
			if( new_flag_modes[id])
			{
				mc_set_str_value( config_keys[id] , "2") ;
			}
			else
			{
				mc_set_str_value( config_keys[id] , "1") ;
			}
		}
		else
		{
			mc_set_flag_value( config_keys[id] , new_flag_modes[id]) ;
		}

		old_flag_modes[id] = new_flag_modes[id];
	}
}
