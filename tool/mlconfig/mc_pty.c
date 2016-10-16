/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_pty.h"

#include <stdio.h>
#include <pobl/bl_str.h>
#include <pobl/bl_mem.h> /* malloc */
#include <pobl/bl_str.h> /* bl_str_sep */

#include "mc_combo.h"
#include "mc_io.h"

#define MAX_TERMS (sizeof(long) * 8) /* this must coincide with xwindow/x_term_manager.c */

/* --- static variables --- */

static char *new_pty = NULL;
static char *old_pty = NULL;

/* --- static functions --- */

static gint selected(GtkWidget *widget, gpointer data) {
  g_free(new_pty);
  new_pty = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);

  return 1;
}

static char *get_pty_title(char *dev) {
  char query[256], *name;

  if (strlen(dev) > 256 - 10) return strdup("");

  sprintf(query, "%s:pty_name", dev);
  name = mc_get_str_value(query);
  return name;
}

static char *get_pty_entry(char *dev) {
  char *title, *entry;

  title = get_pty_title(dev);

  /* Assumed that dev always starts with "/dev/". */
  if (title == NULL) return dev + 5;
  if (strcmp(title, dev) == 0 || strlen(title) == 0) {
    free(title);
    return dev + 5;
  }

  entry = malloc(strlen(dev + 5) + strlen(title) + 4);
  if (entry) {
    sprintf(entry, "%s (%s)", dev + 5, title);
    free(title);
    return entry;
  } else {
    free(title);
    return dev + 5;
  }
}

/* --- global functions --- */

GtkWidget *mc_pty_config_widget_new(void) {
  char *my_pty;
  char *pty_list;
  char *ptys[MAX_TERMS];
  int num;
  GtkWidget *combo;
  GtkWidget *entry;

  my_pty = mc_get_str_value("pty_name");
  pty_list = mc_get_str_value("pty_list");

  if (my_pty == NULL) return NULL;

  ptys[0] = get_pty_entry(my_pty);

  num = 1;
  while (pty_list) {
    char *p;

    if (strlen(pty_list) <= 5) break;

    p = strchr(pty_list, ':');
    if (!p) break;
    if (*(p + 1) == '0') {
      *p = 0;
      ptys[num] = get_pty_entry(pty_list);
      num++;
    }
    pty_list = strchr(p + 1, ';');
    if (pty_list) pty_list++;
  }

  new_pty = strdup(old_pty = strdup(my_pty + 5));

  combo = mc_combo_new("", ptys, num, new_pty, 1, &entry);
  g_signal_connect(entry, "changed", G_CALLBACK(selected), NULL);

  return combo;
}

void mc_select_pty(void) {
  if (strcmp(new_pty, old_pty) != 0) {
    char *cmd;
    char *space;

    if ((cmd = alloca(11 + 5 + strlen(new_pty) + 1)) == NULL) {
      return;
    }

    sprintf(cmd, "select_pty /dev/%s", new_pty);
    space = strchr(cmd + 11, ' ');
    if (space) *space = 0;

    mc_exec(cmd);

    free(old_pty);
    old_pty = strdup(new_pty);
  }
}
