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
	MC_FLAG_CONFIG0,
	MC_FLAG_CONFIG1,
	MC_FLAG_CONFIG2,
	MC_FLAG_CONFIG3,
	MC_FLAG_CONFIG4,
	MC_FLAG_CONFIG5,
	MC_FLAG_CONFIG6
};

static char *label[MC_FLAG_MODES] = {
	MC_FLAG_LABEL0,
	MC_FLAG_LABEL1,
	MC_FLAG_LABEL2,
	MC_FLAG_LABEL3,
	MC_FLAG_LABEL4,
	MC_FLAG_LABEL5,
	MC_FLAG_LABEL6
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
