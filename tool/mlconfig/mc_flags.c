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

static int old_flag_mode[MC_FLAG_MODES], new_flag_mode[MC_FLAG_MODES];
static int is_changed[MC_FLAG_MODES];
static char *configname[MC_FLAG_MODES] = {
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
};

static char *label[MC_FLAG_MODES] = {
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
	N_("Local echo")
};

static GtkWidget *widget[MC_FLAG_MODES];

/* --- global functions --- */

GtkWidget * mc_flag_config_widget_new(int id) 
{
	if( id == MC_FLAG_XFT)
	{
		old_flag_mode[id] = new_flag_mode[id] = 
			( strcmp( mc_get_str_value( configname[id]) , "xft") == 0) ;
	}
	else if( id == MC_FLAG_CAIRO)
	{
		old_flag_mode[id] = new_flag_mode[id] = 
			( strcmp( mc_get_str_value( configname[id]) , "cairo") == 0) ;
	}
	else if( id == MC_FLAG_AWIDTH)
	{
		old_flag_mode[id] = new_flag_mode[id] =
			( strcmp( mc_get_str_value( configname[id]) , "2") == 0) ;
	}
	else
	{
		old_flag_mode[id] = new_flag_mode[id] = mc_get_flag_value(configname[id]);
	}
	is_changed[id] = 0;
	widget[id] = gtk_check_button_new_with_label(_(label[id]));
	if (old_flag_mode[id])
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget[id]), TRUE);

	return widget[id];
}

void mc_update_flag_mode(int id)
{
	new_flag_mode[id] = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget[id]));

	if (old_flag_mode[id] != new_flag_mode[id]) is_changed[id] = 1;

	if (is_changed[id]) {
		if( id == MC_FLAG_XFT)
		{
			if( new_flag_mode[id])
			{
				mc_set_str_value( configname[id] , "xft") ;
			}
			else
			{
				mc_set_str_value( configname[id] , "xcore") ;
			}
		}
		else if( id == MC_FLAG_CAIRO)
		{
			if( new_flag_mode[id])
			{
				mc_set_str_value( configname[id] , "cairo") ;
			}
			else
			{
				mc_set_str_value( configname[id] , "xcore") ;
			}
		}
		else if( id == MC_FLAG_AWIDTH)
		{
			if( new_flag_mode[id])
			{
				mc_set_str_value( configname[id] , "2") ;
			}
			else
			{
				mc_set_str_value( configname[id] , "1") ;
			}
		}
		else
		{
			mc_set_flag_value(configname[id], new_flag_mode[id]);
		}
		old_flag_mode[id] = new_flag_mode[id];
	}
}
