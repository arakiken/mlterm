/*
 *	$Id$
 */

#include  "mc_auto_detect.h"

#include  <stdio.h>
#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_locale.h>
#include  <glib.h>
#include  <c_intl.h>
#include  "mc_io.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int new_flag;
static int old_flag;
static GtkWidget *  entry;
static char *old_encodings;
static int is_changed;


/* --- static functions --- */

static char *
get_default_encodings(void)
{
#if  0
	if ((kik_compare_str(kik_get_lang(), "ja")) == 0)
#endif
	{
		return  strdup("UTF-8,EUC-JP,SJIS");
	}
}

static gint
checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		if (*old_encodings == '\0') {
			if ((old_encodings = get_default_encodings())) {
				gtk_entry_set_text(GTK_ENTRY(entry), old_encodings);
			}
		}
		gtk_widget_set_sensitive(entry, TRUE);
		new_flag = 1;
	}
	else
	{
		gtk_widget_set_sensitive(entry, FALSE);
		new_flag = 0;
	}

	return 1;
}


/* -- global functions --- */

GtkWidget *
mc_auto_detect_config_widget_new(void)
{
	GtkWidget *hbox;
	GtkWidget *check;
	GtkWidget *label;

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);

	old_flag = mc_get_flag_value("use_auto_detect");
	check = gtk_check_button_new_with_label(_("Auto detect"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), old_flag) ;
	gtk_widget_show(check);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	label = gtk_label_new(_("Encoding list"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	entry = gtk_entry_new();
	old_encodings = mc_get_str_value("auto_detect_encodings");
	gtk_entry_set_text(GTK_ENTRY(entry), old_encodings);
	gtk_widget_show(entry);
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 1);

	if (!old_flag) gtk_widget_set_sensitive(entry, 0);

	g_signal_connect( check, "toggled", G_CALLBACK(checked), NULL);

	return hbox;
}

void
mc_update_auto_detect(void)
{
	char *  new_encodings;

	new_encodings = strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

	if (old_flag != new_flag ||
	    kik_compare_str(new_encodings, old_encodings) != 0) {
		is_changed = 1;
	}

	if (is_changed) {
		mc_set_flag_value("use_auto_detect", new_flag);
		mc_set_str_value("auto_detect_encodings", new_encodings);
		free(old_encodings);
		old_encodings = new_encodings;
	}
}
