/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_im.h"

#include <pobl/bl_mem.h> /* free */
#include <pobl/bl_debug.h>
#include <pobl/bl_file.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_map.h>
#include <pobl/bl_str.h>
#include <pobl/bl_def.h> /* USE_WIN32API */
#include <glib.h>
#include <c_intl.h>
#include <dirent.h>
#include <im_info.h>

#include "mc_combo.h"
#include "mc_io.h"

#if 0
#define __DEBUG
#endif

#if defined(USE_WIN32API)
#define CONFIG_PATH "."
#elif defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif

#ifndef LIBDIR
#define IM_DIR "/usr/local/lib/mlterm/"
#else
#define IM_DIR LIBDIR "/mlterm/"
#endif

#define STR_LEN 256

BL_MAP_TYPEDEF(xim_locale, char *, char *);

#define IM_MAX 20

typedef enum im_type {
  IM_NONE = 0,
  IM_XIM = 1,
  IM_OTHER = 2,

  IM_TYPE_MAX = IM_MAX

} im_type_t;

typedef im_info_t *(*im_get_info_func_t)(char *, char *);

/* --- static variables --- */

static char *cur_locale;

static im_type_t im_type;
static im_type_t cur_im_type;

static char **xims;
static char **locales;
static u_int num_xims;

static int is_changed = 0;

static char xim_auto_str[STR_LEN] = "";
static char current_locale_str[STR_LEN] = "";
static char selected_xim_name[STR_LEN] = "";
static char selected_xim_locale[STR_LEN] = "";

static im_info_t *im_info_table[IM_MAX];
static u_int num_info = 0;
static im_info_t *selected_im = NULL;
static int selected_im_arg = -1;

static GtkWidget **im_opt_widget;

static GtkWidget *skk_dict_entry;
static GtkWidget *skk_sskey_entry;
static GtkWidget *wnn_serv_entry;

/* --- static functions --- */

static int is_im_plugin(char *file_name) {
  if (bl_dl_is_module(file_name) && strstr(file_name, "im-")) {
    return 1;
  }

  return 0;
}

#ifdef USE_WIN32API
static im_info_t *get_kbd_info(char *locale, char *encoding) {
  im_info_t *result;

  if (!(result = malloc(sizeof(im_info_t)))) {
    return NULL;
  }

  result->num_args = 13;

  if (!(result->args = malloc(sizeof(char *) * result->num_args))) {
    free(result);
    return NULL;
  }

  if (!(result->readable_args = malloc(sizeof(char *) * result->num_args))) {
    free(result->args);
    free(result);
    return NULL;
  }

  result->readable_args[0] = strdup("Unknown");
  result->readable_args[1] = strdup("Arabic");
  result->readable_args[2] = strdup("Hebrew");
  result->readable_args[3] = strdup("Indic (ASSAMESE)");
  result->readable_args[4] = strdup("Indic (BENGALI)");
  result->readable_args[5] = strdup("Indic (GUJARATI)");
  result->readable_args[6] = strdup("Indic (HINDI)");
  result->readable_args[7] = strdup("Indic (KANNADA)");
  result->readable_args[8] = strdup("Indic (MALAYALAM)");
  result->readable_args[9] = strdup("Indic (ORIYA)");
  result->readable_args[10] = strdup("Indic (PUNJABI)");
  result->readable_args[11] = strdup("Indic (TAMIL)");
  result->readable_args[12] = strdup("Indic (TELUGU)");

  result->args[0] = strdup("");
  result->args[1] = strdup("arabic");
  result->args[2] = strdup("hebrew");
  result->args[3] = strdup("isciiassamese");
  result->args[4] = strdup("isciibengali");
  result->args[5] = strdup("isciigujarati");
  result->args[6] = strdup("isciihindi");
  result->args[7] = strdup("isciikannada");
  result->args[8] = strdup("isciimalayalam");
  result->args[9] = strdup("isciioriya");
  result->args[10] = strdup("isciipunjabi");
  result->args[11] = strdup("isciitamil");
  result->args[12] = strdup("isciitelugu");

  result->id = strdup("kbd");
  result->name = strdup("keyboard");

  return result;
}
#endif

