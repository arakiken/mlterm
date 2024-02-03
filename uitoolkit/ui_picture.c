/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef NO_IMAGE

#include "ui_picture.h"

#include <unistd.h> /* unlink */
#include <stdlib.h> /* system */
#include <fcntl.h>
#include <sys/stat.h>

#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>     /* strdup */
#include <pobl/bl_mem.h>     /* malloc */
#include <pobl/bl_util.h>    /* DIGIT_STR_LEN */
#include <pobl/bl_conf_io.h> /* bl_get_user_rc_path */

#ifdef HAVE_WINDOWS_H
#include <windows.h> /* for CreateEvent */
#endif

#if defined(USE_WIN32API)
#include <process.h> /* _beginthreadex */
#elif defined(HAVE_PTHREAD)
#include <pthread.h>
#endif

#if defined(HAVE_PTHREAD) || defined(USE_WIN32API)
#ifndef HAVE_WINDOWS_H
#include <sys/select.h>
#include <unistd.h>
#endif
#endif

#ifdef USE_XLIB
/* HAVE_WINDOWS_H is undefined on cygwin/x11 */
#undef HAVE_WINDOWS_H
#endif

#include "ui_imagelib.h"

#define DUMMY_PIXMAP ((Pixmap)1)
#define PIXMAP_IS_ACTIVE(inline_pic) ((inline_pic).pixmap && (inline_pic).pixmap != DUMMY_PIXMAP)

#define get_inline_picture(idx) ((idx) < num_inline_pics ? inline_pics + (idx) : NULL)

#if 0
#define __DEBUG
#endif

typedef struct inline_pic_args {
  int idx;
#ifdef HAVE_WINDOWS_H
  HANDLE ev;
#else
  int ev;
#endif
  int keep_aspect;

} inline_pic_args_t;

#ifdef USE_GRF
extern int separate_wall_picture;
#endif

/* --- static varaibles --- */

static ui_picture_t **pics;
static u_int num_pics;
static ui_icon_picture_t **icon_pics;
static u_int num_icon_pics;
static ui_inline_picture_t *inline_pics;
static u_int num_inline_pics;
static u_int num_anims;
static int need_cleanup;

/* --- static functions --- */

static ui_picture_t *create_picture_intern(Display *display, ui_picture_modifier_t *mod,
                                           char *file_path, u_int width, u_int height) {
  ui_picture_t *pic;

  if ((pic = malloc(sizeof(ui_picture_t))) == NULL) {
    return NULL;
  }

  if (mod) {
    if ((pic->mod = malloc(sizeof(ui_picture_modifier_t))) == NULL) {
      goto error1;
    }

    *pic->mod = *mod;
  } else {
    pic->mod = NULL;
  }

  if ((pic->file_path = strdup(file_path)) == NULL) {
    goto error2;
  }

  pic->display = display;
  pic->width = width;
  pic->height = height;

  return pic;

error2:
  free(pic->mod);
error1:
  free(pic);

  return NULL;
}

static int destroy_picture_intern(ui_picture_t *pic) {
  free(pic->file_path);
  free(pic->mod);
  free(pic);

  return 1;
}

static ui_picture_t *create_bg_picture(ui_window_t *win, ui_picture_modifier_t *mod,
                                       char *file_path) {
  ui_picture_t *pic;

#ifdef USE_GRF
  ui_window_t new_win;

  if (separate_wall_picture) {
    memset(&new_win, 0, sizeof(new_win));
    new_win.disp = win->disp;
    new_win.width = win->disp->display->width;
    new_win.height = win->disp->display->height;

    win = &new_win;
  }
#endif

  if (!(pic = create_picture_intern(win->disp->display, mod, file_path, ACTUAL_WIDTH(win),
                                    ACTUAL_HEIGHT(win)))) {
    return NULL;
  }

  if (strcmp(file_path, "root") == 0) {
    pic->pixmap = ui_imagelib_get_transparent_background(win, mod);
  } else {
    pic->pixmap = ui_imagelib_load_file_for_background(win, file_path, mod);
  }

  if (pic->pixmap == None) {
    destroy_picture_intern(pic);

    return NULL;
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " New pixmap %ul is created.\n", pic->pixmap);
#endif

  pic->ref_count = 1;

  return pic;
}

