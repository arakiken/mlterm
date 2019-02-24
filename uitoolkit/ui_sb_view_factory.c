/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_sb_view_factory.h"

#include <stdio.h> /* sprintf */
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_mem.h> /* alloca */
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_debug.h>
#include <pobl/bl_conf_io.h>

#include "ui_simple_sb_view.h"
#include "ui_imagelib.h"

#ifndef LIBDIR
#define SBLIB_DIR "/usr/local/lib/mlterm/"
#else
#define SBLIB_DIR LIBDIR "/mlterm/"
#endif

#ifndef XDATADIR
#define SB_DIR "/usr/local/share/mlterm/scrollbars"
#else
#define SB_DIR XDATADIR "/mlterm/scrollbars"
#endif

typedef ui_sb_view_t* (*ui_sb_view_new_func_t)(void);
typedef ui_sb_view_t* (*ui_sb_engine_new_func_t)(ui_sb_view_conf_t *conf, int is_transparent);

/* --- static variables --- */

#ifdef SUPPORT_PIXMAP_ENGINE
static ui_sb_view_conf_t **view_confs;
static u_int num_view_confs;
#endif

/* --- static functions --- */

static inline ui_sb_view_t *check_version(ui_sb_view_t *view) {
  return view->version == 1 ? view : NULL;
}

static ui_sb_view_new_func_t dlsym_sb_view_new_func(const char *name, int is_transparent) {
  bl_dl_handle_t handle;
  char *symbol;
  u_int len;

#ifdef PLUGIN_MODULE_SUFFIX
  char *p;

  if (!(p = alloca(strlen(name) + 3 + 1))) {
    return NULL;
  }

  sprintf(p, "%s-" PLUGIN_MODULE_SUFFIX, name);

  if (!(handle = bl_dl_open(SBLIB_DIR, p)) && !(handle = bl_dl_open("", p)))
#else
  if (!(handle = bl_dl_open(SBLIB_DIR, name)) && !(handle = bl_dl_open("", name)))
#endif
  {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " bl_dl_open(%s) failed.\n", name);
#endif

    return NULL;
  }

  bl_dl_close_at_exit(handle);

  len = 27 + strlen(name) + 1;
  if ((symbol = alloca(len)) == NULL) {
    return NULL;
  }

  if (is_transparent) {
    sprintf(symbol, "ui_%s_transparent_sb_view_new", name);
  } else {
    sprintf(symbol, "ui_%s_sb_view_new", name);
  }

  return (ui_sb_view_new_func_t)bl_dl_func_symbol(handle, symbol);
}

/*
 * pixmap_engine is supported only if mlterm is built with -export-dynamic
 * option of ld
 * because shared library of pixmap_engine refers to ui_imagelib_load_file.
 */
#ifdef SUPPORT_PIXMAP_ENGINE

static ui_sb_engine_new_func_t dlsym_sb_engine_new_func(const char *name) {
  ui_sb_engine_new_func_t func;
  bl_dl_handle_t handle;
  char *symbol;
  u_int len;

#ifdef PLUGIN_MODULE_SUFFIX
  char *p;

  if (!(p = alloca(strlen(name) + 3 + 1))) {
    return NULL;
  }

  sprintf(p, "%s-" PLUGIN_MODULE_SUFFIX, name);

  if (!(handle = bl_dl_open(SBLIB_DIR, name)) && !(handle = bl_dl_open("", name)))
#else
  if (!(handle = bl_dl_open(SBLIB_DIR, name)) && !(handle = bl_dl_open("", name)))
#endif
  {
    return NULL;
  }

  bl_dl_close_at_exit(handle);

  len = 16 + strlen(name) + 1;
  if ((symbol = alloca(len)) == NULL) {
    return NULL;
  }

  sprintf(symbol, "ui_%s_sb_engine_new", name);

  if ((func = (ui_sb_engine_new_func_t)bl_dl_func_symbol(handle, symbol)) == NULL) {
    return NULL;
  }

  return func;
}

static ui_sb_view_conf_t *search_view_conf(const char *sb_name) {
  u_int count;

  for (count = 0; count < num_view_confs; count++) {
    if (strcmp(view_confs[count]->sb_name, sb_name) == 0) {
      return view_confs[count];
    }
  }

  return NULL;
}

static void free_conf(ui_sb_view_conf_t *conf) {
  ui_sb_view_rc_t *rc;
  int i;

  free(conf->sb_name);
  free(conf->engine_name);
  free(conf->dir);

  for (rc = conf->rc, i = 0; i < conf->rc_num; rc++, i++) {
    free(rc->key);
    free(rc->value);
  }

  free(conf->rc);

  free(conf);
}

static ui_sb_view_conf_t *register_new_view_conf(bl_file_t *rcfile, const char *sb_name,
                                                 char *rcfile_path) {
  ui_sb_view_conf_t *conf;
  char *key;
  char *value;
  int len;
  void *p;

  if ((conf = calloc(1, sizeof(ui_sb_view_conf_t))) == NULL) {
    return NULL;
  }

  conf->load_image = ui_imagelib_load_file;

  conf->sb_name = strdup(sb_name);

  /* remove "/rc" /foo/bar/name/rc -> /foo/bar/name */
  len = strlen(rcfile_path) - 3;
  if ((conf->dir = malloc(sizeof(char) * (len + 1))) == NULL) {
    goto error;
  }
  strncpy(conf->dir, rcfile_path, len);
  conf->dir[len] = '\0';

  while (bl_conf_io_read(rcfile, &key, &value)) {
    if (strcmp(key, "engine") == 0) {
      /* Last "engine" parameter is effective. */
      free(conf->engine_name);
      conf->engine_name = strdup(value);
    } else {
      ui_sb_view_rc_t *p;

      if ((p = realloc(conf->rc, sizeof(ui_sb_view_rc_t) * (conf->rc_num + 1))) == NULL) {
#ifdef __DEBUG
        bl_debug_printf("realloc() failed.");
#endif

        goto error;
      }
      conf->rc = p;
      p = &conf->rc[conf->rc_num];
      p->key = strdup(key);
      p->value = strdup(value);
      conf->rc_num++;
    }
  }

  if (conf->engine_name == NULL) {
    goto error;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG "%s has been registered as new view. [dir: %s]\n", conf->sb_name,
                  conf->dir);
#endif

  if ((p = realloc(view_confs, sizeof(ui_sb_view_conf_t*) * (num_view_confs + 1))) == NULL) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " realloc failed.\n");
