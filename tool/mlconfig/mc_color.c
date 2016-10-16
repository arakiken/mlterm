/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_color.h"

#include <pobl/bl_str.h>
#include <pobl/bl_mem.h> /* free */
#include <pobl/bl_debug.h>
#include <glib.h>
#include <c_intl.h>

#include "mc_io.h"

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

/* "rgba:rrrr/gggg/bbbb/aaaa" == 25 bytes */
#define MC_COLOR_LEN 32

/* colors are stored in untranslated forms */
static char new_color[MC_COLOR_MODES][MC_COLOR_LEN];
static char old_color[MC_COLOR_MODES][MC_COLOR_LEN];
static int is_changed[MC_COLOR_MODES];

static char new_vtcolor[16][MC_COLOR_LEN];
static char old_vtcolor[16][MC_COLOR_LEN];
static int is_changed_vt[16];

static char* configname[MC_COLOR_MODES] = {
    "fg_color", "bg_color", "sb_fg_color", "sb_bg_color", "cursor_fg_color", "cursor_bg_color",
    "bd_color", "it_color", "ul_color",    "bl_color",    "co_color"};

static char* labelname[MC_COLOR_MODES] = {
    N_("Foreground color"), N_("Background color"), N_("Foreground color"), N_("Background color"),
    N_("Foreground color"), N_("Background color"), N_("Bold "), N_("Italic"), N_("Underline"),
    N_("Blink"), N_("Cross out")};

/* --- static functions --- */

#if !GTK_CHECK_VERSION(2, 12, 0)
/* gdk_color_to_string() was not supported by gtk+ < 2.12. */
static gchar* gdk_color_to_string(const GdkColor* color) {
  gchar* str;

  if ((str = g_malloc(14)) == NULL) {
    return NULL;
  }

  sprintf(str, "#%04x%04x%04x", color->red, color->green, color->blue);

  return str;
}
#endif

static char* color_strncpy(char* dst, const char* src) {
  strncpy(dst, src, MC_COLOR_LEN);
  dst[MC_COLOR_LEN - 1] = 0;

  return dst;
}

static void color_selected(GtkWidget* button, gpointer data) {
  GdkColor color;
  gchar* str;

  gtk_color_button_get_color(GTK_COLOR_BUTTON(button), &color);
  str = gdk_color_to_string(&color);
  color_strncpy(g_object_get_data(G_OBJECT(button), "color"), str);
  g_free(str);
}

static void checked(GtkWidget* check, gpointer data) {
  GtkWidget* button;

  button = data;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check))) {
    gtk_widget_set_sensitive(button, 1);
  } else {
    gtk_widget_set_sensitive(button, 0);
    color_strncpy(g_object_get_data(G_OBJECT(button), "color"), "");
  }
}

/* --- global functions --- */

GtkWidget* mc_color_config_widget_new(int id) {
  char* value;
  GtkWidget* hbox;
  GtkWidget* label;
  GtkWidget* button;
  GdkColor color;

  value = mc_get_str_value(configname[id]);
  color_strncpy(new_color[id], value);
  color_strncpy(old_color[id], value);

  hbox = gtk_hbox_new(FALSE, 0);

  if (id <= MC_COLOR_SBBG) {
    label = gtk_label_new(_(labelname[id]));
  } else {
    label = gtk_check_button_new_with_label(_(labelname[id]));
    if (*value) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(label), TRUE);
    }
  }

  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 1);

  memset(&color, 0, sizeof(color));
  gdk_color_parse(value, &color);
  button = gtk_color_button_new_with_color(&color);
  gtk_widget_show(button);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  g_object_set_data(G_OBJECT(button), "color", &new_color[id]);
  g_signal_connect(button, "color-set", G_CALLBACK(color_selected), NULL);

  if (id > MC_COLOR_SBBG) {
    if (*value == '\0') {
      gtk_widget_set_sensitive(button, 0);
    }

    g_signal_connect(label, "toggled", G_CALLBACK(checked), button);
  }

  return hbox;
}

