/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __MC_COMPAT_H__
#define __MC_COMPAT_H__

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(4, 0, 0)

#undef GTK_CONTAINER
#define GTK_CONTAINER(a) (a)
#define gtk_container_set_border_width(container, width) (0)

#define gtk_hbox_new(homogeneous, spacing) gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing)
#define gtk_vbox_new(homogeneous, spacing) gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing)
#define gtk_box_pack_start(box, child, expand, fill, padding) gtk_box_append(box, child)

#define gtk_hseparator_new() gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)

#define gtk_entry_get_text(entry) gtk_entry_buffer_get_text(gtk_entry_get_buffer(entry))
#define gtk_entry_set_text(entry, text) \
  gtk_entry_buffer_set_text(gtk_entry_get_buffer(entry), text, -1)
#define gtk_entry_set_width_chars(entry, max) gtk_entry_set_max_length(entry, max)

#define GTK_STOCK_CANCEL _("_Cancel")
#define GTK_STOCK_OPEN _("_Open")
#define GTK_STOCK_OK _("_OK")

#undef GTK_TOGGLE_BUTTON
#define GTK_TOGGLE_BUTTON(a) GTK_CHECK_BUTTON(a)
#define gtk_toggle_button_set_active(button, flag) gtk_check_button_set_active(button, flag)
#define gtk_toggle_button_get_active(button) gtk_check_button_get_active(button)

#endif

#endif