static int get_im_info(char *locale, char *encoding) {
  char *im_dir_path;
  DIR *dir;
  struct dirent *d;
  const char *gui;

  if ((dir = opendir(IM_DIR))) {
    im_dir_path = IM_DIR;
#if 0
  } else if ((dir = opendir("."))) {
    im_dir_path = "";
#endif
  } else {
#ifdef USE_WIN32API
    if ((im_info_table[num_info] = get_kbd_info(locale, encoding))) {
      num_info++;
      return 1;
    }
#endif
    return 0;
  }

  gui = mc_get_gui();

  while ((d = readdir(dir))) {
    im_info_t *info;
    char symname[100];
    char *p;
    char *end;
    bl_dl_handle_t handle;

    if (d->d_name[0] == '.' || !is_im_plugin(d->d_name)) continue;

    /* libim-foo.so -> libim-foo */
    if (!(p = strchr(d->d_name, '.'))) continue;
    *p = '\0';

    /* libim-foo -> im-foo */
    if (!(p = strstr(d->d_name, "im-"))) continue;

    end = p + strlen(p);
    if (strcmp(gui, "sdl2") == 0) {
      end -= 5;
      if (strcmp(end, "-sdl2") != 0) {
        continue;
      }
    } else if (strcmp(gui, "fb") == 0 || strcmp(gui, "console") == 0 ||
               strcmp(gui, "wayland") == 0) {
      end -= 3;
      if (strcmp(end, *gui == 'w' ? "-wl" : "-fb") != 0) {
        continue;
      }
    }

    if ((handle = bl_dl_open(im_dir_path, p))) {
      im_get_info_func_t func;

      *end = '\0';
      snprintf(symname, 100, "im_%s_get_info", &p[3]);

      if ((func = (im_get_info_func_t)bl_dl_func_symbol(handle, symname))) {
        if ((info = (*func)(locale, encoding))) {
          im_info_table[num_info] = info;
          num_info++;
        }
      }

      bl_dl_close(handle);
    }
  }

  return 0;
}

/*
 * XIM
 */

static char *get_xim_locale(char *xim) {
  int count;

  for (count = 0; count < num_xims; count++) {
    if (strcmp(xims[count], xim) == 0) {
      return locales[count];
    }
  }

  return NULL;
}

static gint xim_selected(GtkWidget *widget, gpointer data) {
  char *locale;

  snprintf(selected_xim_name, STR_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(widget)));

  if ((locale = get_xim_locale(selected_xim_name))) {
    gtk_entry_set_text(GTK_ENTRY(data), locale);
  } else {
    gtk_entry_set_text(GTK_ENTRY(data), current_locale_str);
  }

  snprintf(selected_xim_locale, STR_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(data)));

  is_changed = 1;

  return 1;
}

static int read_xim_conf(BL_MAP(xim_locale) xim_locale_table, char *filename) {
  bl_file_t *from;
  char *key;
  char *value;
  BL_PAIR(xim_locale) pair;
  int result;

  if (!(from = bl_file_open(filename, "r"))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " %s couldn't be opened.\n", filename);
#endif

    return 0;
  }

  while (bl_conf_io_read(from, &key, &value)) {
    bl_map_get(xim_locale_table, key, pair);

    if (pair) {
      free(pair->value);
      pair->value = strdup(value);
    } else {
      key = strdup(key);
      value = strdup(value);

      bl_map_set(result, xim_locale_table, key, value);
    }
  }

  bl_file_close(from);

  return 1;
}

