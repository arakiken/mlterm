/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
 * im_kbd.c - keyboard mapping input method for mlterm
 *
 *    based on x_kbd.c written by Araki Ken.
 *
 * Copyright (C) 2001, 2002, 2003 Araki Ken <arakiken@users.sf.net>
 * Copyright (C) 2004 Seiichi SATO <ssato@sh.rim.or.jp>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 *	$Id$
 */

#include <stdio.h>          /* sprintf */
#include <pobl/bl_mem.h>    /* malloc/alloca/free */
#include <pobl/bl_str.h>    /* bl_snprintf */
#include <pobl/bl_locale.h> /* bl_get_locale */
#include <mef/ef_utf16_parser.h>
#include <vt_iscii.h>
#include <ui_im.h>

#include "../im_info.h"

#if 0
#define IM_KBD_DEBUG 1
#endif

/*
 * If the order of de ... jp106 is changed, change key_event_others().
 * If a new entry is added, add it to kbd_type_tbl.
 */
typedef enum kbd_type {
  KBD_TYPE_UNKNOWN,
  KBD_TYPE_ARABIC,
  KBD_TYPE_HEBREW,
  KBD_TYPE_DE, /* German */
  KBD_TYPE_FR, /* French */
  KBD_TYPE_ES, /* Spanish */
  KBD_TYPE_PT, /* Portuguese */
  KBD_TYPE_JP106, /* Japanese */
  KBD_TYPE_ISCII, /* Must be placed at the end (see get_kbd_type()) */

  KBD_TYPE_MAX

} kbd_type_t;

typedef enum kbd_mode {
  KBD_MODE_ASCII = 0,

  /* arabic or hebrew */
  KBD_MODE_ON,

  /* iscii */
  KBD_MODE_ISCII_INSCRIPT,
  KBD_MODE_ISCII_PHONETIC,

  KBD_MODE_MAX

} kbd_mode_t;

typedef struct im_kbd {
  ui_im_t im;

  kbd_type_t type;
  kbd_mode_t mode;

  vt_isciikey_state_t isciikey_state;

  ef_parser_t *parser;

} im_kbd_t;

/* --- static variables --- */

static int ref_count = 0;
static int initialized = 0;
static ef_parser_t *parser_ascii = NULL;

/* mlterm internal symbols */
static ui_im_export_syms_t *syms = NULL;

/* US -> Arabic */
static u_char *arabic_conv_tbl[] = {
  "\x06\x37",         /* ' */
  NULL,               /* ( */
  NULL,               /* ) */
  NULL,               /* * */
  NULL,               /* + */
  "\x06\x48",         /* , */
  NULL,               /* - */
  "\x06\x32",         /* . */
  "\x06\x38",         /* / */
  "\x06\x60",         /* 0 */
  "\x06\x61",         /* 1 */
  "\x06\x62",         /* 2 */
  "\x06\x63",         /* 3 */
  "\x06\x64",         /* 4 */
  "\x06\x65",         /* 5 */
  "\x06\x66",         /* 6 */
  "\x06\x67",         /* 7 */
  "\x06\x68",         /* 8 */
  "\x06\x69",         /* 9 */
  NULL,               /* : */
  "\x06\x43",         /* ; */
  "\x00\x2c",         /* < */
  NULL,               /* = */
  "\x00\x2e",         /* > */
  "\x06\x1f",         /* ? */
  NULL,               /* @ */
  "\x06\x50",         /* A */
  "\x06\x44\x06\x22", /* B */
  "\x00\x7b",         /* C */
  "\x00\x5b",         /* D */
  "\x06\x4f",         /* E */
  "\x00\x5d",         /* F */
  "\x06\x44\x06\x23", /* G */
  "\x06\x23",         /* H */
  "\x00\xf7",         /* I */
  "\x06\x40",         /* J */
  "\x06\x0c",         /* K */
  "\x00\x2f",         /* L */
  "\x00\x27",         /* M */
  "\x06\x22",         /* N */
  "\x00\xd7",         /* O */
  "\x06\x1b",         /* P */
  "\x06\x4e",         /* Q */
  "\x06\x4c",         /* R */
  "\x06\x4d",         /* S */
  "\x06\x44\x06\x25", /* T */
  "\x00\x60",         /* U */
  "\x00\x7d",         /* V */
  "\x06\x4b",         /* W */
  "\x06\x52",         /* X */
  "\x06\x25",         /* Y */
  "\x00\x7e",         /* Z */
  "\x06\x2c",         /* [ */
  NULL,               /* \ */
  "\x06\x2f",         /* ] */
  NULL,               /* ^ */
  NULL,               /* _ */
  "\x06\x30",         /* ` */
  "\x06\x34",         /* a */
  "\x06\x44\x06\x27", /* b */
  "\x06\x24",         /* c */
  "\x06\x4a",         /* d */
  "\x06\x2b",         /* e */
  "\x06\x28",         /* f */
  "\x06\x44",         /* g */
  "\x06\x27",         /* h */
  "\x06\x47",         /* i */
  "\x06\x2a",         /* j */
  "\x06\x46",         /* k */
  "\x06\x45",         /* l */
  "\x06\x29",         /* m */
  "\x06\x49",         /* n */
  "\x06\x2e",         /* o */
  "\x06\x2d",         /* p */
  "\x06\x36",         /* q */
  "\x06\x42",         /* r */
  "\x06\x33",         /* s */
  "\x06\x41",         /* t */
  "\x06\x39",         /* u */
  "\x06\x31",         /* v */
  "\x06\x35",         /* w */
  "\x06\x21",         /* x */
  "\x06\x3a",         /* y */
  "\x06\x26",         /* z */
  "\x00\x3c",         /* { */
  NULL,               /* | */
  "\x00\x3e",         /* } */
  "\x06\x51",         /* ~ */
};