GtkWidget* mc_cursor_color_config_widget_new(void) {
  GtkWidget* hbox;
  GtkWidget* frame;
  GtkWidget* color;

  frame = gtk_frame_new(_("Cursor color"));
  hbox = gtk_hbox_new(FALSE, 1);
  gtk_widget_show(hbox);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_container_add(GTK_CONTAINER(frame), hbox);

  color = mc_color_config_widget_new(MC_COLOR_CURSOR_FG);
  gtk_widget_show(color);
  gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 1);

  color = mc_color_config_widget_new(MC_COLOR_CURSOR_BG);
  gtk_widget_show(color);
  gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 1);

  return frame;
}

GtkWidget* mc_substitute_color_config_widget_new(void) {
  GtkWidget* vbox;
  GtkWidget* hbox;
  GtkWidget* frame;
  GtkWidget* color;

  frame = gtk_frame_new(_("Substituting color"));
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_container_add(GTK_CONTAINER(frame), vbox);

  hbox = gtk_hbox_new(FALSE, 1);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  color = mc_color_config_widget_new(MC_COLOR_BD);
  gtk_widget_show(color);
  gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 1);

  color = mc_color_config_widget_new(MC_COLOR_UL);
  gtk_widget_show(color);
  gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 1);

  color = mc_color_config_widget_new(MC_COLOR_IT);
  gtk_widget_show(color);
  gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 1);

  hbox = gtk_hbox_new(FALSE, 1);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  color = mc_color_config_widget_new(MC_COLOR_BL);
  gtk_widget_show(color);
  gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 1);

  color = mc_color_config_widget_new(MC_COLOR_CO);
  gtk_widget_show(color);
  gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 1);

  return frame;
}

GtkWidget* mc_vtcolor_config_widget_new(void) {
  int id;
  char id_str[3];
  char* value;
  GtkWidget* frame;
  GtkWidget* vbox;
  GtkWidget* hbox;
  GtkWidget* button;
  GdkColor color;

  frame = gtk_frame_new(_("VT basic 16 colors"));
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_container_add(GTK_CONTAINER(frame), vbox);

  for (id = 0; id < 16; id++) {
    sprintf(id_str, "%d", id);
    value = mc_get_color_name(id_str);
    color_strncpy(new_vtcolor[id], value);
    color_strncpy(old_vtcolor[id], value);

    if (id % 8 == 0) {
      hbox = gtk_hbox_new(FALSE, 0);
      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    }

    memset(&color, 0, sizeof(color));
    gdk_color_parse(value, &color);
    button = gtk_color_button_new_with_color(&color);
    gtk_widget_show(button);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(button), "color", &new_vtcolor[id]);
    g_signal_connect(button, "color-set", G_CALLBACK(color_selected), NULL);
  }

  return frame;
}

void mc_update_color(int id) {
  if (strcmp(new_color[id], old_color[id]) != 0) is_changed[id] = 1;

  if (is_changed[id]) {
    mc_set_str_value(configname[id], new_color[id]);
    strcpy(old_color[id], new_color[id]);
  }
}

void mc_update_cursor_color(void) {
  mc_update_color(MC_COLOR_CURSOR_FG);
  mc_update_color(MC_COLOR_CURSOR_BG);
}

void mc_update_substitute_color(void) {
  mc_update_color(MC_COLOR_BD);
  mc_update_color(MC_COLOR_IT);
  mc_update_color(MC_COLOR_UL);
  mc_update_color(MC_COLOR_BL);
  mc_update_color(MC_COLOR_CO);
}

void mc_update_vtcolor(mc_io_t io) {
  int id;
  char id_str[3];

  for (id = 0; id < 16; id++) {
    if (strcmp(new_vtcolor[id], old_vtcolor[id]) != 0) {
      is_changed_vt[id] = 1;
    }

    if (is_changed_vt[id]) {
      sprintf(id_str, "%d", id);
      mc_set_color_name(io, id_str, new_vtcolor[id]);
      strcpy(old_vtcolor[id], new_vtcolor[id]);
    }
  }
}
