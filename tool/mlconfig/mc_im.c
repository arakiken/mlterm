/*
 *	$Id$
 */

#include "mc_im.h"

#include <string.h>
#include <stdlib.h>		/* free */
#include <kiklib/kik_debug.h>
#include <kiklib/kik_file.h>
#include <kiklib/kik_conf_io.h>
#include <kiklib/kik_map.h>
#include <kiklib/kik_str.h>
#include <glib.h>
#include <c_intl.h>

#include "mc_combo.h"
#include "mc_io.h"


#if 0
#define __DEBUG
#endif

#ifndef SYSCONFDIR
#define CONFIG_PATH "/etc"
#else
#define CONFIG_PATH SYSCONFDIR
#endif

#define STR_LEN 256

KIK_MAP_TYPEDEF(xim_locale, char*, char*);


/* --- static variables --- */

static enum {
	IM_XIM ,
	IM_UIM ,
	IM_IIIMF ,
} im_type = IM_XIM;

static char **xims;
static char **locales;
static u_int num_of_xims;

static int is_changed = 0;

static char xim_auto_str[STR_LEN];
static char current_locale_str[STR_LEN];
static char uim_auto_str[] = N_("default");
static char selected_xim_name[STR_LEN];
static char selected_xim_locale[STR_LEN];
static char selected_uim_engine[STR_LEN];

/* --- static functions --- */

/*
 * xim
 */

static gint
locale_changed(GtkWidget *widget, gpointer data)
{
	is_changed = 1;

	snprintf(selected_xim_locale, STR_LEN,
		 gtk_entry_get_text(GTK_ENTRY(widget)));

	return 1;
}

static char *
get_xim_locale(char *xim)
{
	int count;
	
	for(count = 0; count < num_of_xims; count ++) {
		if( strcmp(xims[count], xim) == 0) {
			return  locales[count];
		}
	}

	return NULL;
}

static gint
xim_selected(GtkWidget *widget, gpointer data)
{
	char *locale;

	snprintf(selected_xim_name, STR_LEN,
		 gtk_entry_get_text(GTK_ENTRY(widget)));

	if ((locale = get_xim_locale(selected_xim_name))) {
		gtk_entry_set_text(GTK_ENTRY(data) , locale);
	} else {
		gtk_entry_set_text(GTK_ENTRY(data) , current_locale_str);
	}

	snprintf(selected_xim_locale, STR_LEN,
		 gtk_entry_get_text(GTK_ENTRY(data)));

	is_changed = 1;

	return 1;
}

static int
xim_read_conf(KIK_MAP(xim_locale) xim_locale_table, char *filename)
{
	kik_file_t *from;
	char *key;
	char *value;
	KIK_PAIR(xim_locale) pair;
	int result;

	if (!(from = kik_file_open(filename, "r"))) {
#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n",
				 filename);
#endif
	
		return 0;
	}

	while (kik_conf_io_read(from, &key, &value)) {

		kik_map_get(result, xim_locale_table, key, pair);

		if (result) {
			free(pair->value);
			pair->value = strdup(value);
		} else {
			key = strdup(key);
			value = strdup(value);
			
			kik_map_set(result, xim_locale_table, key, value);
		}
	}

	kik_file_close(from);
	
	return 1;
}