static int destroy_picture(ui_picture_t *pic) {
  /* XXX Pixmap of "pixmap:<ID>" is managed by others, so don't free here. */
  if (strncmp(pic->file_path, "pixmap:", 7) != 0) {
    ui_destroy_image(pic->display, pic->pixmap);
  }

  destroy_picture_intern(pic);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " pixmap is destroyed.\n");
#endif

  return 1;
}

static ui_icon_picture_t *create_icon_picture(ui_display_t *disp,
                                              char *file_path /* Don't specify NULL. */
                                              ) {
  u_int icon_size = 48;
  ui_icon_picture_t *pic;

  if ((pic = malloc(sizeof(ui_icon_picture_t))) == NULL) {
    return NULL;
  }

  if ((pic->file_path = strdup(file_path)) == NULL ||
      !ui_imagelib_load_file(disp, file_path, 0, &(pic->cardinal), &(pic->pixmap), &(pic->mask),
                             &icon_size, &icon_size, NULL)) {
    free(pic->file_path);
    free(pic);

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Failed to load icon file(%s).\n", file_path);
#endif

    return NULL;
  }

  pic->disp = disp;
  pic->ref_count = 1;

#if 0
  bl_debug_printf(BL_DEBUG_TAG " Successfully loaded icon file %s.\n", file_path);
#endif

  return pic;
}

static int destroy_icon_picture(ui_icon_picture_t *pic) {
#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s icon will be destroyed.\n", pic->file_path);
#endif

  ui_destroy_image(pic->disp->display, pic->pixmap);
  ui_destroy_mask(pic->disp->display, pic->mask);

  free(pic->cardinal);
  free(pic->file_path);

  free(pic);

  return 1;
}

static int hash_path(char *path) {
  int hash;

  hash = 0;
  while (*path) {
    hash += *(path++);
  }

  return hash & 65535 /* 0xffff */;
}

static inline size_t get_anim_file_path_len(char *dir) {
  return strlen(dir) + 10 + 5 + DIGIT_STR_LEN(int)+1;
}

static int anim_file_exists(char *file_path, char *dir, int hash, int count) {
  struct stat st;

  if (count > 0) {
    sprintf(file_path, "%sanim%d-%d.gif", dir, hash, count);
    if (stat(file_path, &st) == 0) {
      return 1;
    }

    sprintf(file_path, "%sanimx%d-%d.gif", dir, hash, count);
  } else {
    sprintf(file_path, "%sanim%d.gif", dir, hash);
  }

  return stat(file_path, &st) == 0;
}

/*
 * XXX
 * This function should be called synchronously because both load_file_async()
 * and cleanup_inline_pictures() can call this asynchronously.
 */
static int destroy_inline_picture(ui_inline_picture_t *pic /* pic->pixmap mustn't be NULL. */
                                 ) {
  if (pic->pixmap == DUMMY_PIXMAP) {
    if (strstr(pic->file_path, "mlterm/anim")) {
      /* GIF Animation frame */
      unlink(pic->file_path);
    } else if (pic->disp) {
      /* loading async */
      return 0;
    }
  }

  /* pic->disp can be NULL by ui_picture_display_closed() and load_file(). */
  if (pic->disp) {
    if (pic->pixmap != DUMMY_PIXMAP) {
      ui_destroy_image(pic->disp->display, pic->pixmap);
      ui_destroy_mask(pic->disp->display, pic->mask);
    }

    pic->disp = NULL;
  }

  if (pic->file_path) {
    if (strcasecmp(pic->file_path + strlen(pic->file_path) - 4, ".gif") == 0 &&
        /* If check_anim was processed, next_frame == -2. */
        pic->next_frame == -1) {
      char *dir;
      char *file_path;

      if ((dir = bl_get_user_rc_path("mlterm/")) &&
          (file_path = alloca(get_anim_file_path_len(dir)))) {
        int hash;
        int count;

        hash = hash_path(pic->file_path);

        for (count = 0;; count++) {
          if (!anim_file_exists(file_path, dir, hash, count)) {
            break;
          }

          unlink(file_path);
        }
      }

      free(dir);
    }

    free(pic->file_path);
    pic->file_path = NULL;
  }

  /* pixmap == None means that the inline picture is empty. */
  pic->pixmap = None;

  /*
   * Don't next_frame = -1 here because ui_animate_inline_pictures() refers it
   * even if load_file() fails.
   */
  if (pic->next_frame >= 0) {
    num_anims--;
  }

  return 1;
}

