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

#ifdef USE_UIM
#include <uim.h>
#endif

#ifdef USE_IIIMF
#define HAVE_STDINT_H 1	/* FIXME */
#include <iiimcf.h>
#endif

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
	IM_NONE ,
} im_type = IM_NONE;

static char **xims;
static char **locales;
static u_int num_of_xims;

static int is_changed = 0;

static char xim_auto_str[STR_LEN] = "";
static char uim_auto_str[STR_LEN] = "";
static char iiimf_auto_str[STR_LEN] = "";
static char current_locale_str[STR_LEN] = "";
static char selected_xim_name[STR_LEN] = "";
static char selected_xim_locale[STR_LEN] = "";
static char selected_uim_engine[STR_LEN] = "";
static char selected_iiimf_lang[STR_LEN] = "";
static char original_iiimf_lang[STR_LEN] = "";

/* --- static functions --- */

/*
 * XIM
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
read_xim_conf(KIK_MAP(xim_locale) xim_locale_table, char *filename)
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
xim_widget_new(const char *xim_name, const char *xim_locale, const char *cur_locale)
{
	KIK_MAP(xim_locale) xim_locale_table;
	KIK_PAIR(xim_locale) *array;
	u_int size;
	char *rcpath;
	char *default_xim_name;
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
		read_xim_conf(xim_locale_table, rcpath);
		free(rcpath);
	}

	if ((rcpath = kik_get_user_rc_path("mlterm/xim"))) {
		read_xim_conf(xim_locale_table, rcpath);
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

	snprintf(current_locale_str, STR_LEN, _("auto (currently %s)"),
		 cur_locale);
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
#ifdef USE_UIM
static gint
uim_selected(GtkWidget *widget, gpointer data)
{
	const char *str;
	char *engine_name;
	char *p;

	str = (const char*)gtk_entry_get_text(GTK_ENTRY(widget));

	snprintf(selected_uim_engine, STR_LEN, str);

	if (strcmp(selected_uim_engine, uim_auto_str)) {
		/* "anthy (ja)" -> "anthy" */
		p = strstr(selected_uim_engine, " (");
		if(p) *p = '\0';
	}

	is_changed = 1;

	return 1;
}
#endif

static GtkWidget *
uim_widget_new(const char *uim_engine, const char *cur_locale)
{
#ifdef USE_UIM
	GtkWidget *combo;
	uim_context context;
	char **engines = NULL;
	int num_of_engines;
	int i;
	int selected_index = -1;

	uim_init();

	context = uim_create_context(NULL, "UTF-8", NULL, NULL, NULL, NULL);

	if (context) {
		const char *engine_name;
		const char *language;
		size_t len;

		num_of_engines = uim_get_nr_im(context);

		engines = alloca(sizeof(char*) * num_of_engines + 1);

		for (i = 0; i < num_of_engines && engines; i++) {

			engine_name = uim_get_im_name(context, i);
			language = uim_get_im_language(context, i);

			/* format: "engine_name (language)" */
			len = strlen(engine_name) + strlen(language) + 3 + 1;

			engines[i + 1] = alloca(sizeof(char) * len);

			snprintf(engines[i + 1], len, "%s (%s)",
				 engine_name, language);

			if (uim_engine) {
				if (strcmp(engine_name, uim_engine) == 0) {
					selected_index = i + 1;
					snprintf(selected_uim_engine, STR_LEN,
						 engine_name);
				}
			}
		}
	}

	if (selected_index == -1) {
		snprintf(uim_auto_str, STR_LEN, _("auto (currently %s)"),
	#ifdef UIM_CAN_GET_DEFAULT_IM
			  uim_get_default_im_name(cur_locale));
	#else
			  "default (*)");
	#endif
		selected_index = 0;
		snprintf(selected_uim_engine, STR_LEN, uim_auto_str);
	} else {
		snprintf( uim_auto_str, STR_LEN, _("auto"));
	}
	engines[0] = strdup(uim_auto_str);

	uim_quit();

	combo = mc_combo_new(_("Conversion engine"), engines, num_of_engines,
			     engines[selected_index], 1, uim_selected, NULL);

	return combo;
