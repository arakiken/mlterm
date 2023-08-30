/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h> /* sprintf */
#include <gtk/gtk.h>
#include <glib.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>

#include <c_intl.h>

#include "mc_compat.h"
#include "mc_char_encoding.h"
#include "mc_auto_detect.h"
#include "mc_ctl.h"
#include "mc_color.h"
#include "mc_bgtype.h"
#include "mc_alpha.h"
#include "mc_tabsize.h"
#include "mc_logsize.h"
#include "mc_wordsep.h"
#include "mc_font.h"
#include "mc_space.h"
#include "mc_im.h"
#include "mc_sb_view.h"
#include "mc_io.h"
#include "mc_pty.h"
#include "mc_flags.h"
#include "mc_ratio.h"
#include "mc_radio.h"
#include "mc_char_width.h"
#include "mc_geometry.h"
#include "mc_click.h"
#include "mc_opentype.h"

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

#if GTK_CHECK_VERSION(4, 0, 0)
static GtkWidget *window;
#endif

/* --- static functions --- */

#ifdef DEBUG
static void check_mem_leak(void) {
  bl_mem_free_all();
}
#endif

#if !GTK_CHECK_VERSION(4, 0, 0)
static void end_application(GtkWidget *widget, gpointer data) { gtk_main_quit(); }
#endif

/*
 *  ********  procedures when buttons are clicked  ********
 */

static int update(mc_io_t io) {
  const char *gui = mc_get_gui();

  mc_update_char_encoding();
  mc_update_auto_detect();
  mc_update_color(MC_COLOR_FG);
  if (mc_is_color_bg()) {
    mc_update_bgtype();
    mc_update_alpha();
  } else {
    /*
     * alpha must be updated before bgtype because transparent or wall picture
     * bgtype could change alpha from 255 to 0.
     */
    mc_update_alpha();
    mc_update_bgtype();
  }
  mc_update_tabsize();
  mc_update_logsize();
  mc_update_wordsep();
  mc_update_geometry();
  mc_update_font_misc();
  mc_update_space(MC_SPACE_LINE);
  mc_update_space(MC_SPACE_LETTER);
  mc_update_space(MC_SPACE_UNDERLINE);
  mc_update_space(MC_SPACE_BASELINE);
  mc_update_ratio(MC_RATIO_SCREEN_WIDTH);
  mc_update_radio(MC_RADIO_MOD_META_MODE);
  mc_update_radio(MC_RADIO_BELL_MODE);
  mc_update_radio(MC_RADIO_LOG_VTSEQ);
  mc_update_ratio(MC_RATIO_BRIGHTNESS);
  mc_update_ratio(MC_RATIO_CONTRAST);
  mc_update_ratio(MC_RATIO_GAMMA);
  mc_update_ratio(MC_RATIO_FADE);
  mc_update_im();
  mc_update_cursor_color();
  mc_update_substitute_color();
  mc_update_ctl();
  mc_update_char_width();
  mc_update_click_interval();
  mc_update_opentype();

  mc_update_flag_mode(MC_FLAG_COMB);
  mc_update_flag_mode(MC_FLAG_DYNCOMB);
  mc_update_flag_mode(MC_FLAG_RECVUCS);
  if (strcmp(gui, "xlib") == 0) {
    mc_update_flag_mode(MC_FLAG_CLIPBOARD);
  }
  mc_update_flag_mode(MC_FLAG_LOCALECHO);
  mc_update_flag_mode(MC_FLAG_BLINKCURSOR);
  mc_update_flag_mode(MC_FLAG_STATICBACKSCROLL);
  mc_update_flag_mode(MC_FLAG_REGARDURIASWORD);
  mc_update_flag_mode(MC_FLAG_OTLAYOUT);

  mc_update_radio(MC_RADIO_SB_MODE);

  if (strcmp(gui, "quartz") != 0) {
    mc_update_color(MC_COLOR_SBFG);
    mc_update_color(MC_COLOR_SBBG);
    mc_update_sb_view_name();
  }

  mc_flush(io);

  if (io == mc_io_set) {
    mc_update_font_name(mc_io_set_font);
    mc_update_vtcolor(mc_io_set_color);
  } else if (io == mc_io_set_save) {
    mc_update_font_name(mc_io_set_save_font);
    mc_update_vtcolor(mc_io_set_save_color);
  }

  return 1;
}

static void cancel_clicked(GtkWidget *widget, gpointer data) {
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_window_destroy(GTK_WINDOW(window));
#else
  gtk_main_quit();
#endif
}

