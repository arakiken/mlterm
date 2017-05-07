/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
 * im_iiimf_keymap.c - IIIMF plugin for mlterm (key mapping part)
 *
 * Copyright 2005 Seiichi SATO <ssato@sh.rim.or.jp>
 */

#include  <pobl/bl_types.h> /* HAVE_STDINT_H */
#include  <iiimcf.h>
#include  <ui_im.h>

int xksym_to_iiimfkey(KeySym ksym, IIIMP_int32 *kchar, IIIMP_int32 *kcode) {
  switch (ksym) {
  /* Latin 1 */
  case ' ' /* XK_space */:
    *kcode = IIIMF_KEYCODE_SPACE;
    *kchar = 0x0020;
    break;
  case '!' /* XK_exclam */:
    *kcode = IIIMF_KEYCODE_EXCLAMATION_MARK;
    *kchar = 0x0021;
    break;
  case '\"' /* XK_quotedbl */:
    *kcode = IIIMF_KEYCODE_QUOTEDBL;
    *kchar = 0x0022;
    break;
  case '#' /* XK_numbersign */:
    *kcode = IIIMF_KEYCODE_NUMBER_SIGN;
    *kchar = 0x0023;
    break;
  case '$' /* XK_dollar */:
    *kcode = IIIMF_KEYCODE_DOLLAR;
    *kchar = 0x0024;
    break;
  case '%' /* XK_percent */:
    *kcode = IIIMF_KEYCODE_5; /* XXX */
    *kchar = 0x0025;
    break;
  case '&' /* XK_ampersand */:
    *kcode = IIIMF_KEYCODE_AMPERSAND;
    *kchar = 0x0026;
    break;
  case '\'' /* XK_apostrophe */:
  /* case XK_quoteright: */
    *kcode = IIIMF_KEYCODE_QUOTE;
    *kchar = 0x0027;
    break;
  case '(' /* XK_parenleft */:
    *kcode = IIIMF_KEYCODE_LEFT_PARENTHESIS;
    *kchar = 0x0028;
    break;
  case ')' /* XK_parenright */:
    *kcode = IIIMF_KEYCODE_RIGHT_PARENTHESIS;
    *kchar = 0x0029;
    break;
  case '*' /* XK_asterisk */:
    *kcode = IIIMF_KEYCODE_ASTERISK;
    *kchar = 0x002a;
    break;
  case '+' /* XK_plus */:
    *kcode = IIIMF_KEYCODE_PLUS;
    *kchar = 0x002b;
    break;
  case ',' /* XK_comma */:
    *kcode = IIIMF_KEYCODE_COMMA;
    *kchar = 0x002c;
    break;
  case '-' /* XK_minus */:
    *kcode = IIIMF_KEYCODE_MINUS;
    *kchar = 0x002d;
    break;
  case '.' /* XK_period */:
    *kcode = IIIMF_KEYCODE_PERIOD;
    *kchar = 0x002e;
    break;
  case '/' /* XK_slash */:
    *kcode = IIIMF_KEYCODE_SLASH;
    *kchar = 0x002f;
    break;
  case '0' /* XK_0 */:
    *kcode = IIIMF_KEYCODE_0;
    *kchar = 0x0030;
    break;
  case '1' /* XK_1 */:
    *kcode = IIIMF_KEYCODE_1;
    *kchar = 0x0031;
    break;
  case '2' /* XK_2 */:
    *kcode = IIIMF_KEYCODE_2;
    *kchar = 0x0032;
    break;
  case '3' /* XK_3 */:
    *kcode = IIIMF_KEYCODE_3;
    *kchar = 0x0033;
    break;
  case '4' /* XK_4 */:
    *kcode = IIIMF_KEYCODE_4;
    *kchar = 0x0034;
    break;
  case '5' /* XK_5 */:
    *kcode = IIIMF_KEYCODE_5;
    *kchar = 0x0035;
    break;
  case '6' /* XK_6 */:
    *kcode = IIIMF_KEYCODE_6;
    *kchar = 0x0036;
    break;
  case '7' /* XK_7 */:
    *kcode = IIIMF_KEYCODE_7;
    *kchar = 0x0037;
    break;
  case '8' /* XK_8 */:
    *kcode = IIIMF_KEYCODE_8;
    *kchar = 0x0038;
    break;
  case '9' /* XK_9 */:
    *kcode = IIIMF_KEYCODE_9;
    *kchar = 0x0039;
    break;
  case ':' /* XK_colon */:
    *kcode = IIIMF_KEYCODE_COLON;
    *kchar = 0x003a;
    break;
  case ';' /* XK_semicolon */:
    *kcode = IIIMF_KEYCODE_SEMICOLON;
    *kchar = 0x003b;
    break;
  case '<' /* XK_less */:
    *kcode = IIIMF_KEYCODE_LESS;
    *kchar = 0x003c;
    break;
  case '=' /* XK_equal */:
    *kcode = IIIMF_KEYCODE_EQUALS; /* XXX */
    *kchar = 0x003d;
    break;
  case '>' /* XK_greater */:
    *kcode = IIIMF_KEYCODE_GREATER;
    *kchar = 0x003e;
    break;
  case '?' /* XK_question */:
    *kcode = IIIMF_KEYCODE_SLASH; /* XXX */
    *kchar = 0x003f;
    break;
  case '@' /* XK_at */:
    *kcode = IIIMF_KEYCODE_AT;
    *kchar = 0x0040;
    break;
  case 'A' /* XK_A */:
    *kcode = IIIMF_KEYCODE_A;
    *kchar = 0x0041;
    break;
  case 'B' /* XK_B */:
    *kcode = IIIMF_KEYCODE_B;
    *kchar = 0x0042;
    break;
  case 'C' /* XK_C */:
    *kcode = IIIMF_KEYCODE_C;
    *kchar = 0x0043;
    break;
  case 'D' /* XK_D */:
    *kcode = IIIMF_KEYCODE_D;
    *kchar = 0x0044;
    break;
  case 'E' /* XK_E */:
    *kcode = IIIMF_KEYCODE_E;
    *kchar = 0x0045;
    break;
  case 'F' /* XK_F */:
    *kcode = IIIMF_KEYCODE_F;
    *kchar = 0x0046;
    break;
  case 'G' /* XK_G */:
    *kcode = IIIMF_KEYCODE_G;
    *kchar = 0x0047;
    break;
  case 'H' /* XK_H */:
    *kcode = IIIMF_KEYCODE_H;
    *kchar = 0x0048;
    break;
  case 'I' /* XK_I */:
    *kcode = IIIMF_KEYCODE_I;
    *kchar = 0x0049;
    break;
  case 'J' /* XK_J */:
    *kcode = IIIMF_KEYCODE_J;
    *kchar = 0x004a;
    break;
  case 'K' /* XK_K */:
    *kcode = IIIMF_KEYCODE_K;
    *kchar = 0x004b;
    break;
  case 'L' /* XK_L */:
    *kcode = IIIMF_KEYCODE_L;
    *kchar = 0x004c;
    break;
  case 'M' /* XK_M */:
    *kcode = IIIMF_KEYCODE_M;
    *kchar = 0x004d;
    break;
  case 'N' /* XK_N */:
    *kcode = IIIMF_KEYCODE_N;
    *kchar = 0x004e;
    break;
  case 'O' /* XK_O */:
    *kcode = IIIMF_KEYCODE_O;
    *kchar = 0x004f;
    break;
  case 'P' /* XK_P */:
    *kcode = IIIMF_KEYCODE_P;
    *kchar = 0x0050;
    break;
  case 'Q' /* XK_Q */:
    *kcode = IIIMF_KEYCODE_Q;
    *kchar = 0x0051;
    break;
  case 'R' /* XK_R */:
    *kcode = IIIMF_KEYCODE_R;
    *kchar = 0x0052;
    break;
  case 'S' /* XK_S */:
    *kcode = IIIMF_KEYCODE_S;
    *kchar = 0x0053;
    break;
  case 'T' /* XK_T */:
    *kcode = IIIMF_KEYCODE_T;
    *kchar = 0x0054;
    break;
  case 'U' /* XK_U */:
    *kcode = IIIMF_KEYCODE_U;
    *kchar = 0x0055;
    break;
  case 'V' /* XK_V */:
    *kcode = IIIMF_KEYCODE_V;
    *kchar = 0x0056;
    break;
  case 'W' /* XK_W */:
    *kcode = IIIMF_KEYCODE_W;
    *kchar = 0x0057;
    break;
  case 'X' /* XK_X */:
    *kcode = IIIMF_KEYCODE_X;
    *kchar = 0x0058;
    break;
  case 'Y' /* XK_Y */:
    *kcode = IIIMF_KEYCODE_Y;
    *kchar = 0x0059;
    break;
  case 'Z' /* XK_Z */:
    *kcode = IIIMF_KEYCODE_Z;
    *kchar = 0x005a;
    break;
  case '[' /* XK_bracketleft */:
    *kcode = IIIMF_KEYCODE_OPEN_BRACKET;
    *kchar = 0x005b;
    break;
  case '\\' /* XK_backslash */:
    *kcode = IIIMF_KEYCODE_BACK_SLASH;
    *kchar = 0x005c;
    break;
  case ']' /* XK_bracketright */:
    *kcode = IIIMF_KEYCODE_CLOSE_BRACKET;
    *kchar = 0x005d;
    break;
  case '^' /* XK_asciicircum */:
    *kcode = IIIMF_KEYCODE_CIRCUMFLEX;
    *kchar = 0x005e;
    break;
  case '_' /* XK_underscore */:
    *kcode = IIIMF_KEYCODE_UNDERSCORE;
    *kchar = 0x005f;
    break;
  case '`' /* XK_grave */:
/* case XK_quoteleft: */
    *kcode = IIIMF_KEYCODE_BACK_QUOTE; /* XXX */
    *kchar = 0x0060;
    break;
  case 'a' /* XK_a */:
    *kcode = IIIMF_KEYCODE_A;
    *kchar = 0x0061;
    break;
  case 'b' /* XK_b */:
    *kcode = IIIMF_KEYCODE_B;
    *kchar = 0x0062;
    break;
  case 'c' /* XK_c */:
    *kcode = IIIMF_KEYCODE_C;
    *kchar = 0x0063;
    break;
  case 'd' /* XK_d */:
    *kcode = IIIMF_KEYCODE_D;
    *kchar = 0x0064;
    break;
  case 'e' /* XK_e */:
    *kcode = IIIMF_KEYCODE_E;
    *kchar = 0x0065;
    break;
  case 'f' /* XK_f */:
    *kcode = IIIMF_KEYCODE_F;
    *kchar = 0x0066;
    break;
  case 'g' /* XK_g */:
    *kcode = IIIMF_KEYCODE_G;
    *kchar = 0x0067;
    break;
  case 'h' /* XK_h */:
    *kcode = IIIMF_KEYCODE_H;
    *kchar = 0x0068;
    break;
  case 'i' /* XK_i */:
    *kcode = IIIMF_KEYCODE_I;
    *kchar = 0x0069;
    break;
  case 'j' /* XK_j */:
    *kcode = IIIMF_KEYCODE_J;
    *kchar = 0x006a;
    break;
  case 'k' /* XK_k */:
    *kcode = IIIMF_KEYCODE_K;
    *kchar = 0x006b;
    break;
  case 'l' /* XK_l */:
    *kcode = IIIMF_KEYCODE_L;
    *kchar = 0x006c;
    break;
  case 'm' /* XK_m */:
    *kcode = IIIMF_KEYCODE_M;
    *kchar = 0x006d;
    break;
  case 'n' /* XK_n */:
    *kcode = IIIMF_KEYCODE_N;
    *kchar = 0x006e;
    break;
  case 'o' /* XK_o */:
    *kcode = IIIMF_KEYCODE_O;
    *kchar = 0x006f;
    break;
  case 'p' /* XK_p */:
    *kcode = IIIMF_KEYCODE_P;
    *kchar = 0x0070;
    break;
  case 'q' /* XK_q */:
    *kcode = IIIMF_KEYCODE_Q;
    *kchar = 0x0071;
    break;
  case 'r' /* XK_r */:
    *kcode = IIIMF_KEYCODE_R;
    *kchar = 0x0072;
    break;
  case 's' /* XK_s */:
    *kcode = IIIMF_KEYCODE_S;
    *kchar = 0x0073;
    break;
  case 't' /* XK_t */:
    *kcode = IIIMF_KEYCODE_T;
    *kchar = 0x0074;
    break;
  case 'u' /* XK_u */:
    *kcode = IIIMF_KEYCODE_U;
    *kchar = 0x0075;
    break;
  case 'v' /* XK_v */:
    *kcode = IIIMF_KEYCODE_V;
    *kchar = 0x0076;
    break;
  case 'w' /* XK_w */:
    *kcode = IIIMF_KEYCODE_W;
    *kchar = 0x0077;
    break;
  case 'x' /* XK_x */:
    *kcode = IIIMF_KEYCODE_X;
    *kchar = 0x0078;
    break;
  case 'y' /* XK_y */:
    *kcode = IIIMF_KEYCODE_Y;
    *kchar = 0x0079;
    break;
  case 'z' /* XK_z */:
    *kcode = IIIMF_KEYCODE_Z;
    *kchar = 0x007a;
    break;
  case '{' /* XK_braceleft */:
    *kcode = IIIMF_KEYCODE_BRACELEFT;
    *kchar = 0x007b;
    break;
  case '|' /* XK_bar */:
    *kcode = IIIMF_KEYCODE_BACK_SLASH; /* XXX */
    *kchar = 0x007c;
    break;
  case '}' /* XK_braceright */:
    *kcode = IIIMF_KEYCODE_BRACERIGHT;
    *kchar = 0x007d;
    break;
  case '~' /* XK_asciitilde */:
    *kcode = IIIMF_KEYCODE_BACK_QUOTE; /* XXX */
    *kchar = 0x007e;
    break;
  case 0xa0 /* XK_nobreakspace */:
    *kcode = IIIMF_KEYCODE_UNDEFINED; /* FIXME */
    *kchar = 0x00a0;
    break;
  case 0xa1 /* XK_exclamdown */:
    *kcode = IIIMF_KEYCODE_INVERTED_EXCLAMATION_MARK;
    *kchar = 0x00a1;
    break;
  case 0xa2 /* XK_cent */:
  case 0xa3 /* XK_sterling */:
  case 0xa4 /* XK_currency */:
  case 0xa5 /* XK_yen */:
  case 0xa6 /* XK_brokenbar */:
  case 0xa7 /* XK_section */:
  case 0xa8 /* XK_diaeresis */:
  case 0xa9 /* XK_copyright */:
  case 0xaa /* XK_ordfeminine */:
  case 0xab /* XK_guillemotleft */:
  case 0xac /* XK_notsign */:
  case 0xad /* XK_hyphen */:
  case 0xae /* XK_registered */:
  case 0xaf /* XK_macron */:
  case 0xb0 /* XK_degree */:
  case 0xb1 /* XK_plusminus */:
  case 0xb2 /* XK_twosuperior */:
  case 0xb3 /* XK_threesuperior */:
  case 0xb4 /* XK_acute */:
  case 0xb5 /* XK_mu */:
  case 0xb6 /* XK_paragraph */:
  case 0xb7 /* XK_periodcentered */:
  case 0xb8 /* XK_cedilla */:
  case 0xb9 /* XK_onesuperior */:
  case 0xba /* XK_masculine */:
  case 0xbb /* XK_guillemotright */:
  case 0xbc /* XK_onequarter */:
  case 0xbd /* XK_onehalf */:
  case 0xbe /* XK_threequarters */:
  case 0xbf /* XK_questiondown */:
  case 0xc0 /* XK_Agrave */:
  case 0xc1 /* XK_Aacute */:
  case 0xc2 /* XK_Acircumflex */:
  case 0xc3 /* XK_Atilde */:
  case 0xc4 /* XK_Adiaeresis */:
  case 0xc5 /* XK_Aring */:
  case 0xc6 /* XK_AE */:
  case 0xc7 /* XK_Ccedilla */:
  case 0xc8 /* XK_Egrave */:
  case 0xc9 /* XK_Eacute */:
  case 0xca /* XK_Ecircumflex */:
  case 0xcb /* XK_Ediaeresis */:
  case 0xcc /* XK_Igrave */:
  case 0xcd /* XK_Iacute */:
  case 0xce /* XK_Icircumflex */:
  case 0xcf /* XK_Idiaeresis */:
  case 0xd0 /* XK_ETH, XK_Eth */:
  case 0xd1 /* XK_Ntilde */:
  case 0xd2 /* XK_Ograve */:
  case 0xd3 /* XK_Oacute */:
  case 0xd4 /* XK_Ocircumflex */:
  case 0xd5 /* XK_Otilde */:
  case 0xd6 /* XK_Odiaeresis */:
  case 0xd7 /* XK_multiply */:
  case 0xd8 /* XK_Ooblique, XK_Oslash */:
  case 0xd9 /* XK_Ugrave */:
  case 0xda /* XK_Uacute */:
  case 0xdb /* XK_Ucircumflex */:
  case 0xdc /* XK_Udiaeresis */:
  case 0xdd /* XK_Yacute */:
  case 0xde /* XK_THORN, XK_Thorn */:
  case 0xdf /* XK_ssharp */:
  case 0xe0 /* XK_agrave */:
  case 0xe1 /* XK_aacute */:
  case 0xe2 /* XK_acircumflex */:
  case 0xe3 /* XK_atilde */:
  case 0xe4 /* XK_adiaeresis */:
  case 0xe5 /* XK_aring */:
  case 0xe6 /* XK_ae */:
  case 0xe7 /* XK_ccedilla */:
  case 0xe8 /* XK_egrave */:
  case 0xe9 /* XK_eacute */:
  case 0xea /* XK_ecircumflex */:
  case 0xeb /* XK_ediaeresis */:
  case 0xec /* XK_igrave */:
  case 0xed /* XK_iacute */:
  case 0xee /* XK_icircumflex */:
  case 0xef /* XK_idiaeresis */:
  case 0xf0 /* XK_eth */:
  case 0xf1 /* XK_ntilde */:
  case 0xf2 /* XK_ograve */:
  case 0xf3 /* XK_oacute */:
  case 0xf4 /* XK_ocircumflex */:
  case 0xf5 /* XK_otilde */:
  case 0xf6 /* XK_odiaeresis */:
  case 0xf7 /* XK_division */:
  case 0xf8 /* XK_oslash, XK_ooblique */:
  case 0xf9 /* XK_ugrave */:
  case 0xfa /* XK_uacute */:
  case 0xfb /* XK_ucircumflex */:
  case 0xfc /* XK_udiaeresis */:
  case 0xfd /* XK_yacute */:
  case 0xfe /* XK_thorn */:
  case 0xff /* XK_ydiaeresis */:
    *kcode = IIIMF_KEYCODE_UNDEFINED;
    *kchar = ksym;
    break;

/*
 * TODO: Latin[23489], Katakana, Arabic, Cyrillic, Greek, Technical, Special
 *     Publishing, APL, Hebrew, Thai, Korean, American, Georgian, Azeri,
 *     Vietnamese,
 */
  /* TTY Functions */
  case XK_BackSpace:
    *kcode = IIIMF_KEYCODE_BACK_SPACE;
    break;
  case XK_Tab:
    *kcode = IIIMF_KEYCODE_TAB;
    break;
  case XK_Clear:
    *kcode = IIIMF_KEYCODE_CLEAR;
    break;
  case XK_Return:
    *kcode = IIIMF_KEYCODE_ENTER;
    break;
  case XK_Pause:
    *kcode = IIIMF_KEYCODE_PAUSE;
    break;
  case XK_Scroll_Lock:
    *kcode = IIIMF_KEYCODE_SCROLL_LOCK;
    break;
  case XK_Escape:
    *kcode = IIIMF_KEYCODE_ESCAPE;
    break;
  case XK_Delete:
    *kcode = IIIMF_KEYCODE_DELETE;
    break;

  /*
   * TODO:
   * - International & multi-key character composition
   * - Japanese keyboard support
   */

  /* Cursor control & motion */
  case XK_Home:
    *kcode = IIIMF_KEYCODE_HOME;
    break;
  case XK_Left:
    *kcode = IIIMF_KEYCODE_LEFT;
    break;
  case XK_Up:
    *kcode = IIIMF_KEYCODE_UP;
    break;
  case XK_Right:
    *kcode = IIIMF_KEYCODE_RIGHT;
    break;
  case XK_Down:
    *kcode = IIIMF_KEYCODE_DOWN;
    break;
  case XK_Prior:
/* case XK_Page_Up:*/
    *kcode = IIIMF_KEYCODE_PAGE_UP;
    break;
  case XK_Next:
/* case XK_Page_Down:*/
    *kcode = IIIMF_KEYCODE_PAGE_DOWN;
    break;
  case XK_End:
    *kcode = IIIMF_KEYCODE_END;
    break;
  case XK_Begin:
    *kcode = IIIMF_KEYCODE_UNDEFINED;
    break;

  /*
   * TODO
   * - Misc Functions
   */

  /* - Keypad Functions, keypad numbers cleverly chosen to map to ascii */
#ifdef XK_KP_Space
  case XK_KP_Space:
    *kcode = IIIMF_KEYCODE_SPACE;
    break;
#endif
#ifdef XK_KP_Tab
  case XK_KP_Tab:
    *kcode = IIIMF_KEYCODE_TAB;
    break;
#endif
#ifdef XK_KP_Enter
  case XK_KP_Enter:
    *kcode = IIIMF_KEYCODE_ENTER;
    break;
#endif
  case XK_KP_F1:
    *kcode = IIIMF_KEYCODE_F1;
    break;
  case XK_KP_F2:
    *kcode = IIIMF_KEYCODE_F2;
    break;
  case XK_KP_F3:
    *kcode = IIIMF_KEYCODE_F3;
    break;
  case XK_KP_F4:
    *kcode = IIIMF_KEYCODE_F4;
    break;
  case XK_KP_Home:
    *kcode = IIIMF_KEYCODE_HOME;
    break;
  case XK_KP_Left:
    *kcode = IIIMF_KEYCODE_LEFT;
    break;
  case XK_KP_Up:
    *kcode = IIIMF_KEYCODE_UP;
    break;
  case XK_KP_Right:
    *kcode = IIIMF_KEYCODE_RIGHT;
    break;
  case XK_KP_Down:
    *kcode = IIIMF_KEYCODE_DOWN;
    break;
  case XK_KP_Prior:
/* case XK_KP_Page_Up: */
    *kcode = IIIMF_KEYCODE_PAGE_UP;
    break;
  case XK_KP_Next:
/* case XK_KP_Page_Down: */
    *kcode = IIIMF_KEYCODE_PAGE_DOWN;
    break;
  case XK_KP_End:
    *kcode = IIIMF_KEYCODE_END;
    break;
#if  0
  case XK_KP_Begin:
  case XK_KP_Insert:
    *kcode = IIIMF_KEYCODE_UNDEFINED;
    break;
#endif
  case XK_KP_Delete:
    *kcode =  IIIMF_KEYCODE_DELETE;
    break;
#ifdef XK_KP_Equal
  case XK_KP_Equal:
    *kcode = IIIMF_KEYCODE_EQUALS; /* XXX */
    *kchar = 0x003d;
    break;
#endif
#if  0
  case XK_KP_Multiply:
  case XK_KP_Add:
  case XK_KP_Separator:
  case XK_KP_Subtract:
  case XK_KP_Decimal:
  case XK_KP_Divide:
    *kcode = IIIMF_KEYCODE_UNDEFINED;
    break;
#endif
  case XK_KP_0:
    *kcode = IIIMF_KEYCODE_0;
    *kchar = 0x0030;
    break;
  case XK_KP_1:
    *kcode = IIIMF_KEYCODE_1;
    *kchar = 0x0031;
    break;
  case XK_KP_2:
    *kcode = IIIMF_KEYCODE_2;
    *kchar = 0x0032;
    break;
  case XK_KP_3:
    *kcode = IIIMF_KEYCODE_3;
    *kchar = 0x0033;
    break;
  case XK_KP_4:
    *kcode = IIIMF_KEYCODE_4;
    *kchar = 0x0034;
    break;
  case XK_KP_5:
    *kcode = IIIMF_KEYCODE_5;
    *kchar = 0x0035;
    break;
  case XK_KP_6:
    *kcode = IIIMF_KEYCODE_6;
    *kchar = 0x0036;
    break;
  case XK_KP_7:
    *kcode = IIIMF_KEYCODE_7;
    *kchar = 0x0037;
    break;
  case XK_KP_8:
    *kcode = IIIMF_KEYCODE_8;
    *kchar = 0x0038;
    break;
  case XK_KP_9:
    *kcode = IIIMF_KEYCODE_9;
    *kchar = 0x0039;
    break;

  /* Auxiliary Functions */
  case XK_F1:
    *kcode = IIIMF_KEYCODE_F1;
    break;
  case XK_F2:
    *kcode = IIIMF_KEYCODE_F2;
    break;
  case XK_F3:
    *kcode = IIIMF_KEYCODE_F3;
    break;
  case XK_F4:
    *kcode = IIIMF_KEYCODE_F4;
    break;
  case XK_F5:
    *kcode = IIIMF_KEYCODE_F5;
    break;
  case XK_F6:
    *kcode = IIIMF_KEYCODE_F6;
    break;
  case XK_F7:
    *kcode = IIIMF_KEYCODE_F7;
    break;
  case XK_F8:
    *kcode = IIIMF_KEYCODE_F8;
    break;
  case XK_F9:
    *kcode = IIIMF_KEYCODE_F9;
    break;
  case XK_F10:
    *kcode = IIIMF_KEYCODE_F10;
    break;
  case XK_F11:
/* case XK_L1: */
    *kcode = IIIMF_KEYCODE_F11;
    break;
  case XK_F12:
/* case XK_L2: */
    *kcode = IIIMF_KEYCODE_F12;
    break;
  case XK_Kanji:
    *kcode = IIIMF_KEYCODE_KANJI;
    break;
  case XK_Muhenkan:
    *kcode = IIIMF_KEYCODE_NONCONVERT;
    break;
  case XK_Henkan:
#ifdef __linux__
    *kcode = IIIMF_KEYCODE_CONVERT;
#else
    *kcode = IIIMF_KEYCODE_KANJI;
#endif
    break;
  case XK_Hiragana:
    *kcode = IIIMF_KEYCODE_HIRAGANA;
    break;
  case XK_Katakana:
    *kcode = IIIMF_KEYCODE_KATAKANA;
    break;
  case XK_Zenkaku_Hankaku:
    *kcode = IIIMF_KEYCODE_KANJI;
    break;
#if 0
  case XK_F13:
  case XK_L3:
  case XK_F14:
  case XK_L4:
  case XK_F15:
  case XK_L5:
  case XK_F16:
  case XK_L6:
  case XK_F17:
  case XK_L7:
  case XK_F18:
  case XK_L8:
  case XK_F19:
  case XK_L9:
  case XK_F20:
  case XK_L10:
  case XK_F21:
  case XK_R1:
  case XK_F22:
  case XK_R2:
  case XK_F23:
  case XK_R3:
  case XK_F24:
  case XK_R4:
  case XK_F25:
  case XK_R5:
  case XK_F26:
  case XK_R6:
  case XK_F27:
  case XK_R7:
  case XK_F28:
  case XK_R8:
  case XK_F29:
  case XK_R9:
  case XK_F30:
  case XK_R10:
  case XK_F31:
  case XK_R11:
  case XK_F32:
  case XK_R12:
  case XK_F33:
  case XK_R13:
  case XK_F34:
  case XK_R14:
  case XK_F35:
  case XK_R15:
    *kcode = IIIMF_KEYCODE_UNDEFINED;
    break;
#endif

  /*
   * TODO
   * - Modifiers
   * - ISO 9995 Function and Modifier Keys
   * - 3270 Terminal Keys
   */

  default:
    *kcode = IIIMF_KEYCODE_UNDEFINED;
    return  0;
  }

  return  1;
}

#if  0  /* not implemented yet */
int xksym_to_iiimfkey_kana(KeySym ksym, IIIMP_int32 *kchar, IIIMP_int32 *kcode) {
  return  0;
}

int xksym_to_iiimfkey_kana_shift(KeySym ksym, IIIMP_int32 *kchar, IIIMP_int32 *kcode) {
  return  0;
}
#endif