/* US -> Hebrew */
static u_char *hebrew_conv_tbl[] = {
  "\x00\x3b", /* ' */
  NULL,       /* ( */
  NULL,       /* ) */
  NULL,       /* * */
  NULL,       /* + */
  "\x05\xea", /* , */
  NULL,       /* - */
  "\x05\xe5", /* . */
  "\x00\x2e", /* / */
  NULL,       /* 0 */
  NULL,       /* 1 */
  NULL,       /* 2 */
  NULL,       /* 3 */
  NULL,       /* 4 */
  NULL,       /* 5 */
  NULL,       /* 6 */
  NULL,       /* 7 */
  NULL,       /* 8 */
  NULL,       /* 9 */
  NULL,       /* : */
  "\x05\xe3", /* ; */
  NULL,       /* < */
  NULL,       /* = */
  NULL,       /* > */
  NULL,       /* ? */
  NULL,       /* @ */
  NULL,       /* A */
  NULL,       /* B */
  NULL,       /* C */
  NULL,       /* D */
  NULL,       /* E */
  NULL,       /* F */
  NULL,       /* G */
  NULL,       /* H */
  NULL,       /* I */
  NULL,       /* J */
  NULL,       /* K */
  NULL,       /* L */
  NULL,       /* M */
  NULL,       /* N */
  NULL,       /* O */
  NULL,       /* P */
  NULL,       /* Q */
  NULL,       /* R */
  NULL,       /* S */
  NULL,       /* T */
  NULL,       /* U */
  NULL,       /* V */
  NULL,       /* W */
  NULL,       /* X */
  NULL,       /* Y */
  NULL,       /* Z */
  NULL,       /* [ */
  NULL,       /* \ */
  NULL,       /* ] */
  NULL,       /* ^ */
  NULL,       /* _ */
  "\x00\x3b", /* ` */
  "\x05\xe9", /* a */
  "\x05\xe0", /* b */
  "\x05\xd1", /* c */
  "\x05\xd2", /* d */
  "\x05\xe7", /* e */
  "\x05\xdb", /* f */
  "\x05\xe2", /* g */
  "\x05\xd9", /* h */
  "\x05\xdf", /* i */
  "\x05\xd7", /* j */
  "\x05\xdc", /* k */
  "\x05\xda", /* l */
  "\x05\xe6", /* m */
  "\x05\xde", /* n */
  "\x05\xdd", /* o */
  "\x05\xe4", /* p */
  "\x00\x2f", /* q */
  "\x05\xe8", /* r */
  "\x05\xd3", /* s */
  "\x05\xd0", /* t */
  "\x05\xd5", /* u */
  "\x05\xd4", /* v */
  "\x00\x27", /* w */
  "\x05\xe1", /* x */
  "\x05\xd8", /* y */
  "\x05\xd6", /* z */
  NULL,       /* { */
  NULL,       /* | */
  NULL,       /* } */
  NULL,       /* ~ */
};

/* US -> de (generated by dump4mlterm https://github.com/arakiken/kbd/tree/dump-for-mlterm) */
static u_char de_normal_tbl[] = {
  0x5d, /* ] <- '(0x27) */
  0x28, /* ( */
  0x29, /* ) */
  0x2a, /* * */
  0x2b, /* + */
  0x2c, /* , */
  0x5c, /* \ <- -(0x2d) */
  0x2e, /* . */
  0x2d, /* - <- /(0x2f) */
  0x30, /* 0 */
  0x31, /* 1 */
  0x32, /* 2 */
  0x33, /* 3 */
  0x34, /* 4 */
  0x35, /* 5 */
  0x36, /* 6 */
  0x37, /* 7 */
  0x38, /* 8 */
  0x39, /* 9 */
  0x3a, /* : */
  0x5b, /* [ <- ;(0x3b) */
  0x3c, /* < */
  0x27, /* ' <- =(0x3d) */
  0x3e, /* > */
  0x3f, /* ? */
  0x40, /* @ */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x40, /* @ <- [(0x5b) */
  0x23, /* # <- \(0x5c) */
  0x2b, /* + <- ](0x5d) */
  0x5e, /* ^ */
  0x5f, /* _ */
  0x5e, /* ^ <- `(0x60) */
  0x61, /* a */
  0x62, /* b */
  0x63, /* c */
  0x64, /* d */
  0x65, /* e */
  0x66, /* f */
  0x67, /* g */
  0x68, /* h */
  0x69, /* i */
  0x6a, /* j */
  0x6b, /* k */
  0x6c, /* l */
  0x6d, /* m */
  0x6e, /* n */
  0x6f, /* o */
  0x70, /* p */
  0x71, /* q */
  0x72, /* r */
  0x73, /* s */
  0x74, /* t */
  0x75, /* u */
  0x76, /* v */
  0x77, /* w */
  0x78, /* x */
  0x7a, /* z <- y(0x79) */
  0x79, /* y <- z(0x7a) */
};

static u_char de_shift_tbl[] = {
  0x7d, /* } <- "(0x22) */
  0x23, /* # */
  0x24, /* $ */
  0x25, /* % */
  0x2f, /* / <- &(0x26) */
  0x27, /* ' */
  0x29, /* ) <- ((0x28) */
  0x3d, /* = <- )(0x29) */
  0x28, /* ( <- *(0x2a) */
  0x60, /* ` <- +(0x2b) */
  0x2c, /* , */
  0x2d, /* - */
  0x2e, /* . */
  0x2f, /* / */
  0x30, /* 0 */
  0x31, /* 1 */
  0x32, /* 2 */
  0x33, /* 3 */
  0x34, /* 4 */
  0x35, /* 5 */
  0x36, /* 6 */
  0x37, /* 7 */
  0x38, /* 8 */
  0x39, /* 9 */
  0x7b, /* { <- :(0x3a) */
  0x3b, /* ; */
  0x3b, /* ; <- <(0x3c) */
  0x3d, /* = */
  0x3a, /* : <- >(0x3e) */
  0x5f, /* _ <- ?(0x3f) */
  0x22, /* " <- @(0x40) */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x5b, /* [ */
  0x5c, /* \ */
  0x5d, /* ] */
  0x26, /* & <- ^(0x5e) */
  0x3f, /* ? <- _(0x5f) */
  0x60, /* ` */
  0x61, /* a */
  0x62, /* b */
  0x63, /* c */
  0x64, /* d */
  0x65, /* e */
  0x66, /* f */
  0x67, /* g */
  0x68, /* h */
  0x69, /* i */
  0x6a, /* j */
  0x6b, /* k */
  0x6c, /* l */
  0x6d, /* m */
  0x6e, /* n */
  0x6f, /* o */
  0x70, /* p */
  0x71, /* q */
  0x72, /* r */
  0x73, /* s */
  0x74, /* t */
  0x75, /* u */
  0x76, /* v */
  0x77, /* w */
  0x78, /* x */
  0x79, /* y */
  0x7a, /* z */
  0x5c, /* \ <- {(0x7b) */
  0x27, /* ' <- |(0x7c) */
  0x2a, /* * <- }(0x7d) */
};

