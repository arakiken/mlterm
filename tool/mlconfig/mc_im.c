/*
 *	$Id$
 */

#include "mc_im.h"

#include <kiklib/kik_mem.h>		/* free */
#include <kiklib/kik_debug.h>
#include <kiklib/kik_file.h>
#include <kiklib/kik_conf_io.h>
#include <kiklib/kik_dlfcn.h>
#include <kiklib/kik_map.h>
#include <kiklib/kik_str.h>
#include <kiklib/kik_config.h>		/* USE_WIN32API */
#include <glib.h>
#include <c_intl.h>
#include <dirent.h>
#include <im_info.h>

#include "mc_combo.h"
#include "mc_io.h"


#if 0
#define __DEBUG
#endif

#if  defined(USE_WIN32API)
#define CONFIG_PATH "."
#elif  defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif

#ifndef LIBDIR
#define  IM_DIR "/usr/local/lib/mlterm/"
#else
#define  IM_DIR LIBDIR "/mlterm/"
#endif

#define STR_LEN 256

KIK_MAP_TYPEDEF(xim_locale, char*, char*);

#define IM_MAX  20

typedef enum im_type {
	IM_NONE = 0,
	IM_XIM = 1,
	IM_OTHER = 2,

	IM_TYPE_MAX = IM_MAX

} im_type_t ;

typedef im_info_t* (*im_get_info_func_t)(char *, char *);


/* --- static variables --- */

static im_type_t im_type;
static im_type_t cur_im_type;

static char **xims;
static char **locales;
static u_int num_of_xims;

static int is_changed = 0;

static char xim_auto_str[STR_LEN] = "";
static char current_locale_str[STR_LEN] = "";
static char selected_xim_name[STR_LEN] = "";
static char selected_xim_locale[STR_LEN] = "";

static im_info_t *im_info_table[IM_MAX];
static u_int  num_of_info = 0;
static im_info_t *selected_im = NULL;
static int selected_im_arg = -1 ;

static GtkWidget *im_opt_widget[IM_MAX];

/* --- static functions --- */

static int
is_im_plugin(char *file_name)
{
	if (kik_dl_is_module(file_name) && strstr(file_name, "im-"))
	{
		return 1;
	}

	return 0;
}

static int
get_im_info(char *locale, char *encoding)
{
	DIR *dir;
	struct dirent *d;

	if (!(dir = opendir(IM_DIR))) return 0;

	while ( (d = readdir(dir)) ) {
		kik_dl_handle_t handle;
		im_get_info_func_t func;
		im_info_t *info;
		char symname[100];
		char *p;

		if (d->d_name[0] == '.' || !is_im_plugin(d->d_name)) continue;

		/* libim-foo.so -> libim-foo */
		if (!(p = strchr(d->d_name, '.'))) continue;
		*p = '\0' ;

		/* libim-foo -> im-foo */
		if (!(p = strstr(d->d_name, "im-"))) continue;

		if (!(handle = kik_dl_open(IM_DIR , p))) continue;

		snprintf(symname, 100, "im_%s_get_info", &p[3]);

		func = (im_get_info_func_t)kik_dl_func_symbol(handle , symname);
		if (!func) {
			kik_dl_close(handle);
			continue;
		}

		info = (*func)(locale, encoding);

		if(info) {
			im_info_table[num_of_info] = info ;
			num_of_info++;
		}

		kik_dl_close(handle);
	}

	return 0;
}


/*
 * XIM
 */

static char *
get_xim_locale(char *xim)
{
	int count;
	
	for (count = 0; count < num_of_xims; count ++) {
		if (strcmp(xims[count], xim) == 0) {
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
		kik_warn_printf(KIK_DEBUG_TAG " %s couldn't be opened.\n",
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
 * pluggable ims
 */

static gint
im_selected(GtkWidget *widget, gpointer data)
{
	const char *str;
	int i;

	if (selected_im == NULL)
		return 1;

	str = (const char*)gtk_entry_get_text(GTK_ENTRY(widget));

	for (i = 0; i < selected_im->num_of_args; i++)
		if (strcmp(selected_im->readable_args[i], str) == 0)
			selected_im_arg = i;

	is_changed = 1;

	return 1;
}

static GtkWidget *
im_widget_new(int nth_im, const char *value, char *locale)
{
	GtkWidget *combo;
	im_info_t *info;
	int i;
	int selected = 0;
	size_t len;

	info = im_info_table[nth_im];

	if (value) {
		for (i = 1; i < info->num_of_args; i++) {
			if (strcmp(info->args[i], value) == 0) {
				selected = i;
			}
		}
	}

	if (!info->num_of_args)
		return NULL;

	if (!value || (value && selected)) {
		char *auto_str;

		/*
		 * replace gettextized string
		 */
		len = strlen(_("auto (currently %s)")) +
		      strlen(info->readable_args[0]) + 1;

		if ((auto_str = malloc(len))) {
			snprintf(auto_str, len, _("auto (currently %s)"),
				 info->readable_args[0]);
			free(info->readable_args[0]);
			info->readable_args[0] = auto_str;
		}
	} else {
		free(info->readable_args[0]);
		info->readable_args[0] = strdup(value);
	}

	combo = mc_combo_new(_("Option"), info->readable_args,
			     info->num_of_args, info->readable_args[selected],
			     1, im_selected, NULL);

	return combo;
}

/*
 * callbacks for radio buttons of im type.
 */
static gint
button_xim_checked(GtkWidget *widget, gpointer data)
{
	if(gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget))) {
		gtk_widget_show(GTK_WIDGET(data));
		im_type = IM_XIM;
	} else {
		gtk_widget_hide(GTK_WIDGET(data));
	}

	is_changed = 1;

	return 1;
}

static gint
button_im_checked(GtkWidget *widget, gpointer  data)
{
	int i;
	int idx = 0;

	if (data == NULL || num_of_info == 0) {
		if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget))) {
			im_type = IM_NONE;
		}
	} else {
		for (i = 0; i < num_of_info; i++)
			if (im_info_table[i] == data)
				idx = i;

		if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget))) {
			im_type = IM_OTHER + idx;
			selected_im = data;
			if (im_opt_widget[idx])
				gtk_widget_show(GTK_WIDGET(im_opt_widget[idx]));
		} else {
			if (im_opt_widget[idx])
				gtk_widget_hide(GTK_WIDGET(im_opt_widget[idx]));
		}
	}

	is_changed = 1;

	return 1;
}


