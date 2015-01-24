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
	"use_ctl" ,
	"col_size_of_width_a" ,
	"use_clipboard" ,
	"use_local_echo" ,
	"blink_cursor" ,
	"static_backscroll_mode" ,
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
	N_("Complex Text Layout"),
	N_("Ambiguouswidth = fullwidth"),
	N_("CLIPBOARD Selection"),
	N_("Local echo"),
	N_("Blink cursor"),
	N_("Don't scroll automatically in scrolling back"),
} ;

static GtkWidget *  widgets[MC_FLAG_MODES];


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
