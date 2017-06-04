/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_BRLTTY_H__
#define __UI_BRLTTY_H__

#include <vt_term.h>
#include "ui.h"

int ui_brltty_init(void);

int ui_brltty_final(void);

void ui_brltty_focus(vt_term_t *term);

void ui_brltty_write(void);

void ui_brltty_key(KeySym ksym, const u_char *kstr, size_t len);

#endif