static u_char de_alt_tbl[] = {
  0x7d, /* } <- 0(0x30) */
  0x31, /* 1 */
  0x32, /* 2 */
  0x33, /* 3 */
  0x34, /* 4 */
  0x35, /* 5 */
  0x36, /* 6 */
  0x7b, /* { <- 7(0x37) */
  0x5b, /* [ <- 8(0x38) */
  0x5d, /* ] <- 9(0x39) */
  0x3a, /* : */
  0x3b, /* ; */
  0x7c, /* | <- <(0x3c) */
  0x3d, /* = */
  0x3e, /* > */
  0x3f, /* ? */
  0x40, /* @ */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x5b, /* [ */
  0x5c, /* \ */
  0x7e, /* ~ <- ](0x5d) */
  0x5e, /* ^ */
  0x5f, /* _ */
  0x60, /* ` */
  0x61, /* a */
  0x62, /* b */
  0xa2, /* ¢ <- c(0x63) */
  0x64, /* d */
  0xa4, /* ¤ <- e(0x65) */
  0x66, /* f */
  0x67, /* g */
  0x68, /* h */
  0x69, /* i */
  0x6a, /* j */
  0x6b, /* k */
  0x6c, /* l */
  0xb5, /* µ <- m(0x6d) */
  0x6e, /* n */
  0x6f, /* o */
  0x70, /* p */
  0x40, /* @ <- q(0x71) */
};

/* US -> fr (generated by dump4mlterm https://github.com/arakiken/kbd/tree/dump-for-mlterm) */
static u_char fr_normal_tbl[] = {
  0x7c, /* | <- '(0x27) */
  0x28, /* ( */
  0x29, /* ) */
  0x2a, /* * */
  0x2b, /* + */
  0x3b, /* ; <- ,(0x2c) */
  0x29, /* ) <- -(0x2d) */
  0x3a, /* : <- .(0x2e) */
  0x21, /* ! <- /(0x2f) */
  0x40, /* @ <- 0(0x30) */
  0x26, /* & <- 1(0x31) */
  0x7b, /* { <- 2(0x32) */
  0x22, /* " <- 3(0x33) */
  0x27, /* ' <- 4(0x34) */
  0x28, /* ( <- 5(0x35) */
  0x2d, /* - <- 6(0x36) */
  0x7d, /* } <- 7(0x37) */
  0x5f, /* _ <- 8(0x38) */
  0x2f, /* / <- 9(0x39) */
  0x3a, /* : */
  0x6d, /* m <- ;(0x3b) */
  0x3c, /* < */
  0x3d, /* = */
  0x3e, /* > */
  0x3f, /* ? */
  0x40, /* @ */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x5e, /* ^ <- [(0x5b) */
  0x2a, /* * <- \(0x5c) */
  0x24, /* $ <- ](0x5d) */
  0x5e, /* ^ */
  0x5f, /* _ */
  0x2a, /* * <- `(0x60) */
  0x71, /* q <- a(0x61) */
  0x62, /* b */
  0x63, /* c */
  0x64, /* d */
  0x65, /* e */
  0x66, /* f */
  0x67, /* g */
  0x68, /* h */
  0x69, /* i */
  0x6a, /* j */
  0x6b, /* k */
  0x6c, /* l */
  0x2c, /* , <- m(0x6d) */
  0x6e, /* n */
  0x6f, /* o */
  0x70, /* p */
  0x61, /* a <- q(0x71) */
  0x72, /* r */
  0x73, /* s */
  0x74, /* t */
  0x75, /* u */
  0x76, /* v */
  0x7a, /* z <- w(0x77) */
  0x78, /* x */
  0x79, /* y */
  0x77, /* w <- z(0x7a) */
};

static u_char fr_shift_tbl[] = {
  0x31, /* 1 <- !(0x21) */
  0x25, /* % <- "(0x22) */
  0x33, /* 3 <- #(0x23) */
  0x34, /* 4 <- $(0x24) */
  0x35, /* 5 <- %(0x25) */
  0x37, /* 7 <- &(0x26) */
  0x27, /* ' */
  0x39, /* 9 <- ((0x28) */
  0x30, /* 0 <- )(0x29) */
  0x38, /* 8 <- *(0x2a) */
  0x2b, /* + */
  0x2c, /* , */
  0x2d, /* - */
  0x2e, /* . */
  0x2f, /* / */
  0x30, /* 0 */
  0x31, /* 1 */
  0x32, /* 2 */
  0x33, /* 3 */
  0x34, /* 4 */
  0x35, /* 5 */
  0x36, /* 6 */
  0x37, /* 7 */
  0x38, /* 8 */
  0x39, /* 9 */
  0x3a, /* : */
  0x3b, /* ; */
  0x2e, /* . <- <(0x3c) */
  0x3d, /* = */
  0x2f, /* / <- >(0x3e) */
  0x5c, /* \ <- ?(0x3f) */
  0x32, /* 2 <- @(0x40) */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x5b, /* [ */
  0x5c, /* \ */
  0x5d, /* ] */
  0x36, /* 6 <- ^(0x5e) */
  0x5d, /* ] <- _(0x5f) */
  0x60, /* ` */
  0x61, /* a */
  0x62, /* b */
  0x63, /* c */
  0x64, /* d */
  0x65, /* e */
  0x66, /* f */
  0x67, /* g */
  0x68, /* h */
  0x69, /* i */
  0x6a, /* j */
  0x6b, /* k */
  0x6c, /* l */
  0x6d, /* m */
  0x6e, /* n */
  0x6f, /* o */
  0x70, /* p */
  0x71, /* q */
  0x72, /* r */
  0x73, /* s */
  0x74, /* t */
  0x75, /* u */
  0x76, /* v */
  0x77, /* w */
  0x78, /* x */
  0x79, /* y */
  0x7a, /* z */
  0x3c, /* < <- {(0x7b) */
  0x23, /* # <- |(0x7c) */
  0x3e, /* > <- }(0x7d) */
};

static u_char fr_alt_tbl[] = {
  0x5d, /* ] <- -(0x2d) */
  0x2e, /* . */
  0x2f, /* / */
  0x40, /* @ <- 0(0x30) */
  0x31, /* 1 */
  0x7e, /* ~ <- 2(0x32) */
  0x23, /* # <- 3(0x33) */
  0x7b, /* { <- 4(0x34) */
  0x5b, /* [ <- 5(0x35) */
  0x7c, /* | <- 6(0x36) */
  0x60, /* ` <- 7(0x37) */
  0x5c, /* \ <- 8(0x38) */
  0x5e, /* ^ <- 9(0x39) */
  0x3a, /* : */
  0x3b, /* ; */
  0x7c, /* | <- <(0x3c) */
  0x7d, /* } <- =(0x3d) */
  0x3e, /* > */
  0x3f, /* ? */
  0x40, /* @ */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x5b, /* [ */
  0x5c, /* \ */
  0x7e, /* ~ <- ](0x5d) */
};