static void pty_closed(vt_term_t *term) {
  u_int count;

  for (count = 0; count < num_inline_pics; count++) {
    if (inline_pics[count].term == term && inline_pics[count].pixmap) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " destroy inline picture %d (%d)\n", count, num_inline_pics);
#endif

      destroy_inline_picture(inline_pics + count);
    }
  }

  /* XXX The memory of intern_pics is not freed. */
}

static void check_inline_pictures(vt_term_t *term, u_int8_t *flags, int beg, int end) {
  int row;
  vt_line_t *line;
  u_int count;

  for (row = beg; row <= end; row++) {
    if ((line = vt_term_get_line(term, row))) {
      for (count = 0; count < line->num_filled_chars; count++) {
        vt_char_t *ch;

        if ((ch = vt_get_picture_char(line->chars + count))) {
          int idx;

          idx = vt_char_picture_id(ch);
          do {
            flags[idx] = 1;
            idx = inline_pics[idx].next_frame;
          } while (idx >= 0 && flags[idx] == 0);
        }
      }
    }
  }
}

static void check_inline_pictures_drcs(vt_term_t *term, u_int8_t *flags) {
  vt_drcs_t *drcs;

  if ((drcs = term->parser->drcs)) {
    size_t count;

    for (count = 0; count < BL_ARRAY_SIZE(drcs->fonts); count++) {
      if (drcs->fonts[count] && vt_drcs_has_picture(drcs->fonts[count])) {
        flags[drcs->fonts[count]->pic_id] = 1;

        /* animation check is not necessary. */
      }
    }
  }
}

static int cleanup_inline_pictures(vt_term_t *term) {
  static u_int threshold = 48; /* 48 is initial value and changed to num_inline_pics + 24. */
  static int num_empty_slots; /* if num_empty_slots < 5, need_cleanup is on. */
  int count;
  int empty_idx;
  u_int8_t *flags;

  /*
   * Don't cleanup unused inline pictures until the number of cached inline
   * pictures is equal to or greater than threshold and need_cleanup is true.
   */

  if (num_inline_pics < threshold || !(flags = alloca(num_inline_pics))) {
    if (num_inline_pics == 0) {
      /* XXX */
      vt_term_pty_closed_event = pty_closed;
    }

    return -1;
  }

  if (!need_cleanup) {
    memset(flags, 1, num_inline_pics);
  } else {
    int beg;
    int end;
    vt_edit_t *orig_edit;

    memset(flags, 0, num_inline_pics);

    /*
     * Inline pictures in back logs except recent MAX_INLINE_PICTURES*2 lines
     * are destroyed in line_scrolled_out() in ui_screen.c.
     */
    if ((beg = -vt_term_get_num_logged_lines(term)) < INLINEPIC_AVAIL_ROW) {
      beg = INLINEPIC_AVAIL_ROW;
    }
    end = vt_term_get_rows(term) - 1;
    orig_edit = term->screen->edit;

    check_inline_pictures(term, flags, beg, end);

    if (term->screen->edit == &term->screen->alt_edit) {
      term->screen->edit = &term->screen->normal_edit;
      check_inline_pictures(term, flags, 0, end);
    }

    if (term->screen->page_edits) {
      int count = 0;

      /*
       * Page 0 = normal_edit or alt_edit
       * Page 1-MAX_PAGE_ID = pages
       */
      for (count = 0; count < MAX_PAGE_ID ; count++) {
        if ((term->screen->edit = term->screen->page_edits + count) != orig_edit) {
          check_inline_pictures(term, flags, 0, end);
        }
      }
    }

    term->screen->edit = orig_edit;

    check_inline_pictures_drcs(term, flags);
  }

  empty_idx = -1;

  for (count = num_inline_pics - 1; count >= 0; count--) {
    if (inline_pics[count].pixmap == None) {
      /* empty slot */
    } else if (!flags[count] && inline_pics[count].term == term) {
      if (inline_pics[count].pixmap == DUMMY_PIXMAP) {
        continue;
      } else {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " destroy inline picture %s %d (%d) \n",
                        inline_pics[count].file_path, count, num_inline_pics);
#endif

        if (!destroy_inline_picture(inline_pics + count)) {
          continue;
        }

        num_empty_slots++;

        if (count == num_inline_pics - 1) {
          num_inline_pics--;

          if (threshold > num_inline_pics + 24) {
            threshold = num_inline_pics + 24;
          }

          /* Don't return count because it is out of num_inline_pics. */
          continue;
        }
      }
    } else {
      continue;
    }

    if (empty_idx == -1) {
      num_empty_slots--;

      if (!need_cleanup) {
        if (num_empty_slots < 5) {
          need_cleanup = 1;
        }

        return count;
      } else {
        empty_idx = count;

        /* Continue cleaning up. */
      }
    }
  }

  if (empty_idx == -1) {
    num_empty_slots = 0;
    if (need_cleanup) {
      /*
       * There is no empty entry even after cleanup.
       *
       * XXX
       * Other vt_term_t is not checked above, so it doesn't means that there is
       * not any empty entry in all vt_term_t.
       * But need_cleanup is set to 0 here to make code simple.
       */
      need_cleanup = 0;
      if ((threshold = num_inline_pics + 24) > MAX_INLINE_PICTURES) {
        threshold = MAX_INLINE_PICTURES;
      }
    } else {
      /* Try cleanup at the next time. */
      need_cleanup = 1;
    }
  } else {
    need_cleanup = 0;
  }

  return empty_idx;
}