static void apply_clicked(GtkWidget *widget, gpointer data) {
  update(mc_io_set);
}

static void saveexit_clicked(GtkWidget *widget, gpointer data) {
  update(mc_io_set_save);

#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_window_destroy(GTK_WINDOW(window));
#else
  gtk_main_quit();
#endif
}

static void applyexit_clicked(GtkWidget *widget, gpointer data) {
  update(mc_io_set);

#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_window_destroy(GTK_WINDOW(window));
#else
  gtk_main_quit();
#endif
}

static void larger_clicked(GtkWidget *widget, gpointer data) {
  mc_set_str_value("fontsize", "larger");
  mc_flush(mc_io_set);
}

static void smaller_clicked(GtkWidget *widget, gpointer data) {
  mc_set_str_value("fontsize", "smaller");
  mc_flush(mc_io_set);
}

static void full_reset_clicked(GtkWidget *widget, gpointer data) {
  mc_exec("full_reset");
}

static void snapshot_clicked(GtkWidget *widget, gpointer data) {
  mc_exec("snapshot");
}

#ifdef USE_LIBSSH2

#define MY_RESPONSE_RETURN 1
#define MY_RESPONSE_EXIT 2
#define MY_RESPONSE_SEND 3
#define MY_RESPONSE_RECV 4

static int dialog_post_process(GtkWidget *dialog, int response_id, GtkWidget *local_entry,
                               GtkWidget *remote_entry) {
  if (response_id >= MY_RESPONSE_SEND) {
    char *cmd;
    const gchar *local_path;
    const gchar *remote_path;

    local_path = gtk_entry_get_text(GTK_ENTRY(local_entry));
    remote_path = gtk_entry_get_text(GTK_ENTRY(remote_entry));

    if ((cmd = alloca(28 + strlen(local_path) + strlen(remote_path)))) {
      char *p;

      if (response_id == MY_RESPONSE_SEND) {
        if (!*local_path) {
          bl_msg_printf("Local file path to send is not specified.\n");

          return -1;
        }

        sprintf(cmd, "scp \"local:%s\" \"remote:%s\" UTF8", local_path, remote_path);
      } else /* if( res == MY_RESPONSE_RECV) */ {
        if (!*remote_path) {
          bl_msg_printf("Remote file path to receive is not specified.\n");

          return -1;
        }

        sprintf(cmd, "scp \"remote:%s\" \"local:%s\" UTF8", remote_path, local_path);
      }

      p = cmd + strlen(cmd) - 2;
      if (*p == '\\') {
        /*
         * Avoid to be parsed as follows.
         * "local:c:\foo\bar\" => local:c:\foo\bar"
         */
        *(p++) = '\"';
        *p = '\0';
      }

      mc_exec(cmd);
    }
  }

  if (response_id == MY_RESPONSE_EXIT) {
    return 0;
  } else /* if (response_id == MY_RESPONSE_RETURN) */ {
    return 1;
  }
}

#if GTK_CHECK_VERSION(4, 0, 0)

static struct {
  GtkWidget *local_entry;
  GtkWidget *remote_entry;
} dialog_arg;

static void dialog_cb(GtkDialog *dialog, int response_id, gpointer user_data) {
  gint res;

  if ((res = dialog_post_process(GTK_WIDGET(dialog), response_id, dialog_arg.local_entry,
                                 dialog_arg.remote_entry)) != -1) {
    gtk_window_destroy(GTK_WINDOW(dialog));

    if (res == 0) {
      gtk_window_destroy(GTK_WINDOW(window));
    } else {
      gtk_widget_show(window);
    }
  }
}

static gboolean drop(GtkDropTarget *target, const GValue *value,
                     double x, double y, gpointer user_data) {
  GtkEntry *entry = user_data;

  if (G_VALUE_HOLDS(value, G_TYPE_STRING)) {
    const gchar *str = g_value_get_string(value);
    char *new_str;
    char *p;

    if (((p = strchr(str, '\r')) || (p = strchr(str, '\n'))) &&
        (new_str = alloca(p - str + 1))) {
      strncpy(new_str, str, p - str);
      new_str[p - str] = '\0';
      str = new_str;
    }

    gtk_entry_set_text(entry, str);
  }

  /* No further processing is necessary. */
  return FALSE;
}

#else