/* US -> es (generated by dump4mlterm https://github.com/arakiken/kbd/tree/dump-for-mlterm) */
static u_char es_normal_tbl[] = {
  0x2b, /* + <- '(0x27) */
  0x28, /* ( */
  0x29, /* ) */
  0x2a, /* * */
  0x2b, /* + */
  0x2c, /* , */
  0x27, /* ' <- -(0x2d) */
  0x2e, /* . */
  0x2d, /* - <- /(0x2f) */
  0x30, /* 0 */
  0x31, /* 1 */
  0x32, /* 2 */
  0x33, /* 3 */
  0x34, /* 4 */
  0x35, /* 5 */
  0x36, /* 6 */
  0x37, /* 7 */
  0x38, /* 8 */
  0x39, /* 9 */
  0x3a, /* : */
  0x3b, /* ; */
  0x3c, /* < */
  0xa1, /* ¡ <- =(0x3d) */
  0x3e, /* > */
  0x3f, /* ? */
  0x40, /* @ */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x27, /* ' <- [(0x5b) */
  0x5d, /* ] <- \(0x5c) */
  0x5b, /* [ <- ](0x5d) */
  0x5e, /* ^ */
  0x5f, /* _ */
  0xba, /* º <- `(0x60) */
};

static u_char es_shift_tbl[] = {
  0x2a, /* * <- "(0x22) */
  0x60, /* ` <- #(0x23) */
  0x24, /* $ */
  0x25, /* % */
  0x2f, /* / <- &(0x26) */
  0x27, /* ' */
  0x29, /* ) <- ((0x28) */
  0x3d, /* = <- )(0x29) */
  0x28, /* ( <- *(0x2a) */
  0xbf, /* ¿ <- +(0x2b) */
  0x2c, /* , */
  0x2d, /* - */
  0x2e, /* . */
  0x2f, /* / */
  0x30, /* 0 */
  0x31, /* 1 */
  0x32, /* 2 */
  0x33, /* 3 */
  0x34, /* 4 */
  0x35, /* 5 */
  0x36, /* 6 */
  0x37, /* 7 */
  0x38, /* 8 */
  0x39, /* 9 */
  0x3a, /* : */
  0x3b, /* ; */
  0x3b, /* ; <- <(0x3c) */
  0x3d, /* = */
  0x3a, /* : <- >(0x3e) */
  0x5f, /* _ <- ?(0x3f) */
  0x22, /* " <- @(0x40) */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x5b, /* [ */
  0x5c, /* \ */
  0x5d, /* ] */
  0x26, /* & <- ^(0x5e) */
  0x3f, /* ? <- _(0x5f) */
  0x60, /* ` */
  0x61, /* a */
  0x62, /* b */
  0x63, /* c */
  0x64, /* d */
  0x65, /* e */
  0x66, /* f */
  0x67, /* g */
  0x68, /* h */
  0x69, /* i */
  0x6a, /* j */
  0x6b, /* k */
  0x6c, /* l */
  0x6d, /* m */
  0x6e, /* n */
  0x6f, /* o */
  0x70, /* p */
  0x71, /* q */
  0x72, /* r */
  0x73, /* s */
  0x74, /* t */
  0x75, /* u */
  0x76, /* v */
  0x77, /* w */
  0x78, /* x */
  0x79, /* y */
  0x7a, /* z */
  0xa8, /* ¨ <- {(0x7b) */
  0x7d, /* } <- |(0x7c) */
  0x7b, /* { <- }(0x7d) */
  0xaa, /* ª <- ~(0x7e) */
};

static u_char es_alt_tbl[] = {
  0x7e, /* ~ <- '(0x27) */
  0x28, /* ( */
  0x29, /* ) */
  0x2a, /* * */
  0x2b, /* + */
  0x2c, /* , */
  0x2d, /* - */
  0x2e, /* . */
  0x2f, /* / */
  0x30, /* 0 */
  0x7c, /* | <- 1(0x31) */
  0x40, /* @ <- 2(0x32) */
  0x23, /* # <- 3(0x33) */
  0x34, /* 4 */
  0x5e, /* ^ <- 5(0x35) */
  0xac, /* ¬ <- 6(0x36) */
  0x5c, /* \ <- 7(0x37) */
  0x38, /* 8 */
  0x39, /* 9 */
  0x3a, /* : */
  0x3b, /* ; */
  0x3c, /* < */
  0x3d, /* = */
  0x3e, /* > */
  0x3f, /* ? */
  0x40, /* @ */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0xb0, /* ° <- [(0x5b) */
  0x5c, /* \ */
  0x5d, /* ] */
  0x5e, /* ^ */
  0x5f, /* _ */
  0x5c, /* \ <- `(0x60) */
  0x61, /* a */
  0x62, /* b */
  0x63, /* c */
  0x64, /* d */
  0xa4, /* ¤ <- e(0x65) */
};

/* US -> pt (generated by dump4mlterm https://github.com/arakiken/kbd/tree/dump-for-mlterm) */
static u_char pt_normal_tbl[] = {
  0x7e, /* ~ <- '(0x27) */
  0x28, /* ( */
  0x29, /* ) */
  0x2a, /* * */
  0x2b, /* + */
  0x2c, /* , */
  0x2d, /* - */
  0x2e, /* . */
  0x3b, /* ; <- /(0x2f) */
  0x30, /* 0 */
  0x31, /* 1 */
  0x32, /* 2 */
  0x33, /* 3 */
  0x34, /* 4 */
  0x35, /* 5 */
  0x36, /* 6 */
  0x37, /* 7 */
  0x38, /* 8 */
  0x39, /* 9 */
  0x3a, /* : */
  0x3b, /* ; */
  0x3c, /* < */
  0x3d, /* = */
  0x3e, /* > */
  0x3f, /* ? */
  0x40, /* @ */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x27, /* ' <- [(0x5b) */
  0x5d, /* ] <- \(0x5c) */
  0x5b, /* [ <- ](0x5d) */
  0x5e, /* ^ */
  0x5f, /* _ */
  0x27, /* ' <- `(0x60) */
};