#endif

    goto error;
  }

  view_confs = p;
  view_confs[num_view_confs++] = conf;

  return conf;

error:
  free_conf(conf);
  return NULL;
}

static int unregister_view_conf(ui_sb_view_conf_t *conf) {
  u_int count;

  for (count = 0; count < num_view_confs; count++) {
    if (view_confs[count] == conf) {
      free_conf(conf);
      view_confs[count] = view_confs[--num_view_confs];

      if (num_view_confs == 0) {
        free(view_confs);
        view_confs = NULL;
      }
    }
  }

  return 1;
}

static ui_sb_view_conf_t *find_view_rcfile(const char *name) {
  ui_sb_view_conf_t *conf;
  bl_file_t *rcfile;
  char *user_dir;
  char *path;

  /* search known conf from view_conf_list */
  if ((conf = search_view_conf(name))) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG "%s was found in view_conf_list\n", sb_name);
#endif

    return conf;
  }

  if (!(user_dir = bl_get_user_rc_path("mlterm/scrollbars"))) {
    return NULL;
  }

  if (!(path = malloc(strlen(user_dir) + strlen(name) + 5))) {
    free(user_dir);
    return NULL;
  }

  sprintf(path, "%s/%s/rc", user_dir, name);
  free(user_dir);

  if (!(rcfile = bl_file_open(path, "r"))) {
    void *p;

    if (!(p = realloc(path, strlen(SB_DIR) + strlen(name) + 5))) {
      free(path);
      return NULL;
    }

    path = p;

    sprintf(path, "%s/%s/rc", SB_DIR, name);

    if (!(rcfile = bl_file_open(path, "r"))) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG "rcfile for %s could not be found\n", name);
#endif
      free(path);
      return NULL;
    }
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG "rcfile for %s: %s\n", name, path);
#endif

  conf = register_new_view_conf(rcfile, name, path);

  free(path);

  bl_file_close(rcfile);

  return conf;
}

#endif

/* --- global functions --- */

ui_sb_view_t *ui_sb_view_new(const char *name) {
  ui_sb_view_new_func_t func;
#ifdef SUPPORT_PIXMAP_ENGINE
  ui_sb_view_conf_t *conf;

  /* new style plugin ? (requires rcfile and engine library) */
  if ((conf = find_view_rcfile(name))) {
    ui_sb_engine_new_func_t func_engine;

    if ((func_engine = dlsym_sb_engine_new_func(conf->engine_name)) == NULL) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " %s scrollbar failed.\n", name);
#endif

      unregister_view_conf(conf);

      return NULL;
    }

    /*
     * Increment conf->use_count in func_engine().
     * Decrement conf->use_count in ui_sb_view_t::destroy().
     */
    return check_version((*func_engine)(conf, 0));
  }
#endif

  if (strcmp(name, "simple") == 0) {
    return check_version(ui_simple_sb_view_new());
  } else if ((func = dlsym_sb_view_new_func(name, 0)) == NULL) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " %s scrollbar failed.\n", name);
#endif

    return NULL;
  }

  return check_version((*func)());
}

ui_sb_view_t *ui_transparent_sb_view_new(const char *name) {
  ui_sb_view_new_func_t func;
#ifdef SUPPORT_PIXMAP_ENGINE
  ui_sb_view_conf_t *conf;

  /* new style plugin? (requires an rcfile and an engine library) */
  if ((conf = find_view_rcfile(name))) {
    ui_sb_engine_new_func_t func_engine;

    if ((func_engine = dlsym_sb_engine_new_func(conf->engine_name)) == NULL) {
      unregister_view_conf(conf);

      return NULL;
    }

    return check_version((*func_engine)(conf, 1));
  }
#endif

  if (strcmp(name, "simple") == 0) {
    return check_version(ui_simple_transparent_sb_view_new());
  } else if ((func = dlsym_sb_view_new_func(name, 1)) == NULL) {
    return NULL;
  }

  return check_version((*func)());
}

/*
 * This function cleans up configurations of pixmap_engine.
 * Call this function after ui_sb_view_t::destroy() is called.
 */
void ui_unload_scrollbar_view_lib(const char *name) {
#ifdef SUPPORT_PIXMAP_ENGINE
  ui_sb_view_conf_t *conf;

  /* new style plugin? (requires an rcfile and an engine library) */
  if ((conf = search_view_conf(name))) {
    /* remove unused conf */

    if (conf->use_count == 0) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG
                      " %s(pixmap_engine) is no longer used. removing from "
                      "view_conf_list\n",
                      name);
#endif

      unregister_view_conf(conf);
    }
#ifdef __DEBUG
    else {
      bl_debug_printf(BL_DEBUG_TAG " %s(pixmap_engine) is still being used. [use_count: %d]\n",
                      name, conf->use_count);
    }
#endif
  }
#endif
}