static void drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                               GtkSelectionData *data, guint info, guint time) {
  gchar **uris;
  gchar *filename;

#if GTK_CHECK_VERSION(2, 14, 0)
  uris = g_uri_list_extract_uris(gtk_selection_data_get_data(data));
#else
  uris = g_uri_list_extract_uris(data->data);
#endif
  filename = g_filename_from_uri(uris[0], NULL, NULL);
  gtk_entry_set_text(GTK_ENTRY(widget), filename);
  g_free(filename);
  g_strfreev(uris);
}

#endif

static void ssh_scp_clicked(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *local_entry;
  GtkWidget *remote_entry;
#if GTK_CHECK_VERSION(4, 0, 0)
  GtkWidget *vbox;
  GtkDropTarget *target;

  gtk_widget_hide(window);
#else
  GtkWidget *content_area;
  gint res;
  GtkTargetEntry local_targets[] = {
      {"text/uri-list", 0, 0},
  };

  gtk_widget_hide(gtk_widget_get_toplevel(widget));
#endif

  dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "mlconfig");
  gtk_dialog_add_button(GTK_DIALOG(dialog), _("Send"), MY_RESPONSE_SEND);
  gtk_dialog_add_button(GTK_DIALOG(dialog), _("Recv"), MY_RESPONSE_RECV);
  gtk_dialog_add_button(GTK_DIALOG(dialog), _("Return"), MY_RESPONSE_RETURN);
  gtk_dialog_add_button(GTK_DIALOG(dialog), _("Exit"), MY_RESPONSE_EXIT);

#if GTK_CHECK_VERSION(4, 0, 0)
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);
  gtk_box_append(gtk_dialog_get_content_area(GTK_DIALOG(dialog)), vbox);
#elif GTK_CHECK_VERSION(2, 14, 0)
  content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
  content_area = GTK_DIALOG(dialog)->vbox;
#endif

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);

  label = gtk_label_new(_("Local"));
  gtk_widget_show(label);
  gtk_widget_set_size_request(label, 70, -1);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 1);

  local_entry = gtk_entry_new();
  gtk_widget_show(local_entry);
  gtk_widget_set_size_request(local_entry, 280, -1);

#if GTK_CHECK_VERSION(4, 0, 0)
  /*
   * XXX
   * I don't know why, but it is impossible to override "drop" event controller of
   * GtkText which GtkEntry internally uses.
   * (drop() callback is never called, while accept, enter, move and leave signals
   *  can be invoked.)
   */
  target = gtk_drop_target_new(G_TYPE_STRING, GDK_ACTION_COPY);
  g_signal_connect(target, "drop", G_CALLBACK(drop), local_entry);
  gtk_widget_add_controller(local_entry, GTK_EVENT_CONTROLLER(target));
#else
  gtk_drag_dest_set(local_entry, GTK_DEST_DEFAULT_ALL, local_targets, 1, GDK_ACTION_COPY);
  g_signal_connect(local_entry, "drag-data-received", G_CALLBACK(drag_data_received), NULL);
#endif

  gtk_box_pack_start(GTK_BOX(hbox), local_entry, FALSE, FALSE, 1);

#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_box_append(GTK_BOX(vbox), hbox);
#else
  gtk_container_add(GTK_CONTAINER(content_area), hbox);
#endif

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);

  label = gtk_label_new(_("Remote"));
  gtk_widget_show(label);
  gtk_widget_set_size_request(label, 70, -1);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 1);

  remote_entry = gtk_entry_new();
  gtk_widget_show(remote_entry);
  gtk_widget_set_size_request(remote_entry, 280, -1);

  gtk_box_pack_start(GTK_BOX(hbox), remote_entry, FALSE, FALSE, 1);

#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_box_append(GTK_BOX(vbox), hbox);
#else
  gtk_container_add(GTK_CONTAINER(content_area), hbox);
#endif

#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  dialog_arg.local_entry = local_entry;
  dialog_arg.remote_entry = remote_entry;
  g_signal_connect(dialog, "response", G_CALLBACK(dialog_cb), NULL);
  gtk_widget_show(dialog);
#else
  while ((res = dialog_post_process(dialog, gtk_dialog_run(GTK_DIALOG(dialog)),
                                    local_entry, remote_entry)) == -1);
  if (res == 0) {
    gtk_main_quit();
  } else {
    gtk_widget_destroy(dialog);
    gtk_widget_show_all(gtk_widget_get_toplevel(widget));
  }
#endif
}

#endif /* USE_LIBSSH2 */

static void pty_new_button_clicked(GtkWidget *widget, gpointer data) {
  mc_exec("open_pty");
  mc_flush(mc_io_set);

#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_window_destroy(GTK_WINDOW(window));
#else
  gtk_main_quit();
#endif
}