static int load_file(void *p) {
  int idx;
  Pixmap pixmap;
  PixmapMask mask;
  u_int width;
  u_int height;
  int transparent;

  idx = ((inline_pic_args_t*)p)->idx;
  width = inline_pics[idx].width;
  height = inline_pics[idx].height;

  if (ui_imagelib_load_file(inline_pics[idx].disp, inline_pics[idx].file_path,
                            ((inline_pic_args_t*)p)->keep_aspect, NULL,
                            &pixmap, &mask, &width, &height, &transparent)) {
    if (strstr(inline_pics[idx].file_path, "mlterm/anim")) {
      /* GIF Animation frame */
      unlink(inline_pics[idx].file_path);
    }

    /* XXX pthread_mutex_lock( &mutex) is necessary. */
    inline_pics[idx].mask = mask;
    inline_pics[idx].width = width;
    inline_pics[idx].height = height;
    inline_pics[idx].pixmap = pixmap;
    inline_pics[idx].transparent = transparent;

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " new inline picture (%s %d %d %d %d %p %p) is created.\n",
                    inline_pics[idx].file_path, idx, width, height, transparent,
                    pixmap, mask);
#endif

    return 1;
  } else {
    inline_pics[idx].disp = NULL;
    destroy_inline_picture(inline_pics + idx);

    return 0;
  }
}

#if defined(USE_WIN32API) || defined(HAVE_PTHREAD)

#ifdef USE_WIN32API
static u_int __stdcall
#else
static void*
#endif
    load_file_async(void *p) {
#ifdef HAVE_PTHREAD
  pthread_detach(pthread_self());
#endif

  load_file(p);

#ifdef HAVE_WINDOWS_H
  if (((inline_pic_args_t*)p)->ev) {
    SetEvent(((inline_pic_args_t*)p)->ev);
    CloseHandle(((inline_pic_args_t*)p)->ev);
  }
#else
  if (((inline_pic_args_t*)p)->ev != -1) {
    close(((inline_pic_args_t*)p)->ev);
  }
#endif

  free(p);

  return 0;
}

#endif