#else
	return gtk_label_new(_("uim is disabled."));
#endif
}

/*
 * IIIMF
 */

#ifdef USE_IIIMF
static void
iiimf_set_item(char **items, char *str1, char* str2, int index)
{
	size_t len;

	if (str2) { /* "str1 (str2)" */
		len = strlen(str1) + strlen(str2) + 4;
		if ((items[index] = malloc(sizeof(char) * len)) == NULL) {
			items[index] = NULL;
			return;
		}
		snprintf(items[index], len, "%s (%s)", str1, str2);
	} else { /* "str1" */
		len = strlen(str1) + 1;
		if ((items[index] = malloc(sizeof(char) * len)) == NULL) {
			items[index] = NULL;
			return;
		}
		snprintf(items[index], len, str1);
	}

	return;
}

static gint
iiimf_selected(GtkWidget *widget, gpointer data)
{
	const char *str;
	char *engine_name;
	char *p;

	str = (const char*)gtk_entry_get_text(GTK_ENTRY(widget));

	if (!strlen(str))
		return 1;

	snprintf(selected_iiimf_lang, STR_LEN, str);

	if (strcmp(selected_iiimf_lang, iiimf_auto_str)) {
		/* "ja (CannaLE)" -> "ja:CannaLE" */
		p = strstr(selected_iiimf_lang, " (");
		snprintf(p, STR_LEN - 5, ":%s", p + 2);
		p = strstr(selected_iiimf_lang, ")");
		*p = '\0';
	}

	is_changed = 1;

	return 1;
}

static int
iiimf_best_match_index(const char **items,
		       int num,
		       const char *lang_id,
		       const char *le_name,
		       const char *cur_locale)
{
	char buf[STR_LEN];
	int i;
	int result = 0;
	int best_score = 0;

	snprintf(buf, STR_LEN, cur_locale);

	if (lang_id == NULL && le_name == NULL) {
		char *p;
		if (!(p = strstr(buf, ".")))
			return 0;
		*p = '\0';
		lang_id = buf;
	}

	for (i = 0; i < num; i++) {
		int score = 0;

		if (!items[i]) continue;

		if (lang_id) {
			if (strlen(lang_id) >= 2 &&
			    strncmp(items[i], lang_id, 2) == 0)
				score++;
			if (strlen(lang_id) >= 5 &&
			    strncmp(items[i], lang_id, 5) == 0)
				score++;
			if (strlen(lang_id) >= 11 &&
			    strncmp(items[i], lang_id, 11) == 0)
				score++;
		}

		if (le_name && strlen(le_name)) {
			char *p;
			if ((p = strstr(items[i], " ("))) {
				p+=2;
				if (strncmp(p, le_name, strlen(le_name)) == 0)
					score++;
			}
		}

		if (score > best_score) {
			best_score = score;
			result = i;
		}
	}

	return result;
}
#endif