static void pty_button_clicked(GtkWidget *widget, gpointer data) {
  mc_select_pty();

  /*
   * As soon as  pty changed, stdout is also changed, but mlconfig couldn't
   * follow it.
   */
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_window_destroy(GTK_WINDOW(window));
#else
  gtk_main_quit();
#endif
}

#if !GTK_CHECK_VERSION(4, 0, 0)
static gboolean event(GtkWidget *widget, GdkEvent *event, gpointer data) {
  if (event->type == GDK_FOCUS_CHANGE && !((GdkEventFocus*)event)->in) {
    gtk_window_set_keep_above(GTK_WINDOW(widget), FALSE);
    g_signal_handlers_disconnect_by_func(widget, event, NULL);
  }

  return FALSE;
}
#endif

/*
 *  ********  Building GUI (lower part, independent buttons)  ********
 */

static void addbutton(const char *label, void (func)(GtkWidget*, gpointer), GtkWidget *box) {
  GtkWidget *button;
  button = gtk_button_new_with_label(label);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_hexpand(button, TRUE);
  gtk_widget_set_margin_start(button, 4);
  gtk_widget_set_margin_end(button, 4);
#endif
  g_signal_connect(button, "clicked", G_CALLBACK(func), NULL);
  gtk_widget_show(button);
  gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
}

static GtkWidget *apply_cancel_button(void) {
  GtkWidget *hbox;

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_widget_show(hbox);

  addbutton(_("Save&Exit"), saveexit_clicked, hbox);

  if (!mc_io_is_file()) {
    addbutton(_("Apply&Exit"), applyexit_clicked, hbox);
    addbutton(_("Apply"), apply_clicked, hbox);
  }

  addbutton(_("Cancel"), cancel_clicked, hbox);

  return hbox;
}

static GtkWidget *font_large_small(void) {
  GtkWidget *frame;
  GtkWidget *hbox;

  frame = gtk_frame_new(_("Font size (temporary)"));
  gtk_widget_show(frame);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(frame, 4);
  gtk_widget_set_margin_end(frame, 4);
#endif

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_frame_set_child(GTK_FRAME(frame), hbox);
#else
  gtk_container_add(GTK_CONTAINER(frame), hbox);
#endif

  addbutton(_("Larger"), larger_clicked, hbox);
  addbutton(_("Smaller"), smaller_clicked, hbox);

#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text(frame,
                              "If you change fonts from \"Select\" button in \"Font\" tab, "
                              "it is not recommended to change font size here.");
#endif

  return frame;
}

static GtkWidget *command(void) {
  GtkWidget *frame;
  GtkWidget *hbox;

  frame = gtk_frame_new(_("Command"));
  gtk_widget_show(frame);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(frame, 4);
  gtk_widget_set_margin_end(frame, 4);
#endif

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_frame_set_child(GTK_FRAME(frame), hbox);
#else
  gtk_container_add(GTK_CONTAINER(frame), hbox);
#endif

  addbutton(_("Full reset"), full_reset_clicked, hbox);
  addbutton(_("Snapshot"), snapshot_clicked, hbox);
#ifdef USE_LIBSSH2
  addbutton(_("SCP"), ssh_scp_clicked, hbox);
#endif

  return frame;
}

static GtkWidget *pty_list(void) {
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *config_widget;

  if ((config_widget = mc_pty_config_widget_new()) == NULL) {
    return NULL;
  }
  gtk_widget_show(config_widget);

  frame = gtk_frame_new(_("PTY List"));
  gtk_widget_show(frame);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(frame, 4);
  gtk_widget_set_margin_end(frame, 4);
#endif

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(hbox, 4);
  gtk_widget_set_margin_end(hbox, 4);
  gtk_frame_set_child(GTK_FRAME(frame), hbox);
#else
  gtk_container_add(GTK_CONTAINER(frame), hbox);
#endif

  addbutton(_(" New "), pty_new_button_clicked, hbox);
  addbutton(_("Select"), pty_button_clicked, hbox);

  gtk_box_pack_start(GTK_BOX(hbox), config_widget, TRUE, TRUE, 0);

  return frame;
}

/*
 *  ********  Building GUI (main part, page (tab)-separated widgets)  ********
 */