static u_char pt_shift_tbl[] = {
  0x5e, /* ^ <- "(0x22) */
  0x23, /* # */
  0x24, /* $ */
  0x25, /* % */
  0x26, /* & */
  0x27, /* ' */
  0x28, /* ( */
  0x29, /* ) */
  0x2a, /* * */
  0x2b, /* + */
  0x2c, /* , */
  0x2d, /* - */
  0x2e, /* . */
  0x2f, /* / */
  0x30, /* 0 */
  0x31, /* 1 */
  0x32, /* 2 */
  0x33, /* 3 */
  0x34, /* 4 */
  0x35, /* 5 */
  0x36, /* 6 */
  0x37, /* 7 */
  0x38, /* 8 */
  0x39, /* 9 */
  0xc7, /* Ç <- :(0x3a) */
  0x3b, /* ; */
  0x3c, /* < */
  0x3d, /* = */
  0x3e, /* > */
  0x3a, /* : <- ?(0x3f) */
  0x40, /* @ */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x5b, /* [ */
  0x5c, /* \ */
  0x5d, /* ] */
  0x5e, /* ^ */
  0x5f, /* _ */
  0x60, /* ` */
  0x61, /* a */
  0x62, /* b */
  0x63, /* c */
  0x64, /* d */
  0x65, /* e */
  0x66, /* f */
  0x67, /* g */
  0x68, /* h */
  0x69, /* i */
  0x6a, /* j */
  0x6b, /* k */
  0x6c, /* l */
  0x6d, /* m */
  0x6e, /* n */
  0x6f, /* o */
  0x70, /* p */
  0x71, /* q */
  0x72, /* r */
  0x73, /* s */
  0x74, /* t */
  0x75, /* u */
  0x76, /* v */
  0x77, /* w */
  0x78, /* x */
  0x79, /* y */
  0x7a, /* z */
  0x60, /* ` <- {(0x7b) */
  0x7d, /* } <- |(0x7c) */
  0x7b, /* { <- }(0x7d) */
  0x22, /* " <- ~(0x7e) */
};

static u_char pt_alt_tbl[] = {
  0x5c, /* \ <- .(0x2e) */
  0x2f, /* / */
  0x30, /* 0 */
  0xb9, /* ¹ <- 1(0x31) */
  0xb2, /* ² <- 2(0x32) */
  0xb3, /* ³ <- 3(0x33) */
  0x34, /* 4 */
  0xa2, /* ¢ <- 5(0x35) */
  0xac, /* ¬ <- 6(0x36) */
  0x37, /* 7 */
  0x38, /* 8 */
  0x39, /* 9 */
  0x3a, /* : */
  0x3b, /* ; */
  0x3c, /* < */
  0xa7, /* § <- =(0x3d) */
  0x3e, /* > */
  0x3f, /* ? */
  0x40, /* @ */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x5b, /* [ */
  0xba, /* º <- \(0x5c) */
  0xaa, /* ª <- ](0x5d) */
  0x5e, /* ^ */
  0x5f, /* _ */
  0x60, /* ` */
  0x61, /* a */
  0x62, /* b */
  0x63, /* c */
  0x64, /* d */
  0xa4, /* ¤ <- e(0x65) */
  0x66, /* f */
  0x67, /* g */
  0x68, /* h */
  0x69, /* i */
  0x6a, /* j */
  0x6b, /* k */
  0x6c, /* l */
  0x6d, /* m */
  0x6e, /* n */
  0x6f, /* o */
  0x70, /* p */
  0x71, /* q */
  0x72, /* r */
  0x73, /* s */
  0x74, /* t */
  0x75, /* u */
  0x76, /* v */
  0x77, /* w */
  0x78, /* x */
  0x79, /* y */
  0x7c, /* | <- z(0x7a) */
};

/* US -> jp106 (generated by dump4mlterm https://github.com/arakiken/kbd/tree/dump-for-mlterm) */
static u_char jp106_normal_tbl[] = {
  0x3a, /* : <- '(0x27) */
  0x28, /* ( */
  0x29, /* ) */
  0x2a, /* * */
  0x2b, /* + */
  0x2c, /* , */
  0x2d, /* - */
  0x2e, /* . */
  0x2f, /* / */
  0x30, /* 0 */
  0x31, /* 1 */
  0x32, /* 2 */
  0x33, /* 3 */
  0x34, /* 4 */
  0x35, /* 5 */
  0x36, /* 6 */
  0x37, /* 7 */
  0x38, /* 8 */
  0x39, /* 9 */
  0x3a, /* : */
  0x3b, /* ; */
  0x3c, /* < */
  0x5e, /* ^ <- =(0x3d) */
  0x3e, /* > */
  0x3f, /* ? */
  0x40, /* @ */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x40, /* @ <- [(0x5b) */
  0x5d, /* ] <- \(0x5c) */
  0x5b, /* [ <- ](0x5d) */
  0x5e, /* ^ */
  0x5f, /* _ */
  0x1b, /*  <- `(0x60) */
};

static u_char jp106_shift_tbl[] = {
  0x2a, /* * <- "(0x22) */
  0x23, /* # */
  0x24, /* $ */
  0x25, /* % */
  0x27, /* ' <- &(0x26) */
  0x27, /* ' */
  0x29, /* ) <- ((0x28) */
  0x7e, /* ~ <- )(0x29) */
  0x28, /* ( <- *(0x2a) */
  0x7e, /* ~ <- +(0x2b) */
  0x2c, /* , */
  0x2d, /* - */
  0x2e, /* . */
  0x2f, /* / */
  0x30, /* 0 */
  0x31, /* 1 */
  0x32, /* 2 */
  0x33, /* 3 */
  0x34, /* 4 */
  0x35, /* 5 */
  0x36, /* 6 */
  0x37, /* 7 */
  0x38, /* 8 */
  0x39, /* 9 */
  0x2b, /* + <- :(0x3a) */
  0x3b, /* ; */
  0x3c, /* < */
  0x3d, /* = */
  0x3e, /* > */
  0x3f, /* ? */
  0x22, /* " <- @(0x40) */
  0x41, /* A */
  0x42, /* B */
  0x43, /* C */
  0x44, /* D */
  0x45, /* E */
  0x46, /* F */
  0x47, /* G */
  0x48, /* H */
  0x49, /* I */
  0x4a, /* J */
  0x4b, /* K */
  0x4c, /* L */
  0x4d, /* M */
  0x4e, /* N */
  0x4f, /* O */
  0x50, /* P */
  0x51, /* Q */
  0x52, /* R */
  0x53, /* S */
  0x54, /* T */
  0x55, /* U */
  0x56, /* V */
  0x57, /* W */
  0x58, /* X */
  0x59, /* Y */
  0x5a, /* Z */
  0x5b, /* [ */
  0x5c, /* \ */
  0x5d, /* ] */
  0x26, /* & <- ^(0x5e) */
  0x3d, /* = <- _(0x5f) */
  0x60, /* ` */
  0x61, /* a */
  0x62, /* b */
  0x63, /* c */
  0x64, /* d */
  0x65, /* e */
  0x66, /* f */
  0x67, /* g */
  0x68, /* h */
  0x69, /* i */
  0x6a, /* j */
  0x6b, /* k */
  0x6c, /* l */
  0x6d, /* m */
  0x6e, /* n */
  0x6f, /* o */
  0x70, /* p */
  0x71, /* q */
  0x72, /* r */
  0x73, /* s */
  0x74, /* t */
  0x75, /* u */
  0x76, /* v */
  0x77, /* w */
  0x78, /* x */
  0x79, /* y */
  0x7a, /* z */
  0x60, /* ` <- {(0x7b) */
  0x7d, /* } <- |(0x7c) */
  0x7b, /* { <- }(0x7d) */
};