/* --- global functions --- */

GtkWidget *
mc_im_config_widget_new(void)
{
	char *cur_locale = NULL;
	char *encoding = NULL;
	char *xim_name = NULL;
	char *xim_locale = NULL;
	char *value;
	char *p;
	int i;
	int index = -1;
	GtkWidget *xim;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *radio;
	GSList *group;

	cur_locale = mc_get_str_value("locale");
	encoding = mc_get_str_value("encoding");

	get_im_info(cur_locale , encoding);

	value = mc_get_str_value("input_method");

	p = kik_str_sep(&value, ":");

	im_type = IM_NONE ;
	if (strncmp(p, "xim", 3) == 0) {
		im_type = IM_XIM;
		xim_name = kik_str_sep(&value, ":");
		xim_locale = kik_str_sep(&value, ":");
	} else if (strncmp(p, "none", 4) == 0) {
		/* do nothing */
	} else {
		for (i = 0; i < num_of_info; i++) {
			if (strcmp(p, im_info_table[i]->id) == 0) {
				index = i;
				im_type = IM_OTHER + i;
				break;
			}
		}
	}
	cur_im_type = im_type ;

	xim = xim_widget_new(xim_name, xim_locale, cur_locale);
	for (i = 0; i < num_of_info; i++) {
		if (index == i)
			im_opt_widget[i] = im_widget_new(i, value, cur_locale);
		else
			im_opt_widget[i] = im_widget_new(i, NULL, cur_locale);
	}

	free(cur_locale);
	free(encoding);

	free(p);

	frame = gtk_frame_new(_("Input Method"));

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	radio = gtk_radio_button_new_with_label(NULL, "XIM");
	g_signal_connect(radio, "toggled",
			   G_CALLBACK(button_xim_checked), xim);
	gtk_widget_show(GTK_WIDGET(radio));
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
	if (im_type == IM_XIM)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio) , TRUE);

	for (i = 0; i < num_of_info; i++) {
		group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
		radio = gtk_radio_button_new_with_label(group,
							im_info_table[i]->name);
		g_signal_connect(radio, "toggled",
				   G_CALLBACK(button_im_checked),
				   im_info_table[i]);
		gtk_widget_show(GTK_WIDGET(radio));
		gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
		if (im_type >= IM_OTHER && index == i)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio) ,
						    TRUE);
	}

	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
	radio = gtk_radio_button_new_with_label(group, _("None"));
	g_signal_connect(radio, "toggled",
			   G_CALLBACK(button_im_checked), NULL);
	gtk_widget_show(GTK_WIDGET(radio));
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
	if (im_type == IM_NONE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio) , TRUE);

	gtk_widget_show(GTK_WIDGET(hbox));
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	switch (im_type) {
	case IM_XIM:
		gtk_widget_show(xim);
		for (i = 0; i < num_of_info; i++)
			if (im_opt_widget[i])
				gtk_widget_hide(im_opt_widget[i]);
		break;
	case IM_NONE:
		gtk_widget_hide(xim);
		for (i = 0; i < num_of_info; i++)
			if (im_opt_widget[i])
				gtk_widget_hide(im_opt_widget[i]);
		break;
	default: /* OTHER */
		gtk_widget_hide(xim);
		for (i = 0; i < num_of_info; i++) {
			if (!im_opt_widget[i])
				continue;

			if (i == index)
				gtk_widget_show(im_opt_widget[i]);
			else
				gtk_widget_hide(im_opt_widget[i]);
		}
		break;
	}

	gtk_box_pack_start(GTK_BOX(vbox), xim, TRUE, TRUE, 0);
	for (i = 0; i < num_of_info; i++)
		if (im_opt_widget[i])
			gtk_box_pack_start(GTK_BOX(vbox), im_opt_widget[i],
					   TRUE, TRUE, 0);

	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	is_changed = 0 ;

	return frame;
}

void
mc_update_im(void)
{
	char *p;
	size_t len;

	if (!is_changed)
		return;

	if (im_type == cur_im_type && selected_im_arg == -1) {
		is_changed = 0;
		return;
	}

	if (selected_im_arg == -1) {
		is_changed = 0;
		selected_im_arg = 0 ;
	}

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
	case IM_NONE:
		p = strdup("none");
		break;
	/* case IM_OTHER: */
	default:
		if (selected_im == NULL) return;
		if (selected_im_arg == 0) { /* auto */
			p = strdup(selected_im->id);
		} else {
			len = strlen(selected_im->id) +
			      strlen(selected_im->args[selected_im_arg]) + 2;
			if(!(p = malloc(len))) return;
			sprintf(p, "%s:%s", selected_im->id ,
				selected_im->args[selected_im_arg]);
		}
		break;
	}

	mc_set_str_value("input_method", p);

	selected_im_arg = -1;
	cur_im_type = im_type;
	is_changed = 0;

	free(p);
}

