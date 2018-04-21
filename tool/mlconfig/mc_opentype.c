/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_unicode_areas.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <c_intl.h>

#include <pobl/bl_mem.h>
#include <pobl/bl_str.h> /* bl_str_sep */

#include "mc_io.h"
#include "mc_flags.h"

#if GTK_CHECK_VERSION(2, 14, 0)

#define FEATURE_LEN 4
#define SCRIPT_LEN 4

/* --- static variables --- */

static char *features;
static char *script;
static GtkWidget *features_button;
static GtkWidget *script_button;

/* --- static functions --- */

static char *ascii_strcasestr(const char *str1 /* separated by ',' */,
                              const char *str2 /* 4 bytes/Lower case */) {
  const char *p1 = str1;
  const char *p2 = str2;

  while (1) {
    if ((*p1|0x20) == *p2) {
      if (*(++p2) == '\0') {
        return p1 - 3;
      }
      if (*(++p1) == '\0') {
        return NULL;
      }
    } else {
      do {
        if (*p1 == '\0') {
          return NULL;
        }
      } while (*(p1++) != ',');
      p2 = str2;
    }
  }
}

static int contains(char *values, char *value) {
  char *p;

  if ((p = ascii_strcasestr(values, value))) {
    if (p == values || *(p - 1) == ',') {
      p += strlen(value);

      if (*p == '\0' || *p == ',') {
        return 1;
      }
    }
  }

  return 0;
}

static void edit_features(GtkWidget *widget, gpointer data) {
  static char *features_tbl[] = {
    "c2pc", "c2sc", "calt", "case", "clig", "cpsp", "cswh", "dlig", "expt", "frac", "fwid", "halt",
    "hist", "hkna", "hlig", "hngl", "hojo", "hwid", "ital", "jp04", "jp78", "jp83", "jp90", "liga",
    "lnum", "mgrk", "nlck", "onum", "ordn", "palt", "pcap", "pkna", "pnum", "pwid", "qwid", "ruby",
    "sinf", "smcp", "smpl", "ss01", "ss02", "ss03", "ss04", "ss05", "ss06", "ss07", "ss08", "ss09",
    "ss10", "ss11", "ss12", "ss13", "ss14", "ss15", "ss16", "ss17", "ss18", "ss19", "ss20", "subs",
    "sups", "swsh", "titl", "tnam", "tnum", "trad", "twid", "unic", "valt", "vert", "vhal", "vkna",
    "vpal", "vrt2", "zero", };
  GtkWidget *buttons[sizeof(features_tbl) / sizeof(features_tbl[0])];
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *hbox;
  u_int count;
  char *value;

  if (features) {
    value = strdup(features);
  } else {
    value = mc_get_str_value("ot_features");
  }

  dialog = gtk_dialog_new_with_buttons(_("Select opentype features"), NULL,
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL,
                                       GTK_RESPONSE_REJECT, NULL);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);
  gtk_box_pack_start(gtk_dialog_get_content_area(GTK_DIALOG(dialog)), vbox, FALSE, FALSE, 0);

  for (count = 0; count < sizeof(features_tbl) / sizeof(features_tbl[0]); count++) {
    if (count % 8 == 0) {
      hbox = gtk_hbox_new(FALSE, 0);
      gtk_widget_show(hbox);
      gtk_box_pack_start(vbox, hbox, FALSE, FALSE, 0);
    }

    buttons[count] = gtk_check_button_new_with_label(features_tbl[count]);
    if (contains(value, features_tbl[count])) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(buttons[count]), TRUE);
    }
    gtk_widget_show(buttons[count]);
    gtk_box_pack_start(hbox, buttons[count], FALSE, FALSE, 0);
  }

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *new_value;

    if ((new_value = malloc((FEATURE_LEN + 1) *
                            (sizeof(features_tbl) / sizeof(features_tbl[0]))))) {
      char *p = new_value;

      for (count = 0; count < sizeof(features_tbl) / sizeof(features_tbl[0]); count++) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(buttons[count]))) {
          strcpy(p, features_tbl[count]);
          p += FEATURE_LEN;
          *(p++) = ',';
        }
      }

      if (p != new_value) {
        p--;
      }
      *p = '\0';
    }

    if (g_ascii_strcasecmp(value, new_value) != 0) {
      free(features);
      features = new_value;
    } else {
      free(new_value);
    }
  }

  free(value);

  gtk_widget_destroy(dialog);
}