/* --- static functions --- */

/*
 * methods of ui_im_t
 */

static void destroy(ui_im_t *im) {
  im_kbd_t *kbd;

  kbd = (im_kbd_t*)im;

  if (kbd->isciikey_state) {
    (*syms->vt_isciikey_state_destroy)(kbd->isciikey_state);
  }

  if (kbd->parser) {
    (*kbd->parser->destroy)(kbd->parser);
  }

  ref_count--;

#ifdef IM_KBD_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " An object was destroyed. ref_count: %d\n", ref_count);
#endif

  free(kbd);

  if (initialized && ref_count == 0) {
    (*parser_ascii->destroy)(parser_ascii);
    parser_ascii = NULL;

    initialized = 0;
  }
}

static int key_event_arabic_hebrew(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  im_kbd_t *kbd;
  size_t len;
  u_char *c;

  kbd = (im_kbd_t*)im;

  if (kbd->mode != KBD_MODE_ON) {
    return 1;
  }

  if (event->state & ~ShiftMask) {
    return 1;
  }

  if (key_char < 0x27 || key_char > 0x7e) {
    return 1;
  }

  if (kbd->type == KBD_TYPE_ARABIC) {
    if (!(c = arabic_conv_tbl[key_char - 0x27])) {
      return 1;
    }
  } else { /* kbd->type == KBD_TYPE_HEBREW */
    if (!(c = hebrew_conv_tbl[key_char - 0x27])) {
      return 1;
    }
  }

  if (*c == 0x0) {
    /* "\x00\xNN" */
    len = 1 + strlen(c + 1);
  } else {
    len = strlen(c);
  }

  (*kbd->im.listener->write_to_term)(kbd->im.listener->self, c, len, kbd->parser);

  return 0;
}

static int key_event_iscii(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  im_kbd_t *kbd;
  u_char buf[512];
  size_t len;

  kbd = (im_kbd_t*)im;

  if (kbd->mode == KBD_MODE_ASCII) {
    return 1;
  }

  if (event->state & ~ShiftMask) {
    return 1;
  }

  if (key_char < 0x21 || key_char > 0x7e) {
    return 1;
  }

  len = (*syms->vt_convert_ascii_to_iscii)(kbd->isciikey_state, buf, sizeof(buf), &key_char, 1);

  (*kbd->im.listener->write_to_term)(kbd->im.listener->self, buf, len, kbd->parser);

  return 0;
}

static int key_event_to_utf16_de(u_char *utf16, u_char key_char, u_int state) {
  if (state & ControlMask) {
    return 1;
  }

  if (state & ShiftMask) {
    if (key_char < 0x22 || key_char > 0x7d) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = de_shift_tbl[key_char - 0x22];
  } else if (state & ModMask) {
    if (key_char < 0x30 || key_char > 0x71) {
      return 1;
    }

    if (key_char == 'e') {
      utf16[0] = 0x20;
      utf16[1] = 0xac;
    } else {
      utf16[0] = 0x0;
      utf16[1] = de_alt_tbl[key_char - 0x30];
    }
  } else {
    if (key_char < 0x27 || key_char > 0x7a) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = de_normal_tbl[key_char - 0x27];
  }

  return 0;
}

static int key_event_to_utf16_fr(u_char *utf16, u_char key_char, u_int state) {
  if (state & ControlMask) {
    return 1;
  }

  if (state & ShiftMask) {
    if (key_char < 0x21 || key_char > 0x7d) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = fr_shift_tbl[key_char - 0x21];
  } else if (state & ModMask) {
    if (key_char < 0x2d || key_char > 0x5d) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = fr_alt_tbl[key_char - 0x2d];
  } else {
    if (key_char < 0x27 || key_char > 0x7a) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = fr_normal_tbl[key_char - 0x27];
  }

  return 0;
}

static int key_event_to_utf16_es(u_char *utf16, u_char key_char, u_int state) {
  if (state & ControlMask) {
    return 1;
  }

  if (state & ShiftMask) {
    if (key_char < 0x22 || key_char > 0x7e) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = es_shift_tbl[key_char - 0x22];
  } else if (state & ModMask) {
    if (key_char < 0x27 || key_char > 0x65) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = es_alt_tbl[key_char - 0x27];
  } else {
    if (key_char < 0x27 || key_char > 0x60) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = es_normal_tbl[key_char - 0x60];
  }

  return 0;
}

static int key_event_to_utf16_pt(u_char *utf16, u_char key_char, u_int state) {
  if (state & ControlMask) {
    return 1;
  }

  if (state & ShiftMask) {
    if (key_char < 0x22 || key_char > 0x7e) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = pt_shift_tbl[key_char - 0x22];
  } else if (state & ModMask) {
    if (key_char < 0x2e || key_char > 0x7a) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = pt_alt_tbl[key_char - 0x2e];
  } else {
    if (key_char < 0x27 || key_char > 0x60) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = pt_normal_tbl[key_char - 0x27];
  }

  return 0;
}