static int ensure_inline_picture(ui_display_t *disp, const char *file_path,
                                 u_int *width,  /* can be 0 */
                                 u_int *height, /* can be 0 */
                                 u_int col_width, u_int line_height, vt_term_t *term) {
  int idx;

  if ((idx = cleanup_inline_pictures(term)) == -1) {
    void *p;

    /* XXX pthread_mutex_lock( &mutex) is necessary. */
    if (num_inline_pics >= MAX_INLINE_PICTURES) {
      int row;
      int rmpic = 0;

      for (row = INLINEPIC_AVAIL_ROW; row < 0; row++) {
        vt_line_t *line;

        if ((line = vt_term_get_line(term, row))) {
          rmpic = vt_line_clear_picture(line);
        }
      }

      if (!rmpic) {
        return -1;
      }

      /* retry cleanup */
      need_cleanup = 1;
      if ((idx = cleanup_inline_pictures(term)) == -1) {
        return -1;
      }
    } else if ((p = realloc(inline_pics, (num_inline_pics + 1) * sizeof(*inline_pics)))) {
      inline_pics = p;
      idx = num_inline_pics++;
    } else {
      return -1;
    }
  }

  inline_pics[idx].pixmap = None; /* mark as empty */
  inline_pics[idx].file_path = strdup(file_path);
  inline_pics[idx].width = *width;
  inline_pics[idx].height = *height;
  inline_pics[idx].disp = disp;
  inline_pics[idx].term = term;
  inline_pics[idx].col_width = col_width;
  inline_pics[idx].line_height = line_height;
  inline_pics[idx].next_frame = -1;

  return idx;
}

static int next_frame_pos(ui_inline_picture_t *prev, ui_inline_picture_t *next, int pos) {
  u_int cur_rows;
  u_int next_rows;
  int row;
  int col;

  cur_rows = (prev->height + prev->line_height - 1) / prev->line_height;
  next_rows = (next->height + next->line_height - 1) / next->line_height;

  row = pos % cur_rows;
  col = pos / cur_rows;

  if (row < next_rows && col < (next->width + next->col_width - 1) / next->col_width) {
    return MAKE_INLINEPIC_POS(col, row, next_rows);
  } else {
    return -1;
  }
}

/* --- global functions --- */

void ui_picture_display_opened(Display *display) {
  ui_imagelib_display_opened(display);
}

void ui_picture_display_closed(Display *display) {
  int count;

  if (num_icon_pics > 0) {
    for (count = num_icon_pics - 1; count >= 0; count--) {
      if (icon_pics[count]->disp->display == display) {
        destroy_icon_picture(icon_pics[count]);
        icon_pics[count] = icon_pics[--num_icon_pics];
      }
    }

    if (num_icon_pics == 0) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " All cached icons were free'ed\n");
#endif

      free(icon_pics);
      icon_pics = NULL;
    }
  }

  for (count = 0; count < num_inline_pics; count++) {
    if (inline_pics[count].disp && inline_pics[count].disp->display == display) {
      if (PIXMAP_IS_ACTIVE(inline_pics[count])) {
        ui_destroy_image(display, inline_pics[count].pixmap);
        ui_destroy_mask(display, inline_pics[count].mask);
      }

      /*
       * Don't set ui_inline_picture_t::pixmap = None here because
       * this inline picture can still exist in vt_term_t.
       *
       * disp = NULL is for platforms where
       * INLINE_PICTURE_MOVABLE_BETWEEN_DISPLAYS is not defined.
       * (see draw_picture() in ui_draw_str.c)
       */
      inline_pics[count].disp = NULL;
    }
  }

  ui_imagelib_display_closed(display);
}

/*
 * Judge whether pic_mods are equal or not.
 * \param a,b picture modifier
 * \return  1 when they are same. 0 when not.
 */
int ui_picture_modifiers_equal(
    ui_picture_modifier_t *a, /* Can be NULL (which means normal pic_mod) */
    ui_picture_modifier_t *b  /* Can be NULL (which means normal pic_mod) */
    ) {
  if (a == b) {
    /* If a==NULL and b==NULL, return 1 */
    return 1;
  }

  if (a == NULL) {
    a = b;
    b = NULL;
  }

  if (b == NULL) {
    /* Check if 'a' is normal or not. */

    if ((a->brightness == 100) && (a->contrast == 100) && (a->gamma == 100) && (a->alpha == 0)) {
      return 1;
    }
  } else {
    if ((a->brightness == b->brightness) && (a->contrast == b->contrast) &&
        (a->gamma == b->gamma) && (a->alpha == b->alpha) && (a->blend_red == b->blend_red) &&
        (a->blend_green == b->blend_green) && (a->blend_blue == b->blend_blue)) {
      return 1;
    }
  }

  return 0;
}