static void edit_script(GtkWidget *widget, gpointer data) {
  static char *scripts_tbl[] = {
    "zyyy", "zinh", "zzzz", "arab", "armn", "beng", "cyrl", "deva", "geor", "grek", "gujr", "guru",
    "hang", "hani", "hebr", "hira", "knda", "kana", "laoo", "latn", "mlym", "orya", "taml", "telu",
    "thai", "tibt", "bopo", "brai", "cans", "cher", "ethi", "khmr", "mong", "mymr", "ogam", "runr",
    "sinh", "syrc", "thaa", "yiii", "dsrt", "goth", "ital", "buhd", "hano", "tglg", "tagb", "cprt",
    "limb", "linb", "osma", "shaw", "tale", "ugar", "bugi", "copt", "glag", "khar", "talu", "xpeo",
    "sylo", "tfng", "bali", "xsux", "nkoo", "phag", "phnx", "cari", "cham", "kali", "lepc", "lyci",
    "lydi", "olck", "rjng", "saur", "sund", "vaii", "avst", "bamu", "egyp", "armi", "phli", "prti",
    "java", "kthi", "lisu", "mtei", "sarb", "orkh", "samr", "lana", "tavt", "batk", "brah", "mand",
    "cakm", "merc", "mero", "plrd", "shrd", "sora", "takr", "bass", "aghb", "dupl", "elba", "gran",
    "khoj", "sind", "lina", "mahj", "mani", "mend", "modi", "mroo", "nbat", "narb", "perm", "hmng",
    "palm", "pauc", "phlp", "sidd", "tirh", "wara", "ahom", "hluw", "hatr", "mult", "hung", "sgnw",
  };
  GtkWidget *buttons[sizeof(scripts_tbl) / sizeof(scripts_tbl[0])];
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *group = NULL;
  u_int count;
  char *value;

  if (script) {
    value = strdup(script);
  } else {
    value = mc_get_str_value("ot_script");
  }

  dialog = gtk_dialog_new_with_buttons(_("Select opentype scripts"), NULL,
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL,
                                       GTK_RESPONSE_REJECT, NULL);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);
  gtk_box_pack_start(gtk_dialog_get_content_area(GTK_DIALOG(dialog)), vbox, FALSE, FALSE, 0);

  for (count = 0; count < sizeof(scripts_tbl) / sizeof(scripts_tbl[0]); count++) {
    if (count % 8 == 0) {
      hbox = gtk_hbox_new(FALSE, 0);
      gtk_widget_show(hbox);
      gtk_box_pack_start(vbox, hbox, FALSE, FALSE, 0);
    }

    buttons[count] = gtk_radio_button_new_with_label(group, scripts_tbl[count]);
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(buttons[count]));
    if (g_ascii_strcasecmp(value, scripts_tbl[count]) == 0) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(buttons[count]), TRUE);
    }
    gtk_widget_show(buttons[count]);
    gtk_box_pack_start(hbox, buttons[count], FALSE, FALSE, 0);
  }

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *new_value;

    if ((new_value = malloc(SCRIPT_LEN + 1))) {
      for (count = 0; count < sizeof(scripts_tbl) / sizeof(scripts_tbl[0]); count++) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(buttons[count]))) {
          strcpy(new_value, scripts_tbl[count]);
          break;
        }
      }
    }

    if (g_ascii_strcasecmp(value, new_value) != 0) {
      free(script);
      script = new_value;
    } else {
      free(new_value);
    }
  }

  free(value);

  gtk_widget_destroy(dialog);
}

static gint button_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    gtk_widget_set_sensitive(features_button, 1);
    gtk_widget_set_sensitive(script_button, 1);
  } else {
    gtk_widget_set_sensitive(features_button, 0);
    gtk_widget_set_sensitive(script_button, 0);
  }

  return 1;
}

/* --- global functions --- */

GtkWidget *mc_opentype_config_widget_new(void) {
  GtkWidget *hbox;
  GtkWidget *widget;

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);

  widget = mc_flag_config_widget_new(MC_FLAG_OTLAYOUT);
  gtk_widget_show(widget);
  gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
  g_signal_connect(widget, "toggled", G_CALLBACK(button_checked), NULL);

  features_button = gtk_button_new_with_label(_("Features"));
  gtk_widget_show(features_button);
  gtk_box_pack_start(GTK_BOX(hbox), features_button, FALSE, FALSE, 4);
  g_signal_connect(features_button, "clicked", G_CALLBACK(edit_features), NULL);

  script_button = gtk_button_new_with_label(_("Script"));
  gtk_widget_show(script_button);
  gtk_box_pack_start(GTK_BOX(hbox), script_button, FALSE, FALSE, 4);
  g_signal_connect(script_button, "clicked", G_CALLBACK(edit_script), NULL);

  button_checked(widget, NULL);

  return hbox;
}

void mc_update_opentype(void) {
  if (features) {
    mc_set_str_value("ot_features", features);
  }
  if (script) {
    mc_set_str_value("ot_script", script);
  }
}

#else

GtkWidget *mc_opentype_config_widget_new(void) {
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE, "This dialog requires GTK+-2.14 or later");
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

  return NULL;
}

void mc_update_opentype(void) {
}

#endif