#if GTK_CHECK_VERSION(4, 0, 0)
static int show(GtkApplication *app, gpointer user_data)
#else
static int show(void)
#endif
{
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *hbox;
  GtkWidget *notebook;
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *config_widget;
  GtkWidget *separator;
  const char *gui = mc_get_gui();

#if GTK_CHECK_VERSION(4, 0, 0)
  window = gtk_application_window_new(app);
#else
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(window, "destroy", G_CALLBACK(end_application), NULL);
#endif
  gtk_window_set_title(GTK_WINDOW(window), _("mlterm configuration"));
  gtk_container_set_border_width(GTK_CONTAINER(window), 0);

  vbox = gtk_vbox_new(FALSE, 10);
  gtk_widget_show(vbox);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_window_set_child(GTK_WINDOW(window), vbox);
#else
  gtk_container_add(GTK_CONTAINER(window), vbox);
#endif

  /* whole screen (except for the contents of notebook) */

  notebook = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
  gtk_widget_show(notebook);
  gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

  separator = gtk_hseparator_new();
  gtk_widget_show(separator);
  gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(separator, 2);
  gtk_widget_set_margin_end(separator, 2);
#endif

  hbox = apply_cancel_button();
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(hbox, 2);
  gtk_widget_set_margin_end(hbox, 2);
#endif

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(hbox, 2);
  gtk_widget_set_margin_end(hbox, 2);
#endif

  frame = font_large_small();
  gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 5);
  frame = command();
  gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 5);

  frame = pty_list();
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

  /* contents of the "Encoding" tab */

  label = gtk_label_new(_("Encoding"));
  gtk_widget_show(label);
  vbox = gtk_vbox_new(FALSE, 3);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
  gtk_widget_show(vbox);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(vbox, 2);
  gtk_widget_set_margin_end(vbox, 2);
#endif

  config_widget = mc_char_encoding_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_auto_detect_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_im_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_ctl_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  config_widget = mc_flag_config_widget_new(MC_FLAG_COMB);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_flag_config_widget_new(MC_FLAG_DYNCOMB);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_flag_config_widget_new(MC_FLAG_RECVUCS);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_opentype_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_char_width_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  /* contents of the "Font" tab */

  label = gtk_label_new(_("Font"));
  gtk_widget_show(label);
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
  gtk_widget_show(vbox);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(vbox, 2);
  gtk_widget_set_margin_end(vbox, 2);
#endif

  config_widget = mc_font_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  config_widget = mc_space_config_widget_new(MC_SPACE_LINE);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_space_config_widget_new(MC_SPACE_LETTER);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_space_config_widget_new(MC_SPACE_UNDERLINE);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_space_config_widget_new(MC_SPACE_BASELINE);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_ratio_config_widget_new(MC_RATIO_SCREEN_WIDTH);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  /* contents of the "Background" tab */

  label = gtk_label_new(_("Background"));
  gtk_widget_show(label);
  vbox = gtk_vbox_new(FALSE, 3);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
  gtk_widget_show(vbox);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(vbox, 2);
  gtk_widget_set_margin_end(vbox, 2);
#endif

  config_widget = mc_bgtype_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  frame = gtk_frame_new(_("Picture/Transparent"));
  gtk_widget_show(frame);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
  vbox2 = gtk_vbox_new(FALSE, 3);
  gtk_widget_show(vbox2);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_frame_set_child(GTK_FRAME(frame), vbox2);
#else
  gtk_container_add(GTK_CONTAINER(frame), vbox2);
#endif
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);

  config_widget = mc_ratio_config_widget_new(MC_RATIO_BRIGHTNESS);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_ratio_config_widget_new(MC_RATIO_GAMMA);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);

  config_widget = mc_ratio_config_widget_new(MC_RATIO_CONTRAST);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_alpha_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_ratio_config_widget_new(MC_RATIO_FADE);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  /* contents of the "Color" tab */

  label = gtk_label_new(_("Color"));
  gtk_widget_show(label);
  vbox = gtk_vbox_new(FALSE, 3);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
  gtk_widget_show(vbox);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(vbox, 2);
  gtk_widget_set_margin_end(vbox, 2);
#endif

  config_widget = mc_cursor_color_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_substitute_color_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_vtcolor_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  /* contents of the "Scrollbar" tab */

  label = gtk_label_new(_("Scrollbar"));
  gtk_widget_show(label);
  vbox = gtk_vbox_new(FALSE, 3);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
  gtk_widget_show(vbox);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(vbox, 2);
  gtk_widget_set_margin_end(vbox, 2);
