/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_bgtype.h"

#include <glib.h>
#include <c_intl.h>

#include "mc_color.h"
#include "mc_wall_pic.h"
#include "mc_io.h"

#if 0
#define __DEBUG
#endif

#define MC_BG_TRANSPARENT 0
#define MC_BG_WALLPICTURE 1
#define MC_BG_COLOR 2

/* --- static variables --- */

static int bgtype;
static int is_changed;
static GtkWidget *bg_color;
static GtkWidget *wall_picture;

/* --- static functions --- */

static int get_bgtype(void) {
  char *val;

  if (mc_get_flag_value("use_transbg"))
    return MC_BG_TRANSPARENT;
  else if ((val = mc_get_str_value("wall_picture")) && *val != '\0')
    return MC_BG_WALLPICTURE;
  else
    return MC_BG_COLOR;
}

static void set_sensitive(void) {
  if (bgtype == MC_BG_COLOR) {
    gtk_widget_set_sensitive(bg_color, 1);
    gtk_widget_set_sensitive(wall_picture, 0);
  } else if (bgtype == MC_BG_WALLPICTURE) {
    gtk_widget_set_sensitive(bg_color, 0);
    gtk_widget_set_sensitive(wall_picture, 1);
  } else {
    gtk_widget_set_sensitive(bg_color, 0);
    gtk_widget_set_sensitive(wall_picture, 0);
  }
}

static gint button_color_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    bgtype = MC_BG_COLOR;
    is_changed = 1;
    set_sensitive();
  }
  return 1;
}

static gint button_transparent_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    bgtype = MC_BG_TRANSPARENT;
    is_changed = 1;
    set_sensitive();
  }
  return 1;
}

static gint button_picture_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    bgtype = MC_BG_WALLPICTURE;
    is_changed = 1;
    set_sensitive();
  }
  return 1;
}

/* --- global functions --- */

GtkWidget *mc_bgtype_config_widget_new(void) {
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *radio;
  GSList *group;

  bgtype = get_bgtype();

  bg_color = mc_color_config_widget_new(MC_COLOR_BG);
  gtk_widget_show(bg_color);
  wall_picture = mc_wall_pic_config_widget_new();
  gtk_widget_show(wall_picture);

  group = NULL;

  frame = gtk_frame_new(_("Background type"));
  vbox = gtk_vbox_new(TRUE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* color button */
  radio = gtk_radio_button_new_with_label(group, _("Color"));
  group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
  g_signal_connect(radio, "toggled", G_CALLBACK(button_color_checked), NULL);
  gtk_widget_show(GTK_WIDGET(radio));
  if (bgtype == MC_BG_COLOR) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), bg_color, TRUE, TRUE, 0);

  /* picture button */
  radio = gtk_radio_button_new_with_label(group, _("Picture"));
  group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
  g_signal_connect(radio, "toggled", G_CALLBACK(button_picture_checked), NULL);
  gtk_widget_show(GTK_WIDGET(radio));
  if (bgtype == MC_BG_WALLPICTURE) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), wall_picture, TRUE, TRUE, 0);

  /* transparent button */
  radio = gtk_radio_button_new_with_label(group, _("Pseudo transparent"));
  group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
  g_signal_connect(radio, "toggled", G_CALLBACK(button_transparent_checked), NULL);
  gtk_widget_show(GTK_WIDGET(radio));
  if (bgtype == MC_BG_TRANSPARENT) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text(radio,
                              "If you want true translucence, toggle this "
                              "button off and start mlterm with depth=32.");
#endif
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
  } else if (bgtype == MC_BG_WALLPICTURE) {
    if (is_changed) {
      mc_set_flag_value("use_transbg", 0);
    }
    mc_update_wall_pic();
  } else if (bgtype == MC_BG_TRANSPARENT) {
    if (is_changed) {
      mc_set_flag_value("use_transbg", 1);
      mc_wall_pic_none();
    }
  }
}

int mc_is_color_bg(void) { return bgtype == MC_BG_COLOR; }