ui_picture_t *ui_acquire_bg_picture(ui_window_t *win, ui_picture_modifier_t *mod,
                                    char *file_path /* "root" means transparency. */
                                    ) {
  ui_picture_t **p;

  if (strcmp(file_path, "root") != 0) /* Transparent background is not cached. */
  {
    u_int count;

    for (count = 0; count < num_pics; count++) {
      if (strcmp(file_path, pics[count]->file_path) == 0 &&
          win->disp->display == pics[count]->display &&
          ui_picture_modifiers_equal(mod, pics[count]->mod) &&
          ACTUAL_WIDTH(win) == pics[count]->width && ACTUAL_HEIGHT(win) == pics[count]->height) {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Use cached picture(%s).\n", file_path);
#endif
        pics[count]->ref_count++;

        return pics[count];
      }
    }
  }

  if ((p = realloc(pics, (num_pics + 1) * sizeof(*pics))) == NULL) {
    return NULL;
  }

  pics = p;

  if (!(pics[num_pics] = create_bg_picture(win, mod, file_path))) {
    if (num_pics == 0 /* pics == NULL */) {
      free(pics);
      pics = NULL;
    }

    return NULL;
  }

  return pics[num_pics++];
}

void ui_release_picture(ui_picture_t *pic) {
  u_int count;

  for (count = 0; count < num_pics; count++) {
    if (pic == pics[count]) {
      if (--(pic->ref_count) == 0) {
        destroy_picture(pic);

        if (--num_pics == 0) {
#ifdef DEBUG
          bl_debug_printf(BL_DEBUG_TAG " All cached bg pictures were free'ed\n");
#endif

          free(pics);
          pics = NULL;
        } else {
          pics[count] = pics[num_pics];
        }
      }

      return;
    }
  }
}

ui_icon_picture_t *ui_acquire_icon_picture(ui_display_t *disp,
                                           char *file_path /* Don't specify NULL. */
                                           ) {
  u_int count;
  ui_icon_picture_t **p;

  for (count = 0; count < num_icon_pics; count++) {
    if (strcmp(file_path, icon_pics[count]->file_path) == 0 && disp == icon_pics[count]->disp) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Use cached icon(%s).\n", file_path);
#endif
      icon_pics[count]->ref_count++;

      return icon_pics[count];
    }
  }

  if ((p = realloc(icon_pics, (num_icon_pics + 1) * sizeof(*icon_pics))) == NULL) {
    return NULL;
  }

  icon_pics = p;

  if ((icon_pics[num_icon_pics] = create_icon_picture(disp, file_path)) == NULL) {
    if (num_icon_pics == 0 /* icon_pics == NULL */) {
      free(icon_pics);
      icon_pics = NULL;
    }

    return NULL;
  }

  return icon_pics[num_icon_pics++];
}

void ui_release_icon_picture(ui_icon_picture_t *pic) {
  u_int count;

  for (count = 0; count < num_icon_pics; count++) {
    if (pic == icon_pics[count]) {
      if (--(pic->ref_count) == 0) {
        destroy_icon_picture(pic);

        if (--num_icon_pics == 0) {
#ifdef DEBUG
          bl_debug_printf(BL_DEBUG_TAG " All cached icons were free'ed\n");
#endif

          free(icon_pics);
          icon_pics = NULL;
        } else {
          icon_pics[count] = icon_pics[num_icon_pics];
        }
      }

      return;
    }
  }
}

/*
 * The caller must check if file_path exists or not to avoid to call mlimgloader unnecessarily.
 * 'file_path' can be /foo/bar.ttf/1f400 for emoji.
 */