static GtkWidget *
iiimf_widget_new(const char *iiimf_lang_id, const char *iiimf_le, const char *cur_locale)
{
#ifdef USE_IIIMF
#  if GLIB_MAJOR_VERSION >= 2
	GtkWidget *combo;
	const char **items = NULL;
	IIIMCF_handle handle = NULL ;
	IIIMCF_input_method *input_methods;
	IIIMCF_language *langs;
	int num_im, num_lang, num_total = 0;
	int selected_index;
	int i, j;
	char *utf8 = NULL;

	if (iiimf_lang_id && strlen(iiimf_lang_id) == 0)
		iiimf_lang_id = NULL;
	if (iiimf_le && strlen(iiimf_le) == 0)
		iiimf_le = NULL;

	if (iiimcf_initialize(IIIMCF_ATTR_NULL) != IIIMF_STATUS_SUCCESS)
		return gtk_label_new(_("IIIMCF initialization failed."));

	/*
	 * TODO: cleanup!!
	 */
	if (iiimcf_create_handle(IIIMCF_ATTR_NULL,
				 &handle) != IIIMF_STATUS_SUCCESS)
		goto error;

	if (iiimcf_get_supported_input_methods(
					handle,
					&num_im,
					&input_methods) != IIIMF_STATUS_SUCCESS)
		goto error;

	for (i = 0; i < num_im; i++) {
		const IIIMP_card16 *im_id, *im_hrn, *im_domain;
		char **p;

		if (iiimcf_get_input_method_desc(
					input_methods[i],
					&im_id, &im_hrn, &im_domain)
						!= IIIMF_STATUS_SUCCESS)
			goto error;

		if (iiimcf_get_input_method_languages(
						input_methods[i],
						&num_lang, &langs)
							!= IIIMF_STATUS_SUCCESS)
			goto error;

		p = realloc(items, sizeof(char*) * (num_total + num_lang));
		if (!p) goto error;

		items = p;

		utf8 = g_utf16_to_utf8(im_id, -1, NULL, NULL, NULL);
		if (!utf8) goto error;

		for (j = 0; j < num_lang; j++) {
			const char *lang_id;
			iiimcf_get_language_id(langs[j], &lang_id);
			iiimf_set_item(items, lang_id, utf8, i + j);
			num_total++;
		}

		free(utf8);
		utf8 = NULL;
	}

	selected_index = iiimf_best_match_index(items, num_total,
						iiimf_lang_id, iiimf_le,
						cur_locale);

	if (iiimf_lang_id == NULL && iiimf_le == NULL) {
		snprintf(iiimf_auto_str, STR_LEN, _("auto (currently %s)"),
			 items[selected_index]);
		iiimf_set_item(items, iiimf_auto_str, NULL, num_total);
		selected_index = num_total;
		snprintf(original_iiimf_lang, STR_LEN, "");
	} else if (iiimf_lang_id == NULL) {
		snprintf(iiimf_auto_str, STR_LEN, "  (%s)", iiimf_le);
		iiimf_set_item(items, iiimf_auto_str, NULL, num_total);
		selected_index = num_total;
		snprintf(original_iiimf_lang, STR_LEN, "::%s", iiimf_le);
	} else if (iiimf_le == NULL) {
		snprintf(iiimf_auto_str, STR_LEN, "%s",
			 iiimf_lang_id);
		iiimf_set_item(items, iiimf_auto_str, NULL, num_total);
		selected_index = num_total;
		snprintf(original_iiimf_lang, STR_LEN, ":%s", iiimf_lang_id);
	} else {
		snprintf(iiimf_auto_str, STR_LEN, _("auto"), iiimf_lang_id);
		iiimf_set_item(items, iiimf_auto_str, NULL, num_total);
		snprintf(original_iiimf_lang, STR_LEN, ":%s:%s",
			 iiimf_lang_id, iiimf_le);
	}
	num_total++;

	combo = mc_combo_new(_("Language (Language engine)"), items, num_total,
			     items[selected_index], 1, iiimf_selected, NULL);

	iiimcf_destroy_handle(handle);
	iiimcf_finalize();

	if (items) {
		for (i = 0; i < num_total; i++) {
			if (items[i]) free(items[i]);
		}
		free(items);
	}

	return combo;

error:
	if (utf8)
		free(utf8);

	if (handle)
		iiimcf_destroy_handle(handle);

	iiimcf_finalize();

	if (items) {
		for (i = 0; i < num_total; i++) {
			if (items[i]) free(items[i]);
		}
		free(items);
	}

	return gtk_label_new(_("Cound not create language list."));
#  else
	return gtk_label_new(_("Language selector depends on glib-2.0 or later."));
#  endif
#else
	return gtk_label_new(_("IIIMF is disabled."));
#endif
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

static gint
button_iiimf_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_show(GTK_WIDGET(data));
		im_type = IM_IIIMF;
	} else {
		gtk_widget_hide(GTK_WIDGET(data));
	}

	is_changed = 1;

	return 1;
}

