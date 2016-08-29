/*
 *	$Id$
 */

#include "mc_unicode_areas.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <c_intl.h>

#include <pobl/bl_mem.h>
#include <pobl/bl_str.h> /* bl_str_sep */

#include "mc_io.h"

#if GTK_CHECK_VERSION(2, 14, 0)

/* --- static functions --- */

static void add_row(GtkWidget* widget, gpointer p) {
  GtkListStore* store;
  GtkTreeIter iter;

  store = p;

  gtk_list_store_append(store, &iter);
}

static void delete_row(GtkWidget* widget, gpointer p) {
  GtkTreeView* view;
  GtkTreeModel* store;
  GtkTreeIter itr;

  view = p;

  gtk_tree_selection_get_selected(gtk_tree_view_get_selection(view), &store, &itr);
  gtk_list_store_remove(GTK_LIST_STORE(store), &itr);
}

static int check_hex(const gchar* text) {
  const gchar* p;
  int count;

  for (count = 0, p = text; *p; p++) {
    if (++count > 17 || /* 17: XXXXXXXX-XXXXXXXX */
        (*p != '-' && (*p < '0' || ('9' < *p && *p < 'A') || ('F' < *p && *p < 'a') || 'f' < *p))) {
      return 0;
    }
  }

  return 1;
}

static void edited(GtkCellRendererText* renderer, gchar* path, gchar* new_text, gpointer data) {
  int min;
  int max;
  int num;
  GtkListStore* store;
  GtkTreeIter itr;
  GtkTreePath* treepath;
  GtkWidget* dialog;

  if (*new_text == '\0') {
    /* do nothing */
  } else if ((num = sscanf(new_text, "%x-%x", &min, &max)) == 2 ||
             (num = sscanf(new_text, "%x", &min)) == 1) {
    if (!check_hex(new_text)) {
      goto error1;
    } else if (num == 2 && min > max) {
      goto error2;
    } else {
      gchar* prepended;
      prepended = alloca(strlen(new_text) + 3);
      sprintf(prepended, "U+%s", new_text);
      new_text = prepended;
    }
  } else if ((num = sscanf(new_text, "U+%x-%x", &min, &max)) == 2 ||
             (num = sscanf(new_text, "U+%x", &min)) == 1) {
    if (!check_hex(new_text + 2)) {
      goto error1;
    } else if (num == 2 && min > max) {
      goto error2;
    }
  } else {
    goto error1;
  }

  store = data;

  treepath = gtk_tree_path_new_from_string(path);
  gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &itr, treepath);
  gtk_tree_path_free(treepath);

  gtk_list_store_set(store, &itr, 0, new_text, -1);

  return;

error1:
  dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE, "\'%s\' is illegal", new_text);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

  return;

error2:
  dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE, "U+%x is larger than U+%x", min, max);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

/* --- global functions --- */

char* mc_get_unicode_areas(char* areas) {
  GtkWidget* dialog;
  GtkWidget* label;
  GtkListStore* store;
  GtkCellRenderer* renderer;
  GtkWidget* view;
  GtkWidget* hbox;
  GtkWidget* button;
  GtkTreeIter itr;
  char* strp;
  char* area;

  dialog = gtk_dialog_new_with_buttons(
      "Edit unicode areas", NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_OK,
      GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

  label = gtk_label_new(
      _("Set unicode area in the following format.\n"
        "Format: U+XXXX-XXXX or U+XXXX (U+ is optional)"));
  gtk_widget_show(label);
  gtk_box_pack_start(gtk_dialog_get_content_area(GTK_DIALOG(dialog)), label, TRUE, TRUE, 1);

  store = gtk_list_store_new(1, G_TYPE_STRING);
  strp = areas;
  while ((area = bl_str_sep(&strp, ","))) {
    gtk_list_store_append(store, &itr);
    gtk_list_store_set(store, &itr, 0, area, -1);
  }
  view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  g_object_unref(G_OBJECT(store));
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), gtk_tree_view_column_new_with_attributes(
                                                       NULL, renderer, "text", 0, NULL));
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
  g_object_set(renderer, "editable", TRUE, NULL);
  g_signal_connect(renderer, "edited", G_CALLBACK(edited), store);
  gtk_widget_show(view);
  gtk_box_pack_start(gtk_dialog_get_content_area(GTK_DIALOG(dialog)), view, TRUE, TRUE, 1);

  hbox = gtk_hbox_new(TRUE, 0);
  gtk_widget_show(hbox);

  button = gtk_button_new_with_label("Add");
  gtk_widget_show(button);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 1);
  g_signal_connect(button, "clicked", G_CALLBACK(add_row), store);

  button = gtk_button_new_with_label("Delete");
  gtk_widget_show(button);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 1);
  g_signal_connect(button, "clicked", G_CALLBACK(delete_row), view);

  gtk_box_pack_start(gtk_dialog_get_content_area(GTK_DIALOG(dialog)), hbox, TRUE, TRUE, 1);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
    areas = NULL;
  } else if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &itr)) {
    char* p;

    /* 20: U+XXXXXXXX-XXXXXXXX, */
    p = areas = g_malloc(20 * gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL));
    do {
      GValue gval = {0};
      const char* str;

      gtk_tree_model_get_value(GTK_TREE_MODEL(store), &itr, 0, &gval);
      str = g_value_get_string(&gval);
      if (str && *str) {
        if (p > areas) {
          *(p++) = ',';
        }
        strcpy(p, str);
        p += strlen(p);
      } else {
        *p = '\0';
      }
      g_value_unset(&gval);
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &itr));
  } else {
    areas = strdup("");
  }

  gtk_widget_destroy(dialog);

  return areas;
}

#else

char* mc_get_unicode_areas(char* areas) {
  GtkWidget* dialog;

  dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE, "This dialog requires GTK+-2.14 or later");
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

  return NULL;
}

#endif
