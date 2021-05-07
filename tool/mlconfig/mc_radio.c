/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_radio.h"

#include <pobl/bl_str.h>
#include <pobl/bl_mem.h> /* free */
#include <pobl/bl_debug.h>
#include <glib.h>
#include <c_intl.h>

#include "mc_compat.h"
#include "mc_io.h"

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static int new_values[MC_RADIOS];
static int old_values[MC_RADIOS];
static int is_changed[MC_RADIOS];
static void (*funcs[MC_RADIOS])(void);

static char *config_keys[MC_RADIOS] = {
    "mod_meta_mode",    "bel_mode",    "scrollbar_mode", "vertical_mode",
    "box_drawing_font", "font_policy", "logging_vt_seq",
};

static char *config_values[MC_RADIOS][4] = {
  { "none", "esc", "8bit", NULL, },
  { "none", "sound", "visual", "sound|visual", },
  { "none", "left", "right", "autohide", },
  { "none", "cjk", "mongol", NULL, },
  { "noconv", "unicode", "decsp", NULL, },
  { "noconv", "unicode", "nounicode", NULL, },
  { "no", "raw", "ttyrec", NULL, },
};

static char *labels[MC_RADIOS][5] = {
  { N_("Meta key outputs"), N_("None"), N_("Esc"), N_("8bit"), NULL, },
  { N_("Bell mode"), N_("None"), N_("Sound"), N_("Visual"), N_("Both"), },
  { N_("Position"), N_("None"), N_("Left"), N_("Right"), N_("Auto hide"), },
  { N_("Vertical mode"), N_("None"), N_("CJK"), N_("Mongol"), NULL, },
  { N_("Box drawing"), N_("As it is"), N_("Unicode"), N_("DEC Special"), NULL, },
  { N_("Font policy"), N_("As it is"), N_("Always unicode"), N_("Never unicode"), NULL, },
  { N_("Save log"), N_("No"), N_("Raw format"), N_("Ttyrec format"), NULL, },
};

/* --- static functions --- */

static void update_value(int *data, int num) {
  int id;

  id = data - new_values;
  *data = num;

  if (funcs[id]) {
    (*funcs[id])();
  }
}

static void button1_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    update_value(data, 0);
  }
}

static void button2_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    update_value(data, 1);
  }
}

static void button3_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    update_value(data, 2);
  }
}

static void button4_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    update_value(data, 3);
  }
}

#if GTK_CHECK_VERSION(4, 0, 0)
static GtkWidget *create_radio_button(int id, int idx, GtkWidget *hbox, GtkWidget **group,
                                      const char *cur_value)
#else
static GtkWidget *create_radio_button(int id, int idx, GtkWidget *hbox, GSList **group,
                                      const char *cur_value)
#endif
{
  GtkWidget *radio;
  void (*callbacks[])(GtkWidget *, gpointer) =
    { button1_checked, button2_checked, button3_checked, button4_checked };

#if GTK_CHECK_VERSION(4, 0, 0)
  radio = gtk_check_button_new_with_label(_(labels[id][idx + 1]));
  if (*group) {
    gtk_check_button_set_group(GTK_CHECK_BUTTON(radio), GTK_CHECK_BUTTON(*group));
  } else {
    *group = radio;
  }
#else
  radio = gtk_radio_button_new_with_label(*group, _(labels[id][idx + 1]));
  *group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
#endif
  g_signal_connect(radio, "toggled", G_CALLBACK(callbacks[idx]), &new_values[id]);
  gtk_widget_show(radio);
  gtk_box_pack_start(GTK_BOX(hbox), radio, TRUE, FALSE, 0);

  if (strcmp(cur_value, config_values[id][idx]) == 0) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
    new_values[id] = old_values[id] = idx;
  }

  return radio;
}

/* --- global functions --- */

GtkWidget *mc_radio_config_widget_new(int id) {
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *radio;
#if GTK_CHECK_VERSION(4, 0, 0)
  GtkWidget *group;
#else
  GSList *group;
#endif
  char *value;

  value = mc_get_str_value(config_keys[id]);

  hbox = gtk_hbox_new(FALSE, 0);

  label = gtk_label_new(_(labels[id][0]));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

  group = NULL;

  radio = create_radio_button(id, 0, hbox, &group, value);
  radio = create_radio_button(id, 1, hbox, &group, value);
  radio = create_radio_button(id, 2, hbox, &group, value);

  if (config_values[id][3]) {
    radio = create_radio_button(id, 3, hbox, &group, value);
  }

  free(value);

#if GTK_CHECK_VERSION(2, 12, 0)
  if (id == MC_RADIO_LOG_VTSEQ) {
    char *pty;

    if ((pty = mc_get_str_value("pty_name"))) {
      char *p;
      char *msg;

      for (p = pty; *p; p++) {
        if (*p == '/') {
          *p = '_';
        }
      }

      /* 35 is "You can do 'ttyplay ~/.mlterm/.log'" */
      if (strcmp(pty, "error") != 0 && (msg = malloc(35 + strlen(pty + 5) + 1))) {
        sprintf(msg, "Log VT sequence in ~/.mlterm/%s.log", pty + 5);
        gtk_widget_set_tooltip_text(label, msg);
        sprintf(msg, "You can do \'ttyplay ~/.mlterm/%s.log\'", pty + 5);
        gtk_widget_set_tooltip_text(radio, msg);
        free(msg);
      }

      free(pty);
    }
  }
#endif

  return hbox;
}

void mc_update_radio(int id) {
  if (new_values[id] != old_values[id]) {
    is_changed[id] = 1;
  }

  if (is_changed[id]) {
    mc_set_str_value(config_keys[id], config_values[id][new_values[id]]);
    old_values[id] = new_values[id];
  }
}

int mc_radio_get_value(int id) { return new_values[id]; }

void mc_radio_set_callback(int id, void (*func)(void)) { funcs[id] = func; }
