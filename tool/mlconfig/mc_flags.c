/*
 *	$Id$
 */

#include  "mc_flags.h"

#include  <string.h>
#include  <stdlib.h>		/* free */
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
	"use_anti_alias",
	"use_variable_column_width",
	"use_combining",
	"use_dynamic_comb",
	"receive_string_via_ucs",
	"use_multi_column_char",
	"use_bidi"
};

static char *label[MC_FLAG_MODES] = {
	"Anti Alias",
	"Variable column width",
	"Combining",
	"Combining = 1 (or 0) logical column(s)",
	"Process received strings via Unicode",
	"Fullwidth = 2 (or 1) logical column(s)",
	"Bidi (UTF8 only)"
};

static GtkWidget *widget[MC_FLAG_MODES];

/* --- global functions --- */

GtkWidget * mc_flag_config_widget_new(int id) 
{
	old_flag_mode[id] = new_flag_mode[id] = 
		mc_get_flag_value(configname[id]);
	is_changed[id] = 0;
	widget[id] = gtk_check_button_new_with_label(_(label[id]));
	if (old_flag_mode[id])
	    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(widget[id]), TRUE);

	return widget[id];
}

void mc_update_flag_mode(int id) {
	new_flag_mode[id] = GTK_TOGGLE_BUTTON(widget[id])->active;

	if (old_flag_mode[id] != new_flag_mode[id]) is_changed[id] = 1;

	if (is_changed[id]) {
		mc_set_flag_value(configname[id], new_flag_mode[id]);
		old_flag_mode[id] = new_flag_mode[id];
	}
}