static GtkWidget *xim_widget_new(const char *xim_name, const char *xim_locale,
                                 const char *cur_locale) {
  BL_MAP(xim_locale) xim_locale_table;
  BL_PAIR(xim_locale) * array;
  u_int size;
  char *rcpath;
  char *default_xim_name;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *combo;
  GtkWidget *combo_entry;
  int count;

  default_xim_name = mc_get_str_value("default_xim_name");
  snprintf(xim_auto_str, STR_LEN, _("auto (currently %s)"), default_xim_name);
  free(default_xim_name);

  /*
   * create known xim list from <prefix>/etc/mlterm/xim and/or
   * ~/.mlterm/xim.
   */

  bl_map_new_with_size(char *, char *, xim_locale_table, bl_map_hash_str, bl_map_compare_str, 16);

  bl_set_sys_conf_dir(CONFIG_PATH);

  if ((rcpath = bl_get_sys_rc_path("mlterm/xim"))) {
    read_xim_conf(xim_locale_table, rcpath);
    free(rcpath);
  }

  if ((rcpath = bl_get_user_rc_path("mlterm/xim"))) {
    read_xim_conf(xim_locale_table, rcpath);
    free(rcpath);
  }

  bl_map_get_pairs_array(xim_locale_table, array, size);

  if ((xims = malloc(sizeof(char *) * (size + 1))) == NULL) return NULL;

  if ((locales = malloc(sizeof(char *) * (size + 1))) == NULL) {
    free(xims);
    return NULL;
  }

  for (count = 0; count < size; count++) {
    xims[count] = array[count]->key;
    locales[count] = array[count]->value;
  }

  xims[count] = xim_auto_str;
  locales[count] = NULL;

  num_xims = size + 1;

  bl_map_destroy(xim_locale_table);

  /*
   * create widgets
   */

  vbox = gtk_vbox_new(FALSE, 5);

  snprintf(current_locale_str, STR_LEN, _("auto (currently %s)"), cur_locale);
  entry = gtk_entry_new();
  snprintf(selected_xim_locale, STR_LEN, "%s", xim_locale ? xim_locale : current_locale_str);
  gtk_entry_set_text(GTK_ENTRY(entry), selected_xim_locale);

  snprintf(selected_xim_name, STR_LEN, "%s", xim_name ? xim_name : xim_auto_str);
  combo = mc_combo_new(_("XIM Server"), xims, num_xims, selected_xim_name, 0, &combo_entry);
  g_signal_connect(combo_entry, "changed", G_CALLBACK(xim_selected), entry);

  label = gtk_label_new(_("XIM locale"));

  hbox = gtk_hbox_new(FALSE, 5);

  gtk_box_pack_start(GTK_BOX(vbox), combo, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

  gtk_widget_show(combo);
  gtk_widget_show(entry);
  gtk_widget_show(label);
  gtk_widget_show(hbox);

  return vbox;
}

/*
 * pluggable ims
 */

static gint im_selected(GtkWidget *widget, gpointer data) {
  const char *str;
  int i;

  if (selected_im == NULL) return 1;

  str = (const char *)gtk_entry_get_text(GTK_ENTRY(widget));

  for (i = 0; i < selected_im->num_args; i++)
    if (strcmp(selected_im->readable_args[i], str) == 0) selected_im_arg = i;

  is_changed = 1;

  return 1;
}

static GtkWidget *im_widget_new(int nth_im, const char *value, char *locale) {
  GtkWidget *combo;
  GtkWidget *entry;
  im_info_t *info;
  int i;
  int selected = 0;
  size_t len;

  info = im_info_table[nth_im];

  if (value) {
    for (i = 1; i < info->num_args; i++) {
      if (strcmp(info->args[i], value) == 0) {
        selected = i;
      }
    }
  }

  if (!info->num_args) return NULL;

  if (!value || (value && selected)) {
    char *auto_str;

    /*
     * replace gettextized string
     */
    len = strlen(_("auto (currently %s)")) + strlen(info->readable_args[0]) + 1;

    if ((auto_str = malloc(len))) {
      snprintf(auto_str, len, _("auto (currently %s)"), info->readable_args[0]);
      free(info->readable_args[0]);
      info->readable_args[0] = auto_str;
    }
  } else {
    free(info->readable_args[0]);
    info->readable_args[0] = strdup(value);
  }

  combo = mc_combo_new(_("Option"), info->readable_args, info->num_args,
                       info->readable_args[selected], 1, &entry);
  g_signal_connect(entry, "changed", G_CALLBACK(im_selected), NULL);

  return combo;
}

static gint entry_selected(GtkWidget *widget, gpointer data) {
  is_changed = 1;

  return 1;
}

static GtkWidget *entry_with_label_new(GtkWidget *parent, const char *text, const char *value) {
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(parent), hbox, TRUE, TRUE, 0);
  gtk_widget_show(hbox);

  label = gtk_label_new(text);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry), value);
  g_signal_connect(entry, "changed", G_CALLBACK(entry_selected), entry);
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show(entry);

  return entry;
}