static GtkWidget *
xim_widget_new(const char *xim_name, const char *xim_locale)
{
	KIK_MAP(xim_locale) xim_locale_table;
	KIK_PAIR(xim_locale) *array;
	u_int size;
	char *rcpath;
	char *default_xim_name;
	char *cur_locale;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *combo;
	int count;

	default_xim_name = mc_get_str_value("default_xim_name");
	snprintf(xim_auto_str, STR_LEN, _("auto (currently %s)"),
		 default_xim_name);
	free(default_xim_name);

	/*
	 * create known xim list from <prefix>/etc/mlterm/xim and/or
	 * ~/.mlterm/xim.
	 */

	kik_map_new(char *, char *, xim_locale_table, kik_map_hash_str,
		    kik_map_compare_str);

	kik_set_sys_conf_dir(CONFIG_PATH);
	
	if ((rcpath = kik_get_sys_rc_path("mlterm/xim"))) {
		xim_read_conf(xim_locale_table, rcpath);
		free(rcpath);
	}

	if ((rcpath = kik_get_user_rc_path("mlterm/xim"))) {
		xim_read_conf(xim_locale_table, rcpath);
		free(rcpath);
	}

	kik_map_get_pairs_array(xim_locale_table, array, size);

	if ((xims = malloc(sizeof(char*) * (size + 1))) == NULL)
		return NULL;

	if ((locales = malloc(sizeof(char*) * (size + 1))) == NULL) {
		free(xims);
		return NULL;
	}

	for (count = 0; count < size; count++) {
	    xims[count] = array[count]->key;
	    locales[count] = array[count]->value;
	}

	xims[count] = xim_auto_str;
	locales[count] = NULL;

	num_of_xims = size + 1;

	kik_map_delete(xim_locale_table);

	/*
	 * create widgets
	 */

	vbox = gtk_vbox_new(FALSE, 5);

	cur_locale = mc_get_str_value("locale");
	snprintf(current_locale_str, STR_LEN, _("auto (currently %s)"),
		 cur_locale);
	free(cur_locale);
	entry = gtk_entry_new();
	snprintf(selected_xim_locale, STR_LEN,
		 xim_locale ? xim_locale : current_locale_str);
	gtk_entry_set_text(GTK_ENTRY(entry), selected_xim_locale);

	snprintf(selected_xim_name, STR_LEN,
		 xim_name ? xim_name : xim_auto_str);
	combo = mc_combo_new(_("XIM Server"), xims, num_of_xims,
			     selected_xim_name, 0, xim_selected, entry);

	label = gtk_label_new(_("XIM locale"));

	hbox = gtk_hbox_new(FALSE, 5);

	gtk_box_pack_start(GTK_BOX(vbox), combo, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	gtk_widget_show(combo);
	gtk_widget_show(entry);
	gtk_widget_show(label);
	gtk_widget_show(hbox);

	return vbox;
}


/*
 * uim
 */
static gint
uim_selected(GtkWidget *widget, gpointer data)
{
	is_changed = 1;

	snprintf(selected_uim_engine, STR_LEN,
		 gtk_entry_get_text(GTK_ENTRY(widget)));

	return 1;
}

char **
uim_read_conf(char **engines, u_int *num_of_engines, char *path)
{
	kik_file_t *from;
	char *line;
	size_t len;

	if (!(from = kik_file_open(path, "r"))) {
#ifdef DEBUG
		kik_warn_printf(KIK_DEBUG_TAG " Could not open %s.\n", path);
#endif
		return engines;
	}

	while (1) {
		if (!(line = kik_file_get_line(from, &len))) {
			break;
		}

		if(*line == '#' || *line == '\n') { /* comment or empty line */
			continue;
		}

		line[len-1] = '\0';

		line = kik_str_chop_spaces(line);

		if (!(engines = realloc(engines, sizeof(char*) * (*num_of_engines+1)))) {
			break;
		}

		engines[*num_of_engines] = strdup(line);
		(*num_of_engines)++;
	}

	kik_file_close(from);

	return engines;
}

static GtkWidget *
uim_widget_new(const char *uim_engine)
{
	GtkWidget *combo;
	char *rcpath;
	char **engines = NULL;
	u_int num_of_engines = 0;
	int i;

	if ((rcpath = kik_get_sys_rc_path("mlterm/uim"))) {
		engines = uim_read_conf(engines, &num_of_engines, rcpath);
		free(rcpath);
	}

	if ((rcpath = kik_get_user_rc_path("mlterm/uim"))) {
		engines = uim_read_conf(engines, &num_of_engines, rcpath);
		free(rcpath);
	}

	snprintf(selected_uim_engine, STR_LEN,
		 uim_engine ? uim_engine : uim_auto_str);

	combo = mc_combo_new(_("Conversion engine"), engines, num_of_engines,
			     selected_uim_engine , 0, uim_selected, NULL);

	for (i = 0; i < num_of_engines; i++) {
		free(engines[i]);
	}
	free(engines);

	return combo;
}

/*
 * callbacks for radio buttons of im type.
 */
static gint
button_xim_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_show(GTK_WIDGET(data));
		im_type = IM_XIM;
	} else {
		gtk_widget_hide(GTK_WIDGET(data));
	}

	is_changed = 1;

	return 1;
}