#endif

  config_widget = mc_radio_config_widget_new(MC_RADIO_SB_MODE);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  if (strcmp(gui, "quartz") != 0) {
    config_widget = mc_sb_view_config_widget_new();
    gtk_widget_show(config_widget);
    gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    config_widget = mc_color_config_widget_new(MC_COLOR_SBFG);
    gtk_widget_show(config_widget);
    gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 5);

    config_widget = mc_color_config_widget_new(MC_COLOR_SBBG);
    gtk_widget_show(config_widget);
    gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);
  }

  /* contents of the "Others" tab */

  label = gtk_label_new(_("Others"));
  gtk_widget_show(label);
  vbox = gtk_vbox_new(FALSE, 3);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
  gtk_widget_show(vbox);
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_margin_start(vbox, 2);
  gtk_widget_set_margin_end(vbox, 2);
#endif

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  config_widget = mc_tabsize_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_logsize_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  config_widget = mc_geometry_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_wordsep_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_click_interval_config_widget_new();
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_radio_config_widget_new(MC_RADIO_MOD_META_MODE);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_radio_config_widget_new(MC_RADIO_BELL_MODE);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_radio_config_widget_new(MC_RADIO_LOG_VTSEQ);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  if (strcmp(gui, "xlib") == 0) {
    config_widget = mc_flag_config_widget_new(MC_FLAG_CLIPBOARD);
    gtk_widget_show(config_widget);
    gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);
  }

  config_widget = mc_flag_config_widget_new(MC_FLAG_LOCALECHO);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_flag_config_widget_new(MC_FLAG_BLINKCURSOR);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_flag_config_widget_new(MC_FLAG_STATICBACKSCROLL);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  config_widget = mc_flag_config_widget_new(MC_FLAG_REGARDURIASWORD);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_flag_config_widget_new(MC_FLAG_BROADCAST);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(hbox), config_widget, FALSE, FALSE, 0);

  config_widget = mc_flag_config_widget_new(MC_FLAG_TRIMNEWLINE);
  gtk_widget_show(config_widget);
  gtk_box_pack_start(GTK_BOX(vbox), config_widget, FALSE, FALSE, 0);

#if !GTK_CHECK_VERSION(4, 0, 0)
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
#endif
#if !GTK_CHECK_VERSION(2, 90, 0)
  gtk_window_set_policy(GTK_WINDOW(window), 0, 0, 0);
#endif

  gtk_widget_show(window);

#if !GTK_CHECK_VERSION(4, 0, 0)
  if (strcmp(gui, "win32") == 0 || strcmp(gui, "quartz") == 0) {
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    g_signal_connect(window, "event", G_CALLBACK(event), NULL);
  }

  gtk_main();
#endif

  return 1;
}

/* --- global functions --- */

int main(int argc, char **argv) {
  int is_file_io = 0;
#if GTK_CHECK_VERSION(4, 0, 0)
  GtkApplication *app;
  int status;
#endif

#ifdef DEBUG
  atexit(check_mem_leak);
#endif

  /*
   * Loading libim-*.so after gtk im module is loaded might break internal states
   * in gtk or input method library.
   */
  mc_im_init();

#if !GTK_CHECK_VERSION(2, 90, 0)
  gtk_set_locale();
#endif

  bindtextdomain("mlconfig", LOCALEDIR);
  bind_textdomain_codeset("mlconfig", "UTF-8");
  textdomain("mlconfig");

  if (argc == 1) {
    is_file_io = 1;
  } else {
    int count;

    for (count = 0; count < argc; count++) {
#ifdef __DEBUG
      fprintf(stderr, "%s\n", argv[count]);
#endif

      if (strcmp(argv[count], "--file") == 0) {
        is_file_io = 1;
      }
#ifdef GTK_CHECK_VERSION(4, 0, 0)
      else if (strcmp(argv[count], "--geometry") == 0 || strcmp(argv[count], "--display") == 0) {
        if (count + 1 < argc) {
          memmove(argv + count, argv + count + 2, sizeof(*argv) * (argc - count - 1));
          argc -= 2;
        } else {
          argv[count] = NULL;
          argc = count;
        }
        count--;
      }
#endif
    }
  }
  mc_io_set_use_file(is_file_io);

  bl_set_msg_log_file_name("mlterm/msg.log");

#if GTK_CHECK_VERSION(4, 0, 0)
  app = gtk_application_new("jp.mlterm", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(show), NULL);
  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
#else
  gtk_init(&argc, &argv);

  if (show() == 0) {
    bl_msg_printf("Starting mlconfig failed.\n");

    return 1;
  } else {
    return 0;
  }
#endif
}