static GtkWidget *combo_with_label_new(GtkWidget *parent, const char *label_name,
                                       char **item_names, u_int item_num, const char *value) {
  GtkWidget *hbox;
  GtkWidget *combo;
  GtkWidget *entry;

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(parent), hbox, TRUE, TRUE, 0);
  gtk_widget_show(hbox);

  combo = mc_combo_new(label_name, item_names, item_num, value, 0, &entry);
  gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
  gtk_widget_show(combo);
  g_signal_connect(entry, "changed", G_CALLBACK(entry_selected), entry);

  return entry;
}

static GtkWidget *skk_widget_new(char *value) {
  GtkWidget *vbox;
  char *dict = NULL;
  char *sskey = NULL;
  char buf[2];
  char *cands[] = { ";", ":", "Muhenkan", "Henkan", };

  /* Same processing as im_skk_new() in im_skk.c */
  if (value) {
#if 1
    /* XXX Compat with 3.8.3 or before. */
    if (!strchr(value, '=')) {
      dict = value;
    } else
#endif
    {
      char *next;

      do {
        if ((next = strchr(value, ','))) {
          *(next++) = '\0';
        }

        if (strncmp(value, "sskey=", 6) == 0) {
          int digit;
          sskey = value + 6;
          if (sscanf(sskey, "\\x%x", &digit) == 1) {
            buf[0] = digit;
            buf[1] = '\0';
            sskey = buf;
          }
        } else if (strncmp(value, "dict=", 5) == 0) {
          dict = value + 5;
        }
      } while ((value = next));
    }
  }

  vbox = gtk_vbox_new(FALSE, 5);
  skk_dict_entry = entry_with_label_new(vbox, _("Dictionary"), dict ? dict : "");
  skk_sskey_entry = combo_with_label_new(vbox, _("Sticky shift key"), cands,
                                         sizeof(cands) / sizeof(cands[0]),
                                         sskey ? sskey : "");

  return vbox;
}

static char *skk_current_value(void) {
  const char *dict;
  const char *sskey;
  size_t len = 4;
  char *value;
  char *p;

  dict = gtk_entry_get_text(GTK_ENTRY(skk_dict_entry));
  if (*dict) {
    len += (strlen(dict) + 5 + 1);
  }
  sskey = gtk_entry_get_text(GTK_ENTRY(skk_sskey_entry));
  if (*sskey) {
    len += (strlen(sskey) + 6 + 1);
  }

  value = p = malloc(len);

  strcpy(p, "skk");
  p += 3;

  if (*dict) {
    sprintf(p, ":dict=%s", dict);
    p += strlen(p);
  }
  if (*sskey) {
    char sep = *dict ? ',' : ':';

    if (strlen(sskey) == 1) {
      sprintf(p, "%csskey=\\x%.2x", sep, *sskey);
    } else {
      sprintf(p, "%csskey=%s", sep, sskey);
    }
  }

  return value;
}

static GtkWidget *wnn_widget_new(char *value) {
  GtkWidget *vbox;

  vbox = gtk_vbox_new(FALSE, 5);
  wnn_serv_entry = entry_with_label_new(vbox, _("Server"), value);

  return vbox;
}

