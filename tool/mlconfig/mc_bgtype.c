/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_bgtype.h"

#include <glib.h>
#include <c_intl.h>
#include <stdlib.h> /* atoi */
#include <string.h> /* strcmp */

#include "mc_compat.h"
#include "mc_color.h"
#include "mc_wall_pic.h"
#include "mc_io.h"

#if 0
#define __DEBUG
#endif

#define MC_BG_PSEUDO_TRANSBG 0
#define MC_BG_WALLPICTURE 1
#define MC_BG_COLOR 2

/* --- static variables --- */

static int bgtype;
static int is_changed;
static GtkWidget *bg_color;
static GtkWidget *wall_picture;
static GtkWidget *true_transbg;
static int true_transbg_state;

/* --- static functions --- */

static int get_bgtype(void) {
  char *val;

  if (mc_get_flag_value("use_transbg"))
    return MC_BG_PSEUDO_TRANSBG;
  else if ((val = mc_get_str_value("wall_picture")) && *val != '\0')
    return MC_BG_WALLPICTURE;
  else
    return MC_BG_COLOR;
}

static int is_true_transbg(void) {
  char *val;

  if (strcmp(mc_get_gui(), "xlib") == 0 &&
      (val = mc_get_str_value("depth")) && atoi(val) == 32) {
    return 1;
  } else {
    return 0;
  }
}

static void set_sensitive(void) {
  if (bgtype == MC_BG_COLOR) {
    gtk_widget_set_sensitive(bg_color, 1);
    gtk_widget_set_sensitive(wall_picture, 0);
    if (strcmp(mc_get_gui(), "xlib") == 0) {
      gtk_widget_set_sensitive(true_transbg, 1);
    } else {
      gtk_widget_set_sensitive(true_transbg, 0);
    }
  } else if (bgtype == MC_BG_WALLPICTURE) {
    gtk_widget_set_sensitive(bg_color, 0);
    gtk_widget_set_sensitive(wall_picture, 1);
    gtk_widget_set_sensitive(true_transbg, 0);
  } else {
    gtk_widget_set_sensitive(bg_color, 0);
    gtk_widget_set_sensitive(wall_picture, 0);
    gtk_widget_set_sensitive(true_transbg, 0);
  }
}

static void button_true_transbg_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    true_transbg_state = 1;
  } else {
    true_transbg_state = -1;
  }
}

static void button_color_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    bgtype = MC_BG_COLOR;
    is_changed = 1;
    set_sensitive();
  }
}

static void button_pseudo_transbg_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    bgtype = MC_BG_PSEUDO_TRANSBG;
    is_changed = 1;
    set_sensitive();
  }
}

static void button_picture_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    bgtype = MC_BG_WALLPICTURE;
    is_changed = 1;
    set_sensitive();
  }
}

/* --- global functions --- */

GtkWidget *mc_bgtype_config_widget_new(void) {
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *radio;
#if GTK_CHECK_VERSION(4, 0, 0)
  GtkWidget *group;
#else
  GSList *group;
#endif

  bgtype = get_bgtype();

  bg_color = mc_color_config_widget_new(MC_COLOR_BG);
  gtk_widget_show(bg_color);
  wall_picture = mc_wall_pic_config_widget_new();
  gtk_widget_show(wall_picture);
  true_transbg = gtk_check_button_new_with_label(_("True Transparent"));
  gtk_widget_show(true_transbg);

  group = NULL;

  frame = gtk_frame_new(_("Background type"));
  vbox = gtk_vbox_new(TRUE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_frame_set_child(GTK_FRAME(frame), vbox);
#else
  gtk_container_add(GTK_CONTAINER(frame), vbox);
#endif
  gtk_widget_show(vbox);

  /* color button */
#if GTK_CHECK_VERSION(4, 0, 0)
  group = radio = gtk_check_button_new_with_label(_("Color"));
#else
  radio = gtk_radio_button_new_with_label(group, _("Color"));
  group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
#endif
  if (bgtype == MC_BG_COLOR) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
  g_signal_connect(radio, "toggled", G_CALLBACK(button_color_checked), NULL);
  gtk_widget_show(GTK_WIDGET(radio));

  if (is_true_transbg()) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(true_transbg), TRUE);
  }
  g_signal_connect(true_transbg, "toggled", G_CALLBACK(button_true_transbg_checked), NULL);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text(true_transbg,
                              "Modify \"Alpha\", press \"Save&Exit\" and restart mlterm");
#endif

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), bg_color, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), true_transbg, TRUE, TRUE, 0);

  /* picture button */
#if GTK_CHECK_VERSION(4, 0, 0)
  radio = gtk_check_button_new_with_label(_("Picture"));
  gtk_check_button_set_group(GTK_CHECK_BUTTON(radio), GTK_CHECK_BUTTON(group));
#else
  radio = gtk_radio_button_new_with_label(group, _("Picture"));
  group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
#endif
  if (bgtype == MC_BG_WALLPICTURE) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
  g_signal_connect(radio, "toggled", G_CALLBACK(button_picture_checked), NULL);
  gtk_widget_show(GTK_WIDGET(radio));

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), wall_picture, TRUE, TRUE, 0);

  /* pseudo_transbg button */
#if GTK_CHECK_VERSION(4, 0, 0)
  radio = gtk_check_button_new_with_label(_("Pseudo transparent"));
  gtk_check_button_set_group(GTK_CHECK_BUTTON(radio), GTK_CHECK_BUTTON(group));
#else
  radio = gtk_radio_button_new_with_label(group, _("Pseudo transparent"));
  group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
#endif
  if (bgtype == MC_BG_PSEUDO_TRANSBG) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
  g_signal_connect(radio, "toggled", G_CALLBACK(button_pseudo_transbg_checked), NULL);
  gtk_widget_show(GTK_WIDGET(radio));

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);

  set_sensitive();

  return frame;
}

void mc_update_bgtype(void) {
  if (bgtype == MC_BG_COLOR) {
    if (is_changed) {
      mc_set_flag_value("use_transbg", 0);
      mc_wall_pic_none();
    }
    mc_update_color(MC_COLOR_BG);

    if (true_transbg_state) {
      if (true_transbg_state > 0) {
        mc_set_str_value("depth", "32");
      } else {
        mc_set_str_value("depth", "");
      }
    }
  } else if (bgtype == MC_BG_WALLPICTURE) {
    if (is_changed) {
      mc_set_flag_value("use_transbg", 0);
    }
    mc_update_wall_pic();
  } else if (bgtype == MC_BG_PSEUDO_TRANSBG) {
    if (is_changed) {
      mc_set_flag_value("use_transbg", 1);
      mc_wall_pic_none();
    }
  }
}

int mc_is_color_bg(void) { return bgtype == MC_BG_COLOR; }