static int key_event_to_utf16_jp106(u_char *utf16, u_char key_char, u_int state) {
  if (state & (ControlMask|ModMask)) {
    return 1;
  }

  if (state & ShiftMask) {
    if (key_char < 0x22 || key_char > 0x7d) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = jp106_shift_tbl[key_char - 0x22];
  } else {
    if (key_char < 0x27 || key_char > 0x60) {
      return 1;
    }

    utf16[0] = 0x0;
    utf16[1] = jp106_normal_tbl[key_char - 0x27];
  }

  return 0;
}

typedef int (*key_event_func)(u_char *, u_char, u_int);

static int key_event_others(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  im_kbd_t *kbd = (im_kbd_t*)im;
  u_char utf16[2];
  key_event_func funcs[] = {
    key_event_to_utf16_de, key_event_to_utf16_fr, key_event_to_utf16_es,
    key_event_to_utf16_pt, key_event_to_utf16_jp106
  };

  if (kbd->mode != KBD_MODE_ON) {
    return 1;
  }

  if ((*funcs[kbd->type - KBD_TYPE_DE])(utf16, key_char, event->state) == 0) {
    (*kbd->im.listener->write_to_term)(kbd->im.listener->self, utf16, 2, kbd->parser);

    return 0;
  } else {
    return 1;
  }
}

/* The order must be that of kbd_type_t. */
static struct {
  char *readable_name;
  char *name;
  char *locale;
  int (*key_event)(ui_im_t*, u_char, KeySym, XKeyEvent*);
} kbd_type_tbl[] = {
  { "unknown", "", "xx", NULL, },
  { "Arabic", "arabic", "ar", key_event_arabic_hebrew, },
  { "Hebrew", "hebrew", "he", key_event_arabic_hebrew, },
  { "de", "de", "de", key_event_others, },
  { "fr", "fr", "fr", key_event_others, },
  { "es", "es", "es", key_event_others, },
  { "pt", "pt", "pt", key_event_others, },
  { "jp", "jp106", "ja", key_event_others, },
  { "ISCII", "iscii", "xx", key_event_iscii, },
};

static kbd_type_t get_kbd_type(const char *name) {
  kbd_type_t type;

  for (type = 0; type < KBD_TYPE_ISCII; type++) {
    if (strcasecmp(name, kbd_type_tbl[type].name) == 0) {
      return type;
    }
  }

  if (strncmp(name, "iscii", 5) == 0) {
    return KBD_TYPE_ISCII;
  }

  return KBD_TYPE_UNKNOWN;
}

static kbd_type_t guess_kbd_type(const char *locale) {
  if (locale) {
    kbd_type_t type;

    for (type = KBD_TYPE_UNKNOWN + 1; type < KBD_TYPE_ISCII; type++) {
      if (strncmp(locale, kbd_type_tbl[type].locale, 2) == 0) {
        return type;
      }
    }
  }

  return KBD_TYPE_UNKNOWN;
}

static int switch_mode(ui_im_t *im) {
  im_kbd_t *kbd;

  kbd = (im_kbd_t*)im;

  if (kbd->type == KBD_TYPE_UNKNOWN) {
    return 0;
  }

  if (kbd->type == KBD_TYPE_ISCII) {
    if (kbd->isciikey_state) {
      (*syms->vt_isciikey_state_destroy)(kbd->isciikey_state);
      kbd->isciikey_state = NULL;
    }

    /* Inscript => Phonetic => US ASCII => Inscript ... */

    if (kbd->mode == KBD_MODE_ASCII) {
      kbd->isciikey_state = (*syms->vt_isciikey_state_new)(1);
      kbd->mode = KBD_MODE_ISCII_INSCRIPT;

#ifdef IM_KBD_DEBUG
      bl_debug_printf(BL_DEBUG_TAG " switched to inscript.\n");
#endif
    } else if (kbd->mode == KBD_MODE_ISCII_INSCRIPT) {
      kbd->isciikey_state = (*syms->vt_isciikey_state_new)(0);
      kbd->mode = KBD_MODE_ISCII_PHONETIC;

#ifdef IM_KBD_DEBUG
      bl_debug_printf(BL_DEBUG_TAG " switched to phonetic.\n");
#endif
    } else {
      kbd->mode = KBD_MODE_ASCII;

#ifdef IM_KBD_DEBUG
      bl_debug_printf(BL_DEBUG_TAG " switched to ascii.\n");
#endif
    }

    if ((kbd->mode == KBD_MODE_ISCII_INSCRIPT || kbd->mode == KBD_MODE_ISCII_PHONETIC) &&
        (kbd->isciikey_state == NULL)) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " vt_isciikey_state_new() failed.\n");
#endif
      kbd->mode = KBD_MODE_ASCII;
    }
  } else {
    if (kbd->mode == KBD_MODE_ASCII) {
      kbd->mode = KBD_MODE_ON;
    } else {
      kbd->mode = KBD_MODE_ASCII;
    }
  }

  if (kbd->mode == KBD_MODE_ASCII) {
    if (kbd->im.stat_screen) {
      (*kbd->im.stat_screen->destroy)(kbd->im.stat_screen);
      kbd->im.stat_screen = NULL;
    }
  } else {
    int x;
    int y;

    (*kbd->im.listener->get_spot)(kbd->im.listener->self, NULL, 0, &x, &y);

    if (kbd->im.stat_screen == NULL) {
      if (!(kbd->im.stat_screen = (*syms->ui_im_status_screen_new)(
                kbd->im.disp, kbd->im.font_man, kbd->im.color_man, kbd->im.vtparser,
                (*kbd->im.listener->is_vertical)(kbd->im.listener->self),
                (*kbd->im.listener->get_line_height)(kbd->im.listener->self), x, y))) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " ui_im_satus_screen_new() failed.\n");
#endif

        return 0;
      }
    }

    switch (kbd->mode) {
      case KBD_MODE_ON:
        /* Arabic, Hebrew, De, Fr */
        (*kbd->im.stat_screen->set)(kbd->im.stat_screen, parser_ascii,
                                    kbd_type_tbl[kbd->type].readable_name);
        break;
      case KBD_MODE_ISCII_INSCRIPT:
        (*kbd->im.stat_screen->set)(kbd->im.stat_screen, parser_ascii, "ISCII:inscript");
        break;
      case KBD_MODE_ISCII_PHONETIC:
        (*kbd->im.stat_screen->set)(kbd->im.stat_screen, parser_ascii, "ISCII:phonetic");
        break;
      default:
        break;
    }
  }

  return 1;
}

static int is_active(ui_im_t *im) { return (((im_kbd_t*)im)->mode != KBD_MODE_ASCII); }