static gint
button_none_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if(GTK_TOGGLE_BUTTON(widget)->active)
		im_type = IM_NONE;

	is_changed = 1;

	return 1;
}

/* --- global functions --- */

GtkWidget *
mc_im_config_widget_new(void)
{
	char *cur_locale = NULL;
	char *xim_name = NULL;
	char *xim_locale = NULL;
	char *uim_engine = NULL;
	char *iiimf_lang_id = NULL;
	char *iiimf_le = NULL;
	char *value;
	char *p;
	GtkWidget *xim;
	GtkWidget *uim;
	GtkWidget *iiimf;
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
	} else if (strncmp(p, "iiimf", 5) == 0) {
		im_type = IM_IIIMF;
		iiimf_lang_id = kik_str_sep(&value, ":");
		iiimf_le = kik_str_sep(&value, ":");
	} else if (strncmp(p, "none", 4) == 0) {
		im_type = IM_NONE;
	} else {
		im_type = IM_NONE;
	}

	cur_locale = mc_get_str_value("locale");

	xim = xim_widget_new(xim_name, xim_locale, cur_locale);
	uim = uim_widget_new(uim_engine, cur_locale);
	iiimf = iiimf_widget_new(iiimf_lang_id, iiimf_le, cur_locale);

	free(cur_locale);

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

	group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio));
	radio = gtk_radio_button_new_with_label(group, "IIIMF");
	gtk_signal_connect(GTK_OBJECT(radio), "toggled",
			   GTK_SIGNAL_FUNC(button_iiimf_checked), iiimf);
	gtk_widget_show(GTK_WIDGET(radio));
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
#ifndef USE_IIIMF
	gtk_widget_set_sensitive(radio, 0);
	gtk_widget_set_sensitive(iiimf, 0);
#endif
	if (im_type == IM_IIIMF)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(radio) , TRUE);

	group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio));
	radio = gtk_radio_button_new_with_label(group, _("None"));
	gtk_signal_connect(GTK_OBJECT(radio), "toggled",
			   GTK_SIGNAL_FUNC(button_none_checked), NULL);
	gtk_widget_show(GTK_WIDGET(radio));
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
	if (im_type == IM_NONE)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(radio) , TRUE);

	gtk_widget_show(GTK_WIDGET(hbox));
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	switch (im_type) {
	case IM_XIM:
		gtk_widget_show(xim);
		gtk_widget_hide(uim);
		gtk_widget_hide(iiimf);
		break;
	case IM_UIM:
		gtk_widget_hide(xim);
		gtk_widget_show(uim);
		gtk_widget_hide(iiimf);
		break;
	case IM_IIIMF:
		gtk_widget_hide(xim);
		gtk_widget_hide(uim);
		gtk_widget_show(iiimf);
		break;
	case IM_NONE:
		gtk_widget_hide(xim);
		gtk_widget_hide(uim);
		gtk_widget_hide(iiimf);
		break;
	default:
		break;
	}

	gtk_box_pack_start(GTK_BOX(vbox), xim, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), uim, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), iiimf, TRUE, TRUE, 0);

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
		if (strcmp(selected_iiimf_lang, iiimf_auto_str) == 0) {
			len = 3 + 1 + strlen(original_iiimf_lang) + 1;
			if(!(p = malloc(sizeof(char) * len))) return;
			sprintf(p, "iiimf%s", original_iiimf_lang);
		} else {
			len = 5 + 1 + strlen(selected_iiimf_lang) + 1;
			if(!(p = malloc(sizeof(char) * len))) return;
			sprintf(p, "iiimf:%s", selected_iiimf_lang);
		}
		break;
	case IM_NONE:
		p = strdup("none");
		break;
	default:
		break;
	}

	mc_set_str_value("input_method", p);

	free(p);

	is_changed = 0;
}

