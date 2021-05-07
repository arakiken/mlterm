/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_wordsep.h"

#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_mem.h> /* free */
#include <pobl/bl_debug.h>
#include <glib.h>
#include <c_intl.h>

#include "mc_compat.h"
#include "mc_io.h"

#if 0
#define __DEBUG
#endif

#define CHAR_WIDTH 10

/* --- static variables --- */

static GtkWidget *entry;
static char *old_wordsep;
static int is_changed;

/* --- static funcitons --- */

static void set_str_value(const char *value) {
  size_t len;

  if ((len = strlen(value)) > 0) {
    char *value2;
    char *p;

    if (len < 3 && strchr(value, ' ')) {
      /* len must be more than 2 to hold ' ' between other characters. */
      if ((value2 = alloca(4))) {
        value = strcpy(value2, value);
        for (; len < 3; len++) {
          value2[len] = '\xff';
        }
        value2[len] = '\0';
      } else {
        return;
      }
    }

    if (((*value == ' ' && strchr(value + 1, ' ') == NULL) ||
         (value[len - 1] == ' ' && strchr(value, ' ') == value + len - 1)) &&
        (value2 = alloca(len + 1))) {
      /* Hold ' ' between other characters. */
      if (*value == ' ') {
        value2[0] = value[1];
        value2[1] = value[0];
        strcpy(value2 + 2, value + 2);
      } else {
        strncpy(value2, value, len - 2);
        value2[len - 1] = value[len - 2];
        value2[len - 2] = value[len - 1];
        value2[len] = '\0';
      }

      value = value2;
    }

    /* ';' => \x3b */
    if ((p = strchr(value, ';'))) {
      if ((value2 = alloca(len + 3 + 1))) {
        strncpy(value2, value, p - value);
        strcpy(value2 + (p - value), "\\x3b");
        strcpy(value2 + (p - value) + 4, p + 1);

        p = value2 + (p - value) + 4;
        while ((p = strchr(p, ';'))) {
          memmove(p, p + 1, strlen(p + 1));
          p++;
        }

        value = value2;
      }
    }
  }

  mc_set_str_value("word_separators", value);
}

static char *remove_ff_mark(char *value) {
  size_t len;

  if ((len = strlen(value)) > 0) {
    if (*value == '\xff') {
      memmove(value, value + 1, --len);
    }

    if (value[len - 1] == '\xff') {
      value[len - 1] = '\0';
    }
  }

  return value;
}

/* --- global functions --- */

GtkWidget *mc_wordsep_config_widget_new(void) {
  GtkWidget *hbox;
  GtkWidget *label;

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);

  label = gtk_label_new(_("Word separators"));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

  entry = gtk_entry_new();
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_widget_set_halign(entry, GTK_ALIGN_START);
#endif
  old_wordsep = remove_ff_mark(mc_get_str_value("word_separators"));
  gtk_entry_set_text(GTK_ENTRY(entry), old_wordsep);
  gtk_widget_show(entry);
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 1);

#if GTK_CHECK_VERSION(2, 90, 0)
  /* gtk_entry_set_width_chars(GTK_ENTRY(entry), 10) truncates text within 10 chars. */
#else
  gtk_widget_set_size_request(entry, 10 * CHAR_WIDTH, -1);
#endif
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text(entry, "ASCII characters only");
#endif

  return hbox;
}

void mc_update_wordsep(void) {
  const char *new_wordsep;

  new_wordsep = gtk_entry_get_text(GTK_ENTRY(entry));

  if (strcmp(new_wordsep, old_wordsep)) {
    is_changed = 1;
  }

  if (is_changed) {
    free(old_wordsep);
    set_str_value((old_wordsep = strdup(new_wordsep)));
  }
}
