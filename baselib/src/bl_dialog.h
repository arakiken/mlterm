/*
 *	$Id$
 */

#ifndef __BL_DIALOG_H__

typedef enum {
  BL_DIALOG_OKCANCEL = 0,
  BL_DIALOG_ALERT = 1,

} bl_dialog_style_t;

int bl_dialog_set_callback(int (*callback)(bl_dialog_style_t, char *));

int bl_dialog(bl_dialog_style_t style, char *msg);

#endif
