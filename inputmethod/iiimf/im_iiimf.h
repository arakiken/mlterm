/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __IM_IIIMF_H__
#define __IM_IIIMF_H__

#include <pobl/bl_types.h> /* HAVE_STDINT_H */
#include <X11/Xlib.h>
#include <iiimcf.h>

#include "ui_im.h"

typedef struct im_iiimf {
  /* input method common object */
  ui_im_t im;

  IIIMCF_context context;

  ef_parser_t *parser_term;
  ef_conv_t *conv;

  struct aux *aux;

  int on;

} im_iiimf_t;

int im_iiimf_process_event(im_iiimf_t *iiimf);

/*
 * taken from Minami-san's hack in x_dnd.c
 *
 * Note: The byte order is the same as client.
 * (see lib/iiimp/data/im-connect.c:iiimp_connect_new())
 */
#define PARSER_INIT_WITH_BOM(parser)        \
do {                \
  u_int16_t BOM[] = {0xfeff};       \
  (*(parser)->init)((parser));        \
  (*(parser)->set_str)((parser), (u_char*)BOM, 2);  \
  (*(parser)->next_char)((parser), NULL);   \
} while (0)

static size_t strlen_utf16(const IIIMP_card16 *str) {
  size_t len = 0;

  if (! str) {
    return 0;
  }

  while (*str) {
    len += 2;
    str ++;
    if(len == 0xffff) /* prevent infinity loop */ {
      return 0;
    }
  }

  return len;
}

/*
 * aux related definitions based on iiimpAux.h and iiimpAuxP.h of im-sdk-r2195
 */

/*
 * Copyright 1990-2001 Sun Microsystems, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions: The above copyright notice and this
 * permission notice shall be included in all copies or substantial
 * portions of the Software.
 *
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE OPEN GROUP OR SUN MICROSYSTEMS, INC. BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE EVEN IF
 * ADVISED IN ADVANCE OF THE POSSIBILITY OF SUCH DAMAGES.
 *
 *
 * Except as contained in this notice, the names of The Open Group and/or
 * Sun Microsystems, Inc. shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without prior
 * written authorization from The Open Group and/or Sun Microsystems,
 * Inc., as applicable.
 *
 *
 * X Window System is a trademark of The Open Group
 *
 * OSF/1, OSF/Motif and Motif are registered trademarks, and OSF, the OSF
 * logo, LBX, X Window System, and Xinerama are trademarks of the Open
 * Group. All other trademarks and registered trademarks mentioned herein
 * are the property of their respective owners. No right, title or
 * interest in or to any trademark, service mark, logo or trade name of
 * Sun Microsystems, Inc. or its licensors is granted.
 *
 */

typedef struct aux {
  im_iiimf_t *iiimf;
  struct aux_service *service;
  struct aux_im_data *im_data;
  struct aux_im_data *im_data_list;

} aux_t;

typedef struct {
  int len;
  IIIMP_card16 *utf16str;

} aux_name_t;

typedef struct aux_string {
  int len;
  u_char *ptr;

} aux_string_t;

typedef struct aux_im_data {
  int im_id;
  int ic_id;
  struct aux_entry *entry;
  void *data;
  struct aux_im_data *next;

} aux_im_data_t;

typedef struct aux_dir {
  aux_name_t name;
  struct aux_method *method;

} aux_dir_t;

typedef struct aux_entry {
  int created;
  aux_dir_t dir;
  unsigned int if_version;

} aux_entry_t;

typedef enum aux_data_type {
  AUX_DATA_NONE = 0,
  AUX_DATA_START = 1,
  AUX_DATA_DRAW = 2,
  AUX_DATA_DONE = 3,
  AUX_DATA_SETVALUE = 4,
  AUX_DATA_GETVALUE = 5,
  AUX_DATA_GETVALUE_REPLY = 6,

} aux_data_type_t;

typedef struct aux_data {
  aux_data_type_t type;
  int im;
  int ic;
  int aux_index;
  int aux_name_length;
  unsigned char *aux_name;
  int integer_count;
  int *integer_list;
  int string_count;
  aux_string_t *string_list;
  unsigned char *string_ptr;

} aux_data_t;

typedef struct aux_method {
  Bool (*create)(aux_t *);
  Bool (*start)(aux_t *, const u_char *, int);
  Bool (*draw)(aux_t *, const u_char *, int);
  Bool (*done)(aux_t *, const u_char *, int);
  Bool (*switched)(aux_t *, int, int);
  Bool (*destroy)(aux_t *);

  /* AUX_IF_VERSION_2 */
  Bool (*getvalues_reply)(aux_t *, const u_char *, int);
  Bool (*destroy_ic)(aux_t *);
  Bool (*set_icfocus)(aux_t *);
  Bool (*unset_icfocus)(aux_t *);

} aux_method_t;

typedef struct aux_service {
  void (*aux_setvalue) (aux_t *, const u_char *, int) ;
  int (*im_id) (aux_t *) ;
  int (*ic_id) (aux_t *) ;
  void (*data_set) (aux_t *, int, void *) ;
  void *(*data_get) (aux_t *, int) ;
  Display *(*display) (aux_t *) ;
  Window (*window)(aux_t *) ;
  XPoint *(*point)(aux_t *, XPoint *);
  XPoint *(*point_caret)(aux_t *, XPoint *);
  size_t (*utf16_mb)(const char **, size_t *, char **, size_t *);
  size_t (*mb_utf16)(const char **, size_t *, char **, size_t *);
  u_char *(*compose)(const aux_data_t *, int *);
  int (*compose_size)(aux_data_type_t, const u_char *);
  aux_data_t *(*decompose)(aux_data_type_t, const u_char *);
  void (*decompose_free)(aux_data_t *);
  void (*register_X_filter)(Display *, Window, int, int,
                            Bool (*filter)(Display *, Window, XEvent *, XPointer), XPointer);
  void (*unregister_X_filter)(Display *, Window,
                              Bool (* filter)(Display *, Window, XEvent *, XPointer), XPointer);
  Bool (*server)(aux_t *);
  Window (*client_window)(aux_t *);
  Window (*focus_window)(aux_t *);
  int (*screen_number)(aux_t *);
  int (*point_screen)(aux_t *, XPoint *);
  int (*point_caret_screen)(aux_t *, XPoint *);
  Bool (*get_conversion_mode)(aux_t *);
  void (*set_conversion_mode)(aux_t *, int);

  /* AUX_IF_VERSION_2 */
  void (*aux_getvalue)(aux_t *, const unsigned char *, int);
  aux_t *(*aux_get_from_id)(int, int, IIIMP_card16 *, int);

} aux_service_t;


typedef struct aux_info {
  unsigned int if_version;
  Bool (*register_service)(u_int, aux_service_t *);
  aux_dir_t *dir;

} aux_info_t;


void aux_init(IIIMCF_handle handle, ui_im_export_syms_t *syms, ef_parser_t *parser);

void aux_quit(void);

aux_t *aux_new(im_iiimf_t *iiimf);

void aux_delete(aux_t *aux);

void aux_event(aux_t *aux, IIIMCF_event ev, IIIMCF_event_type);

void aux_set_focus(aux_t *aux);

void aux_unset_focus(aux_t *aux);

#endif
