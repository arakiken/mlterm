/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
  should be compiled as
   gcc `pkg-config --cflags --libs gtk+-2.0 ` ./mlconf_dnd.c
 */

#include <gtk/gtk.h>

/* --- static variables --- */

#define MLTERM_CONFIG_MIMETYPE "text/x-mlterm.config"

GtkTargetEntry targets[] = {{MLTERM_CONFIG_MIMETYPE, 0, 0}};

/* --- static functions --- */
static void drag_data_get(GtkWidget *widget, GdkDragContext *context,
                          GtkSelectionData *selection_data, guint info, guint time, gpointer data) {
  char buffer[sizeof("use_transbg=xxxxx") + 1];

  sprintf(buffer, "use_transbg=%s",
          gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ? "true" : "false");
  gtk_selection_data_set(selection_data, gdk_atom_intern(MLTERM_CONFIG_MIMETYPE, FALSE), 8, buffer,
                         sizeof(buffer));
}

static void toggled(GtkToggleButton *toggle, gpointer data) {
  gtk_button_set_label(GTK_BUTTON(toggle), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle))
                                               ? " set \ntransparency"
                                               : "unset\ntransparency");
}

static gint end_application(GtkWidget *widget, gpointer data) {
  gtk_main_quit();
  return FALSE;
}

/* --- global functions --- */

int main(int argc, char **argv) {
  GtkWidget *window;
  GtkWidget *toggle;

  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect(GTK_OBJECT(window), "delete_event", GTK_SIGNAL_FUNC(end_application), NULL);

  toggle = gtk_toggle_button_new_with_label("unset\ntransparency");

  gtk_drag_source_set(toggle, GDK_MODIFIER_MASK, targets, 1, GDK_ACTION_COPY);
  gtk_signal_connect(GTK_OBJECT(toggle), "drag_data_get", GTK_SIGNAL_FUNC(drag_data_get), NULL);
  gtk_signal_connect(GTK_OBJECT(toggle), "toggled", GTK_SIGNAL_FUNC(toggled), NULL);

  gtk_container_add(GTK_CONTAINER(window), toggle);

  gtk_window_set_title(GTK_WINDOW(window), ("mlterm configuration"));
  gtk_container_set_border_width(GTK_CONTAINER(window), 0);

  gtk_drag_source_set_icon_stock(toggle, GTK_STOCK_EXECUTE);

  gtk_widget_show(toggle);
  gtk_widget_show(window);
  gtk_main();
}