static gint
button_uim_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_show(GTK_WIDGET(data));
		im_type = IM_UIM;
	} else {
		gtk_widget_hide(GTK_WIDGET(data));
	}

	is_changed = 1;

	return 1;
}

/* --- global functions --- */

GtkWidget *
mc_im_config_widget_new(void)
{
	char *xim_name = NULL;
	char *xim_locale = NULL;
	char *uim_engine = NULL;
	char *value;
	char *p;
	GtkWidget *xim;
	GtkWidget *uim;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *radio;
	GSList *group;

	value = mc_get_str_value("input_method");

	p = kik_str_sep(&value, ":");

	if (strncmp(p, "xim", 3) == 0) {
		im_type = IM_XIM;
		xim_name = kik_str_sep(&value, ":");
		xim_locale = kik_str_sep(&value, ":");
	} else if (strncmp(p, "uim", 3) == 0) {
		im_type = IM_UIM;
		uim_engine = value;
	} else {
		im_type = IM_XIM;
	}

	xim = xim_widget_new(xim_name, xim_locale);
	uim = uim_widget_new(uim_engine);

	free(p);

	frame = gtk_frame_new(_("Input Method"));

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	radio = gtk_radio_button_new_with_label(NULL, "XIM");
	gtk_signal_connect(GTK_OBJECT(radio), "toggled",
			   GTK_SIGNAL_FUNC(button_xim_checked), xim);
	gtk_widget_show(GTK_WIDGET(radio));
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
	if (im_type == IM_XIM)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(radio) , TRUE);

	group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio));
	radio = gtk_radio_button_new_with_label(group, "uim");
	gtk_signal_connect(GTK_OBJECT(radio), "toggled",
			   GTK_SIGNAL_FUNC(button_uim_checked), uim);
	gtk_widget_show(GTK_WIDGET(radio));
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
#ifndef USE_UIM
	gtk_widget_set_sensitive(radio, 0);
	gtk_widget_set_sensitive(uim, 0);
#endif
	if (im_type == IM_UIM)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(radio) , TRUE);

	gtk_widget_show(GTK_WIDGET(hbox));
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	switch (im_type) {
	case IM_XIM:
		gtk_widget_show(xim);
		gtk_widget_hide(uim);
		break;
	case IM_UIM:
		gtk_widget_hide(xim);
		gtk_widget_show(uim);
		break;
	case IM_IIIMF:
	default:
		break;
	}

	gtk_box_pack_start(GTK_BOX(vbox), xim, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), uim, TRUE, TRUE, 0);

	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	return frame;
}

void
mc_update_im(void)
{
	char *p;
	size_t len;

	if (!is_changed) return;

	switch (im_type) {
	case IM_XIM:
		if (strcmp(selected_xim_name, xim_auto_str) == 0) {
			p = strdup("xim");
		} else if (strcmp(selected_xim_locale, current_locale_str) == 0) {
			len = 3 + 1 + strlen(selected_xim_name) + 1;
			if (!(p = malloc(sizeof(char) * len))) return;
			sprintf(p, "xim:%s", selected_xim_name);
		} else {
			len = 3 + 1 + strlen(selected_xim_name) + 1 + strlen(selected_xim_locale) + 1;
			if (!(p = malloc(sizeof(char) * len))) return;
			sprintf(p, "xim:%s:%s", selected_xim_name, selected_xim_locale);
		}
		break;
	case IM_UIM:
		if (strcmp(selected_uim_engine, uim_auto_str) == 0) {
			p = strdup("uim");
		} else {
			len = 3 + 1 + strlen(selected_uim_engine) + 1;
			if(!(p = malloc(sizeof(char) * len))) return;
			sprintf(p, "uim:%s", selected_uim_engine);
		}
		break;
	case IM_IIIMF:
	default:
		break;
	}

	mc_set_str_value("input_method", p);

	free(p);

	is_changed = 0;
}