static char *wnn_current_value(void) {
  const char *serv;
  size_t len = 4;
  char *value;

  serv = gtk_entry_get_text(GTK_ENTRY(wnn_serv_entry));
  if (*serv) {
    len += (strlen(serv) + 1);
  }

  value = malloc(len);

  if (*serv) {
    sprintf(value, "wnn:%s", serv);
  } else {
    strcpy(value, "wnn");
  }

  return value;
}

/*
 * callbacks for radio buttons of im type.
 */
static gint button_xim_checked(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    if (data) gtk_widget_show(GTK_WIDGET(data));
    im_type = IM_XIM;
  } else {
    if (data) gtk_widget_hide(GTK_WIDGET(data));
  }

  is_changed = 1;

  return 1;
}

static gint button_im_checked(GtkWidget *widget, gpointer data) {
  int i;
  int idx = 0;

  if (data == NULL || num_info == 0) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
      im_type = IM_NONE;
    }
  } else {
    for (i = 0; i < num_info; i++)
      if (im_info_table[i] == data) idx = i;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
      im_type = IM_OTHER + idx;
      selected_im = data;
      if (im_opt_widget[idx]) gtk_widget_show(GTK_WIDGET(im_opt_widget[idx]));
    } else {
      if (im_opt_widget[idx]) gtk_widget_hide(GTK_WIDGET(im_opt_widget[idx]));
    }
  }

  is_changed = 1;

  return 1;
}

/* --- global functions --- */

void mc_im_init(void) {
  char *encoding = mc_get_str_value("encoding");

  cur_locale = mc_get_str_value("locale"); /* XXX leaked */

  get_im_info(cur_locale, encoding);

  free(encoding);
}

GtkWidget *mc_im_config_widget_new(void) {
  char *xim_name = NULL;
  char *xim_locale = NULL;
  char *value;
  char *im_name;
  int i;
  int index = -1;
  GtkWidget *xim;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *radio;
  GSList *group;

  value = mc_get_str_value("input_method");

  im_name = bl_str_sep(&value, ":");

  im_type = IM_NONE;
  if (strncmp(im_name, "xim", 3) == 0) {
    im_type = IM_XIM;
    xim_name = bl_str_sep(&value, ":");
    xim_locale = bl_str_sep(&value, ":");
  } else if (strncmp(im_name, "none", 4) == 0) {
    /* do nothing */
  } else {
    for (i = 0; i < num_info; i++) {
      if (strcmp(im_name, im_info_table[i]->id) == 0) {
        index = i;
        im_type = IM_OTHER + i;
        break;
      }
    }
  }
  cur_im_type = im_type;

  if (strcmp(mc_get_gui(), "xlib") != 0) {
    xim = NULL;
  } else {
    xim = xim_widget_new(xim_name, xim_locale, cur_locale);
  }

  im_opt_widget = malloc(sizeof(GtkWidget*) * num_info);

  for (i = 0; i < num_info; i++) {
    if (strcmp(im_info_table[i]->id, "skk") == 0)
      im_opt_widget[i] = skk_widget_new(index == i ? value : "");
    else if (strcmp(im_info_table[i]->id, "wnn") == 0)
      im_opt_widget[i] = wnn_widget_new(index == i ? value : "");
    else
      im_opt_widget[i] = im_widget_new(i, index == i ? value : NULL, cur_locale);
  }

  free(im_name);

  frame = gtk_frame_new(_("Input Method"));

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_widget_show(vbox);

  hbox = gtk_hbox_new(FALSE, 5);
  radio = gtk_radio_button_new_with_label(NULL, xim ? "XIM" : "Default");
  g_signal_connect(radio, "toggled", G_CALLBACK(button_xim_checked), xim);
  gtk_widget_show(GTK_WIDGET(radio));
  gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
  if (im_type == IM_XIM) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);

  for (i = 0; i < num_info; i++) {
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
    radio = gtk_radio_button_new_with_label(group, im_info_table[i]->name);
    g_signal_connect(radio, "toggled", G_CALLBACK(button_im_checked), im_info_table[i]);
    gtk_widget_show(GTK_WIDGET(radio));
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
    if (index == i)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
  }

  if (strcmp(mc_get_gui(), "xlib") == 0) {
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
    radio = gtk_radio_button_new_with_label(group, _("None"));
    g_signal_connect(radio, "toggled", G_CALLBACK(button_im_checked), NULL);
    gtk_widget_show(GTK_WIDGET(radio));
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
    if (im_type == IM_NONE) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
  }

  gtk_widget_show(GTK_WIDGET(hbox));
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

  switch (im_type) {
    case IM_XIM:
      if (xim) gtk_widget_show(xim);
      for (i = 0; i < num_info; i++)
        if (im_opt_widget[i]) gtk_widget_hide(im_opt_widget[i]);
      break;
    case IM_NONE:
      if (xim) gtk_widget_hide(xim);
      for (i = 0; i < num_info; i++)
        if (im_opt_widget[i]) gtk_widget_hide(im_opt_widget[i]);
      break;
    default: /* OTHER */
      if (xim) gtk_widget_hide(xim);
      for (i = 0; i < num_info; i++) {
        if (!im_opt_widget[i]) continue;

        if (i == index)
          gtk_widget_show(im_opt_widget[i]);
        else
          gtk_widget_hide(im_opt_widget[i]);
      }
      break;
  }

  if (xim) gtk_box_pack_start(GTK_BOX(vbox), xim, TRUE, TRUE, 0);
  for (i = 0; i < num_info; i++)
    if (im_opt_widget[i]) gtk_box_pack_start(GTK_BOX(vbox), im_opt_widget[i], TRUE, TRUE, 0);

  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_container_add(GTK_CONTAINER(frame), vbox);

  is_changed = 0;

  return frame;
}