int ui_load_inline_picture(ui_display_t *disp, char *file_path, u_int *width /* can be 0 */,
                           u_int *height /* can be 0 */, u_int col_width, u_int line_height,
                           int keep_aspect, vt_term_t *term) {
  int idx;
  inline_pic_args_t *args;

  /* XXX Don't reuse ~/.mlterm/[pty name].six, [pty name].rgs and anim-*.gif */
  if (!strstr(file_path, "mlterm/") || strstr(file_path, "mlterm/macro") ||
      strstr(file_path, "mlterm/emoji/")) {
    for (idx = 0; idx < num_inline_pics; idx++) {
      if (PIXMAP_IS_ACTIVE(inline_pics[idx]) && disp == inline_pics[idx].disp &&
          strcmp(file_path, inline_pics[idx].file_path) == 0 && term == inline_pics[idx].term &&
          /* XXX */ (*width == 0 || *width == inline_pics[idx].width) &&
          /* XXX */ (*height == 0 || *height == inline_pics[idx].height)) {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Use cached picture(%s).\n", file_path);
#endif

        if (strcasecmp(file_path + strlen(file_path) - 4, ".gif") == 0 &&
            /* If check_anim was processed, next_frame == -2. */
            inline_pics[idx].next_frame == -1) {
          goto check_anim;
        } else {
          goto end;
        }
      }
    }
  }

  if ((idx = ensure_inline_picture(disp, file_path, width, height, col_width, line_height, term)) ==
          -1 ||
      !(args = malloc(sizeof(inline_pic_args_t)))) {
    return -1;
  }

  args->idx = idx;
  args->keep_aspect = keep_aspect;

#if defined(HAVE_PTHREAD) || defined(USE_WIN32API)
  if (strstr(file_path, "://")) {
/* Loading a remote file asynchronously. */

#ifdef HAVE_WINDOWS_H
    args->ev = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
    int fds[2];

    if (pipe(fds) != 0) {
      fds[1] = args->ev = -1;
    } else {
      args->ev = fds[0];
    }
#endif

    inline_pics[idx].pixmap = DUMMY_PIXMAP;

#ifdef USE_WIN32API
    {
      HANDLE thrd;
      u_int tid;

      if ((thrd = _beginthreadex(NULL, 0, load_file_async, args, 0, &tid))) {
        CloseHandle(thrd);
      }
    }
#else
    {
      pthread_t thrd;

      pthread_create(&thrd, NULL, load_file_async, args);
    }
#endif

#ifdef HAVE_WINDOWS_H
    if (WaitForSingleObject(args->ev, 750) != WAIT_TIMEOUT && PIXMAP_IS_ACTIVE(inline_pics[idx])) {
      goto check_anim;
    }
#else
    if (fds[1] != -1) {
      fd_set read_fds;
      struct timeval tval;
      int ret;

      tval.tv_usec = 750000;
      tval.tv_sec = 0;
      FD_ZERO(&read_fds);
      FD_SET(fds[1], &read_fds);

      ret = select(fds[1] + 1, &read_fds, NULL, NULL, &tval);
      close(fds[1]);

      if (ret != 0 && PIXMAP_IS_ACTIVE(inline_pics[idx])) {
        goto check_anim;
      }
    }
#endif
  } else
#endif
  {
    int ret = load_file(args);

    free(args);

    if (ret) {
      goto check_anim;
    }
  }

  return -1;

check_anim:
  if (strcasecmp(file_path + strlen(file_path) - 4, ".gif") == 0) {
    /* Animation GIF */

    char *dir;

    /* mark checked */
    inline_pics[idx].next_frame = -2;

    if ((dir = bl_get_user_rc_path("mlterm/")) &&
        (file_path = alloca(get_anim_file_path_len(dir)))) {
      int hash;
      int count;
      int i;
      int prev_i;

      hash = hash_path(inline_pics[idx].file_path);

      if (anim_file_exists(file_path, dir, hash, 0)) {
        /* The first frame has been already loaded. */
        unlink(file_path);
      }

      prev_i = idx;
      for (count = 1;; count++) {
        if (!anim_file_exists(file_path, dir, hash, count)) {
          break;
        }

        /*
         * Don't clean up because the 1st frame has not been set
         * to vt_term_t yet here.
         */
        need_cleanup = 0;

        if ((i = ensure_inline_picture(disp, file_path, width, height, col_width, line_height,
                                       term)) >= 0 &&
            ui_add_frame_to_animation(prev_i, i)) {
          inline_pics[i].pixmap = DUMMY_PIXMAP;
          prev_i = i;
        }
      }
    }

    free(dir);
  }

end:
  *width = inline_pics[idx].width;
  *height = inline_pics[idx].height;

  return idx;
}