static void focused(ui_im_t *im) {
  im_kbd_t *kbd;

  kbd = (im_kbd_t*)im;

  if (kbd->im.stat_screen) {
    (*kbd->im.stat_screen->show)(kbd->im.stat_screen);
  }
}

static void unfocused(ui_im_t *im) {
  im_kbd_t *kbd;

  kbd = (im_kbd_t*)im;

  if (kbd->im.stat_screen) {
    (*kbd->im.stat_screen->hide)(kbd->im.stat_screen);
  }
}

/* --- global functions --- */

ui_im_t *im_kbd_new(u_int64_t magic, vt_char_encoding_t term_encoding,
                    ui_im_export_syms_t *export_syms, char *opt, /* arabic/hebrew/iscii */
                    u_int mod_ignore_mask) {
  im_kbd_t *kbd;
  kbd_type_t type;

  if (magic != (u_int64_t)IM_API_COMPAT_CHECK_MAGIC) {
    bl_error_printf("Incompatible input method API.\n");

    return NULL;
  }

  if (opt == NULL || (type = get_kbd_type(opt)) == KBD_TYPE_UNKNOWN) {
    type = guess_kbd_type(bl_get_locale());

    if (type == KBD_TYPE_UNKNOWN) {
      if (IS_ISCII_ENCODING(term_encoding)) {
        type = KBD_TYPE_ISCII;
      } else {
        return NULL;
      }
    }
  }

  if (!initialized) {
    syms = export_syms;

    if (!(parser_ascii = (*syms->vt_char_encoding_parser_new)(VT_ISO8859_1))) {
      return NULL;
    }

    initialized = 1;
  }

  kbd = NULL;

  if (!(kbd = malloc(sizeof(im_kbd_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    goto error;
  }

  kbd->type = type;
  kbd->mode = KBD_MODE_ASCII;
  kbd->isciikey_state = NULL;
  kbd->parser = NULL;

  if (kbd->type == KBD_TYPE_ISCII) {
    vt_char_encoding_t iscii_encoding;

    if (IS_ISCII_ENCODING(term_encoding)) {
      iscii_encoding = term_encoding;
    } else if (!opt ||
               (iscii_encoding = (*syms->vt_get_char_encoding)(opt)) == VT_UNKNOWN_ENCODING) {
      iscii_encoding = VT_ISCII_HINDI;
    }

    if (!(kbd->parser = (*syms->vt_char_encoding_parser_new)(iscii_encoding))) {
      goto error;
    }
  } else {
    if (!(kbd->parser = ef_utf16_parser_new())) {
      goto error;
    }
  }

  /*
   * set methods of ui_im_t
   */
  kbd->im.destroy = destroy;
  if (kbd->type == KBD_TYPE_ISCII) {
    kbd->im.key_event = key_event_iscii;
  } else if (kbd->type == KBD_TYPE_ARABIC || kbd->type == KBD_TYPE_HEBREW) {
    kbd->im.key_event = key_event_arabic_hebrew;
  } else {
    kbd->im.key_event = key_event_others;
  }
  kbd->im.switch_mode = switch_mode;
  kbd->im.is_active = is_active;
  kbd->im.focused = focused;
  kbd->im.unfocused = unfocused;

  ref_count++;

#ifdef IM_KBD_DEBUG
  bl_debug_printf("New object was created. ref_count is %d.\n", ref_count);
#endif

  return (ui_im_t*)kbd;

error:
  if (kbd) {
    if (kbd->parser) {
      (*kbd->parser->destroy)(kbd->parser);
    }

    free(kbd);
  }

  if (initialized && ref_count) {
    (*parser_ascii->destroy)(parser_ascii);
    parser_ascii = NULL;

    initialized = 0;
  }

  return NULL;
}

/* --- API for external tools --- */

im_info_t *im_kbd_get_info(char *locale, char *encoding) {
  im_info_t *result;
  kbd_type_t type;
  char **args;
  char **readable_args;

  if (!(result = malloc(sizeof(im_info_t)))) {
    return NULL;
  }

  result->num_args = KBD_TYPE_MAX - 1 /* ISCII */ + 10 /* ISCIIXXX */;

  if (!(args = malloc(sizeof(char*) * result->num_args))) {
    free(result);
    return NULL;
  }

  if (!(readable_args = malloc(sizeof(char*) * result->num_args))) {
    free(args);
    free(result);
    return NULL;
  }

  result->args = args;
  result->readable_args = readable_args;

  if ((type = guess_kbd_type(locale)) == KBD_TYPE_UNKNOWN) {
    if (strncmp(encoding, "ISCII", 5) == 0) {
      *readable_args = malloc(6 /* "Indic " */ + 2 /* () */ + strlen(encoding + 5) + 1);
      sprintf(*(readable_args++), "Indic (%s)", encoding + 5);
    } else {
      *(readable_args++) = strdup("unknown");
    }
  } else {
    *(readable_args++) = strdup(kbd_type_tbl[type].readable_name);
  }

  *(args++) = strdup("");

  for (type = KBD_TYPE_UNKNOWN + 1; type < KBD_TYPE_ISCII; type++) {
    *(readable_args++) = strdup(kbd_type_tbl[type].readable_name);
    *(args++) = strdup(kbd_type_tbl[type].name);
  }

  *(readable_args++) = strdup("Indic (ASSAMESE)");
  *(readable_args++) = strdup("Indic (BENGALI)");
  *(readable_args++) = strdup("Indic (GUJARATI)");
  *(readable_args++) = strdup("Indic (HINDI)");
  *(readable_args++) = strdup("Indic (KANNADA)");
  *(readable_args++) = strdup("Indic (MALAYALAM)");
  *(readable_args++) = strdup("Indic (ORIYA)");
  *(readable_args++) = strdup("Indic (PUNJABI)");
  *(readable_args++) = strdup("Indic (TAMIL)");
  *(readable_args++) = strdup("Indic (TELUGU)");

  *(args++) = strdup("isciiassamese");
  *(args++) = strdup("isciibengali");
  *(args++) = strdup("isciigujarati");
  *(args++) = strdup("isciihindi");
  *(args++) = strdup("isciikannada");
  *(args++) = strdup("isciimalayalam");
  *(args++) = strdup("isciioriya");
  *(args++) = strdup("isciipunjabi");
  *(args++) = strdup("isciitamil");
  *(args++) = strdup("isciitelugu");

  result->id = strdup("kbd");
  result->name = strdup("keyboard");

  return result;
}