void mc_update_im(void) {
  char *p;
  size_t len;

  if (!is_changed) return;

  if (strcmp(selected_im->id, "skk") == 0) {
    p = skk_current_value();
    goto end;
  } else if (strcmp(selected_im->id, "wnn") == 0) {
    p = wnn_current_value();
    goto end;
  }

  if (im_type == cur_im_type && selected_im_arg == -1) {
    is_changed = 0;
    return;
  }

  if (selected_im_arg == -1) {
    is_changed = 0;
    selected_im_arg = 0;
  }

  switch (im_type) {
    case IM_XIM:
      if (strcmp(selected_xim_name, xim_auto_str) == 0) {
        p = strdup("xim");
      } else if (strcmp(selected_xim_locale, current_locale_str) == 0) {
        len = 3 + 1 + strlen(selected_xim_name) + 1;
        if (!(p = malloc(sizeof(char) * len))) return;
        sprintf(p, "xim:%s", selected_xim_name);
      } else {
        len = 3 + 1 + strlen(selected_xim_name) + 1 + strlen(selected_xim_locale) + 1;
        if (!(p = malloc(sizeof(char) * len))) return;
        sprintf(p, "xim:%s:%s", selected_xim_name, selected_xim_locale);
      }
      break;
    case IM_NONE:
      p = strdup("none");
      break;
    /* case IM_OTHER: */
    default:
      if (selected_im == NULL) return;
      if (selected_im_arg == 0) { /* auto */
        p = strdup(selected_im->id);
      } else {
        len = strlen(selected_im->id) + strlen(selected_im->args[selected_im_arg]) + 2;
        if (!(p = malloc(len))) return;
        sprintf(p, "%s:%s", selected_im->id, selected_im->args[selected_im_arg]);
      }
      break;
  }

end:
  mc_set_str_value("input_method", p);

  selected_im_arg = -1;
  cur_im_type = im_type;
  is_changed = 0;

  free(p);
}