ui_inline_picture_t *ui_get_inline_picture(int idx) {
  if (idx < num_inline_pics) {
    /*
     * XXX
     * This check is not really necessary because cleanup_inline_pictures()
     * doesn't destroy inline pictures remaining in vt_term.
     * But it is enabled for now because of this bug.
     * http://twitter.com/isaki68k/status/1259110003874488326
     */
#if 1
    if (inline_pics[idx].pixmap == None) {
      bl_msg_printf("A bug that should never happen.\n");

      return NULL;
    }
#endif

    return inline_pics + idx;
  } else {
    return NULL;
  }
}

int ui_add_frame_to_animation(int prev_idx, int next_idx) {
  ui_inline_picture_t *prev_pic;
  ui_inline_picture_t *next_pic;

  if ((prev_pic = get_inline_picture(prev_idx)) &&
      (next_pic = get_inline_picture(next_idx)) &&
      /* Animation is stopped after adding next_idx which equals to
         prev_pic->next_frame */
      prev_pic->next_frame != next_idx &&
      /* Don't add a picture which has been already added to an animation. */
      next_pic->next_frame < 0) {
    if (prev_pic->next_frame < 0) {
      num_anims += 2;
      prev_pic->next_frame = next_idx;
      next_pic->next_frame = prev_idx;
    } else {
      num_anims++;
      next_pic->next_frame = prev_pic->next_frame;
      prev_pic->next_frame = next_idx;
    }

    return 1;
  } else {
    return 0;
  }
}

int ui_animate_inline_pictures(vt_term_t *term) {
  int wait;
  int row;
  vt_line_t *line;
  u_int num_rows;

  if (!num_anims) {
    return 0;
  }

  wait = 0;

  num_rows = vt_term_get_rows(term);
  for (row = 0; row < num_rows; row++) {
    int char_index;

    line = vt_term_get_line_in_screen(term, row); /* Always non-NULL */

    for (char_index = 0; char_index < line->num_filled_chars; char_index++) {
      vt_char_t *ch;

      if ((ch = vt_get_picture_char(line->chars + char_index))) {
        int32_t pos;
        int idx;
        int next;

        pos = vt_char_code(ch);
        idx = vt_char_picture_id(ch);
        if ((next = inline_pics[idx].next_frame) < 0) {
          continue;
        }

      retry:
        if (inline_pics[next].pixmap == DUMMY_PIXMAP) {
          inline_pic_args_t args;

          args.idx = next;
          if (!load_file(&args)) {
            if (inline_pics[next].next_frame == idx) {
              inline_pics[idx].next_frame = -1;
              continue;
            }

            next = inline_pics[idx].next_frame = inline_pics[next].next_frame;
            goto retry;
          }

          /* shorten waiting time. */
          wait = 2;
        }

        if ((pos = next_frame_pos(inline_pics + idx, inline_pics + next, pos)) >= 0) {
          vt_char_set_code(ch, pos);
          vt_char_set_picture_id(ch, next);
          vt_line_set_modified(line, char_index, char_index);

          if (wait == 0) {
            wait = 1;
          }
        }
      }
    }
  }

  return wait;
}

int ui_load_tmp_picture(ui_display_t *disp, char *file_path, Pixmap *pixmap, PixmapMask *mask,
                        u_int *width, u_int *height) {
  *width = *height = 0;

  if (ui_imagelib_load_file(disp, file_path, 0, NULL, pixmap, mask, width, height, NULL)) {
    return 1;
  } else {
    return 0;
  }
}

void ui_destroy_tmp_picture(ui_display_t *disp, Pixmap pixmap, PixmapMask mask) {
  ui_destroy_image(disp->display, pixmap);
  ui_destroy_mask(disp->display, mask);
}

#endif /* NO_IMAGE */
