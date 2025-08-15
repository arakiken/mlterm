/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_main_config.h"

#include <stdlib.h> /* strtol */
#include <pobl/bl_conf_io.h>
#include <pobl/bl_mem.h>    /* malloc/realloc */
#include <pobl/bl_str.h>    /* bl_str_to_uint */
#include <pobl/bl_locale.h> /* bl_get_lang */
#include <pobl/bl_args.h>
#include <pobl/bl_util.h>   /* BL_INT_TO_STR */
#include <pobl/bl_unistd.h> /* bl_setenv */

#include <vt_term_manager.h>
#include <vt_shape.h> /* vt_set_use_arabic_dynamic_comb */
#include "ui_selection_encoding.h"
#include "ui_emoji.h"
#include "ui_sb_view_factory.h"
#include "ui_draw_str.h"

#if defined(__ANDROID__) || defined(USE_QUARTZ) || defined(USE_WIN32API)
/* Shrink unused memory */
#define bl_conf_add_opt(conf, a, b, c, d, e) bl_conf_add_opt(conf, a, b, c, d, "")
#endif

/* --- static variables --- */

static char *default_display = "";

/* --- static functions --- */

static vt_char_encoding_t get_encoding(const char *value,
                                       int *is_auto_encoding /* overwritten only if auto encoding */
                                       ) {
  vt_char_encoding_t encoding;

  if (!value) {
    /* goto "auto" */
  } else if ((encoding = vt_get_char_encoding(value)) == VT_UNKNOWN_ENCODING) {
    bl_msg_printf("Use auto detected encoding instead of %s(unsupported).\n", value);
  } else {
    return encoding;
  }

  encoding = vt_get_char_encoding("auto");

  if (is_auto_encoding) {
    *is_auto_encoding = 1;
  }

  return encoding;
}

/* --- global functions --- */

void ui_prepare_for_main_config(bl_conf_t *conf) {
  char *rcpath;

  if ((rcpath = bl_get_sys_rc_path("mlterm/main"))) {
    bl_conf_read(conf, rcpath);
    free(rcpath);
  }

  if ((rcpath = bl_get_user_rc_path("mlterm/main"))) {
    bl_conf_read(conf, rcpath);
    free(rcpath);
  }

  bl_conf_add_opt(conf, '#', "initstr", 0, "init_str", "initial string sent to pty");
  bl_conf_add_opt(conf, '$', "mc", 0, "click_interval", "click interval(milisecond) [250]");
  bl_conf_add_opt(conf, '%', "logseq", 1, "logging_vt_seq",
                  "enable logging vt100 sequence [false]");

#ifdef USE_XLIB
  bl_conf_add_opt(conf, '&', "borderless", 1, "borderless", "override redirect [false]");
  bl_conf_add_opt(conf, '*', "type", 0, "type_engine",
                  "type engine "
#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
                  "[cairo]"
#elif !defined(USE_TYPE_XCORE) && defined(USE_TYPE_XFT)
                  "[xft]"
#else
                  "[xcore]"
#endif
                  );
#endif

  bl_conf_add_opt(conf, '1', "wscr", 0, "screen_width_ratio",
                  "screen width in percent against font width [100]");
#if 1 /* BACKWARD COMPAT (3.8.2 or before) */
  bl_conf_add_opt(conf, '2', "hscr", 0, "screen_height_ratio",
                  "screen height in percent against font height [100]");
#endif
#ifndef NO_IMAGE
  bl_conf_add_opt(conf, '3', "contrast", 0, "contrast",
                  "contrast of background image in percent [100]");
  bl_conf_add_opt(conf, '4', "gamma", 0, "gamma", "gamma of background image in percent [100]");
#endif
  bl_conf_add_opt(conf, '5', "big5bug", 1, "big5_buggy",
                  "manage buggy Big5 CTEXT in XFree86 4.1 or earlier [false]");
  bl_conf_add_opt(conf, '6', "stbs", 1, "static_backscroll_mode",
                  "screen is static under backscroll mode [false]");
  bl_conf_add_opt(conf, '7', "bel", 0, "bel_mode", "bel (0x07) mode (none/sound/visual) [sound]");
  bl_conf_add_opt(conf, '8', "88591", 1, "iso88591_font_for_usascii",
                  "use ISO-8859-1 font for ASCII part of any encoding [false]");
  bl_conf_add_opt(conf, '9', "crfg", 0, "cursor_fg_color", "cursor foreground color");
  bl_conf_add_opt(conf, '0', "crbg", 0, "cursor_bg_color", "cursor background color");
#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO) || \
  defined(USE_FREETYPE) || defined(USE_WIN32GUI)
  bl_conf_add_opt(conf, 'A', "aa", 1, "use_anti_alias",
                  "forcibly use anti alias font by using Xft or cairo");
#endif
  bl_conf_add_opt(conf, 'B', "sbbg", 0, "sb_bg_color", "scrollbar background color");
#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI) || defined(USE_IND) || \
    defined(USE_OT_LAYOUT)
  bl_conf_add_opt(conf, 'C', "ctl", 1, "use_ctl", "use complex text layouting [true]");
#endif
  bl_conf_add_opt(conf, 'E', "km", 0, "encoding",
                  "character encoding (AUTO/ISO-8859-*/EUC-*/UTF-8/...) [AUTO]");
  bl_conf_add_opt(conf, 'F', "sbfg", 0, "sb_fg_color", "scrollbar foreground color");
  bl_conf_add_opt(conf, 'G', "vertical", 0, "vertical_mode",
                  "vertical mode (none/cjk/mongol) [none]");
#ifndef NO_IMAGE
  bl_conf_add_opt(conf, 'H', "bright", 0, "brightness",
                  "brightness of background image in percent [100]");
#endif
#ifdef USE_XLIB
  bl_conf_add_opt(conf, 'I', "icon", 0, "icon_name", "icon name");
#endif
  bl_conf_add_opt(conf, 'J', "dyncomb", 1, "use_dynamic_comb", "use dynamic combining [false]");
  bl_conf_add_opt(conf, 'K', "metakey", 0, "mod_meta_key", "meta key [none]");
  bl_conf_add_opt(conf, 'L', "ls", 1, "use_login_shell", "turn on login shell [false]");
  bl_conf_add_opt(conf, 'M', "im", 0, "input_method",
                  "input method (default/kbd/uim/m17nlib/scim/ibus/fcitx/canna/wnn/skk/iiimf/none) [default]");
  bl_conf_add_opt(conf, 'N', "name", 0, "app_name", "application name");
  bl_conf_add_opt(conf, '\0', "role", 0, "wm_role", "application role [none]");
  bl_conf_add_opt(conf, 'O', "sbmod", 0, "scrollbar_mode",
                  "scrollbar mode (none/left/right/autohide) [none]");
  bl_conf_add_opt(conf, 'Q', "vcur", 1, "use_vertical_cursor",
                  "rearrange cursor key for vertical mode [true]");
  bl_conf_add_opt(conf, 'S', "sbview", 0, "scrollbar_view_name",
                  "scrollbar view name (simple/sample/...) [simple]");
  bl_conf_add_opt(conf, 'T', "title", 0, "title", "title name");
  bl_conf_add_opt(conf, '\0', "locktitle", 1, "use_locked_title", "use locked title [false]");
  bl_conf_add_opt(conf, 'U', "viaucs", 1, "receive_string_via_ucs",
                  "process received (pasted) strings via Unicode [false]");
  bl_conf_add_opt(conf, 'V', "varwidth", 1, "use_variable_column_width",
                  "variable column width (for proportional/ISCII) [false]");
  bl_conf_add_opt(conf, 'W', "sep", 0, "word_separators",
                  "word-separating characters for double-click [,.:;/@]");
  bl_conf_add_opt(conf, 'X', "alpha", 0, "alpha", "alpha blending for translucent [255]");
  bl_conf_add_opt(conf, 'Z', "multicol", 1, "use_multi_column_char",
                  "fullwidth character occupies two logical columns [true]");
  bl_conf_add_opt(conf, 'a', "ac", 0, "col_size_of_width_a",
                  "columns for Unicode \"EastAsianAmbiguous\" character [1]");
  bl_conf_add_opt(conf, 'b', "bg", 0, "bg_color", "background color");
#if defined(USE_XLIB) || defined(USE_CONSOLE) || defined(USE_WAYLAND)
  bl_conf_add_opt(conf, 'd', "display", 0, "display", "X server to connect");
#endif
  bl_conf_add_opt(conf, 'f', "fg", 0, "fg_color", "foreground color");
  bl_conf_add_opt(conf, 'g', "geometry", 0, "geometry",
                  "size (in characters) and position [80x24]");
  bl_conf_add_opt(conf, 'k', "meta", 0, "mod_meta_mode",
                  "mode in pressing meta key (none/esc/8bit) [8bit]");
  bl_conf_add_opt(conf, 'l', "sl", 0, "logsize",
                  "number of backlog (scrolled lines to save) ["
                  BL_INT_TO_STR(VT_LOG_SIZE_UNIT) "]");
  bl_conf_add_opt(conf, 'm', "comb", 1, "use_combining", "use combining characters [true]");
  bl_conf_add_opt(conf, 'n', "noucsfont", 1, "not_use_unicode_font",
                  "use non-Unicode fonts even in UTF-8 mode [false]");
  bl_conf_add_opt(conf, 'o', "lsp", 0, "line_space", "extra space between lines in pixels [0]");
#ifndef NO_IMAGE
  bl_conf_add_opt(conf, 'p', "pic", 0, "wall_picture", "path for wallpaper (background) image");
#endif
  bl_conf_add_opt(conf, 'r', "fade", 0, "fade_ratio",
                  "fade ratio in percent when window unfocued [100]");
  bl_conf_add_opt(conf, 's', "mdi", 1, "use_mdi", "use multiple document interface [true]");
#if 1
  bl_conf_add_opt(conf, '\0', "sb", 1, "use_scrollbar", "use scrollbar [true]");
#endif
  bl_conf_add_opt(conf, 't', "transbg", 1, "use_transbg", "use transparent background [false]");
  bl_conf_add_opt(conf, 'u', "onlyucsfont", 1, "only_use_unicode_font",
                  "use a Unicode font even in non-UTF-8 modes [false]");
  bl_conf_add_opt(conf, 'w', "fontsize", 0, "fontsize", "font size in pixels [16]");
  bl_conf_add_opt(conf, 'x', "tw", 0, "tabsize", "tab width in columns [8]");
  bl_conf_add_opt(conf, 'y', "term", 0, "termtype", "terminal type for TERM variable [xterm]");
  bl_conf_add_opt(conf, 'z', "largesmall", 0, "step_in_changing_font_size",
                  "step in changing font size in GUI configurator [1]");
  bl_conf_add_opt(conf, '\0', "bdfont", 1, "use_bold_font", "use bold fonts [true]");
  bl_conf_add_opt(conf, '\0', "itfont", 1, "use_italic_font", "use italic fonts [true]");
  bl_conf_add_opt(conf, '\0', "iconpath", 0, "icon_path",
                  "path to an imagefile to be use as an window icon");
#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)
  bl_conf_add_opt(conf, '\0', "bimode", 0, "bidi_mode", "bidi mode [normal]");
  bl_conf_add_opt(conf, '\0', "bisep", 0, "bidi_separators",
                  "Separator characters to render bidi text");
#endif
  bl_conf_add_opt(conf, '\0', "parent", 0, "parent_window", "parent window");
  bl_conf_add_opt(conf, '\0', "bd", 0, "bd_color",
                  "Color to use to display bold characters (equivalent to colorBD)");
  bl_conf_add_opt(conf, '\0', "ul", 0, "ul_color",
                  "Color to use to display underlined characters (equivalent to colorUL)");
  bl_conf_add_opt(conf, '\0', "bl", 0, "bl_color",
                  "Color to use to display blinking characters (equivalent to colorBL)");
  bl_conf_add_opt(conf, '\0', "rv", 0, "rv_color",
                  "Color to use to display reverse characters (equivalent to colorRV)");
  bl_conf_add_opt(conf, '\0', "it", 0, "it_color", "Color to use to display italic characters");
  bl_conf_add_opt(conf, '\0', "co", 0, "co_color",
                  "Color to use to display crossed-out characters");
  bl_conf_add_opt(conf, '\0', "noul", 1, "hide_underline", "Don't draw underline [false]");
  bl_conf_add_opt(conf, '\0', "ulpos", 0, "underline_offset", "underline position [0]");
  bl_conf_add_opt(conf, '\0', "blpos", 0, "baseline_offset", "baseline position [0]");
#ifdef USE_OT_LAYOUT
  bl_conf_add_opt(conf, '\0', "otl", 1, "use_ot_layout", "OpenType shape [false]");
  bl_conf_add_opt(conf, '\0', "ost", 0, "ot_script", "Script of glyph subsutitution [latn]");
  bl_conf_add_opt(conf, '\0', "oft", 0, "ot_features",
                  "Features of glyph subsutitution [liga,clig,dlig,hlig,rlig]");
#endif
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
  bl_conf_add_opt(conf, '\0', "serv", 0, "default_server", "connecting server by default");
  bl_conf_add_opt(conf, '\0', "dialog", 1, "always_show_dialog",
                  "always show dialog to input server address, password and so on [false]");
  bl_conf_add_opt(conf, '\0', "pubkey", 0, "ssh_public_key",
                  "ssh public key file "
#ifdef USE_WIN32API
                  "[%HOMEPATH%\\mlterm\\id_rsa.pub]"
#else
                  "[~/.ssh/id_rsa.pub]"
#endif
                  );
  bl_conf_add_opt(conf, '\0', "privkey", 0, "ssh_private_key",
                  "ssh private key file "
#ifdef USE_WIN32API
                  "[%HOMEPATH%\\mlterm\\id_rsa]"
#else
                  "[~/.ssh/id_rsa]"
#endif
                  );
  bl_conf_add_opt(conf, '\0', "ciphlist", 0, "cipher_list", "preferred cipher list");
  bl_conf_add_opt(conf, '\0', "x11", 1, "ssh_x11_forwarding", "allow x11 forwarding [false]");
  bl_conf_add_opt(conf, '\0', "scp", 1, "allow_scp", "allow scp [false]");
  bl_conf_add_opt(conf, '\0', "rcn", 1, "ssh_auto_reconnect",
                  "reconnect to ssh server automatically[false]");
#endif
  bl_conf_add_opt(conf, '\0', "csp", 0, "letter_space",
                  "extra space between letters in pixels [0]");
  bl_conf_add_opt(conf, '\0', "osc52", 1, "allow_osc52",
                  "allow access to clipboard by OSC 52 sequence [false]");
  bl_conf_add_opt(conf, '\0', "blink", 1, "blink_cursor", "blink cursor [false]");
  bl_conf_add_opt(conf, '\0', "border", 0, "inner_border", "inner border [2]");
  bl_conf_add_opt(conf, '\0', "lborder", 0, "layout_inner_border",
                  "inner border of layout manager [0]");
#ifndef __ANDROID__
  bl_conf_add_opt(conf, '\0', "restart", 1, "auto_restart",
                  "restart mlterm automatically if an error like segv happens. [true]");
#endif
  bl_conf_add_opt(conf, '\0', "logmsg", 1, "logging_msg",
                  "output messages to ~/.mlterm/msg.log [true]");
  bl_conf_add_opt(conf, '\0', "loecho", 1, "use_local_echo", "use local echo [false]");
  bl_conf_add_opt(conf, '\0', "altbuf", 1, "use_alt_buffer", "use alternative buffer. [true]");
  bl_conf_add_opt(conf, '\0', "colors", 1, "use_ansi_colors",
                  "recognize ANSI color change escape sequences. [true]");
  bl_conf_add_opt(conf, '\0', "exitbs", 1, "exit_backscroll_by_pty",
                  "exit backscroll mode on receiving data from pty. [false]");
  bl_conf_add_opt(conf, '\0', "shortcut", 1, "allow_change_shortcut",
                  "allow dynamic change of shortcut keys. [false]");
  bl_conf_add_opt(conf, '\0', "boxdraw", 0, "box_drawing_font",
                  "force unicode or decsp font for box-drawing characters. [noconv]");
  bl_conf_add_opt(conf, '\0', "urgent", 1, "use_urgent_bell",
                  "draw the user's attention when making a bell sound. [false]");
  bl_conf_add_opt(conf, '\0', "locale", 0, "locale", "set locale");
  bl_conf_add_opt(conf, '\0', "ucsnoconv", 0, "unicode_noconv_areas",
                  "use unicode fonts partially regardless of -n option");
  bl_conf_add_opt(conf, '\0', "fullwidth", 0, "unicode_full_width_areas",
                  "force full width regardless of EastAsianWidth.txt");
  bl_conf_add_opt(conf, '\0', "halfwidth", 0, "unicode_half_width_areas",
                  "force half width regardless of EastAsianWidth.txt");
  bl_conf_add_opt(conf, '\0', "ade", 0, "auto_detect_encodings",
                  "encodings detected automatically");
  bl_conf_add_opt(conf, '\0', "auto", 1, "use_auto_detect",
                  "detect character encoding automatically");
  bl_conf_add_opt(conf, '\0', "ldd", 1, "leftward_double_drawing",
                  "embold glyphs by drawing doubly at 1 pixel leftward "
                  "instead of rightward");
  bl_conf_add_opt(conf, '\0', "working-directory", 0, "working_directory", "working directory");
  bl_conf_add_opt(conf, '\0', "seqfmt", 0, "vt_seq_format",
                  "format of logging vt100 sequence. [raw]");
  bl_conf_add_opt(conf, '\0', "uriword", 1, "regard_uri_as_word",
                  "select uri by double-clicking it [false]");
  bl_conf_add_opt(conf, '\0', "vtcolor", 0, "vt_color_mode", "vt color mode [high]");
  bl_conf_add_opt(conf, '\0', "da1", 0, "primary_da",
                  "primary device attributes string "
#ifndef NO_IMAGE
                  "[63;1;2;3;4;7;29]"
#else
                  "[63;1;2;7;29]"
#endif
                  );
  bl_conf_add_opt(conf, '\0', "da2", 0, "secondary_da",
                  "secondary device attributes string [24;279;0]");
  bl_conf_add_opt(conf, '\0', "metaprefix", 0, "mod_meta_prefix",
                  "prefix characters in pressing meta key if mod_meta_mode = esc");
#ifdef USE_GRF
  bl_conf_add_opt(conf, '\0', "multivram", 1, "separate_wall_picture",
                  "draw wall picture on another vram. (available on 4bpp) [true]");
#endif
#ifdef ROTATABLE_DISPLAY
  bl_conf_add_opt(conf, '\0', "rotate", 0, "rotate_display", "rotate display. [none]");
#endif
#ifdef USE_CONSOLE
  bl_conf_add_opt(conf, '\0', "ckm", 0, "console_encoding", "character encoding of console [none]");
  bl_conf_add_opt(conf, '\0', "csc", 0, "console_sixel_colors",
                  "the number of sixel colors of console [16]");
  bl_conf_add_opt(conf, '\0', "csz", 0, "default_cell_size", "default cell size [8,16]");
#endif
#if (defined(__ANDROID__) && defined(USE_LIBSSH2)) || defined(USE_WIN32API)
  bl_conf_add_opt(conf, '\0', "slp", 1, "start_with_local_pty",
                  "start mlterm with local pty instead of ssh connection [false]");
#endif
  bl_conf_add_opt(conf, '\0', "trim", 1, "trim_trailing_newline_in_pasting",
                  "trim trailing newline in pasting text. [false]");
  bl_conf_add_opt(conf, '\0', "bc", 1, "broadcast",
                  "broadcast input characters to all ptys [false]");
  bl_conf_add_opt(conf, '\0', "ibc", 1, "ignore_broadcasted_chars",
                  "ignore broadcasted characters [false]");
  bl_conf_add_opt(conf, '\0', "emoji", 0, "emoji_path",
                  "emoji directory or file path [~/.mlterm/emoji]");
  bl_conf_add_opt(conf, '\0', "emojifmt", 0, "emoji_file_format",
                  "emoji file format [%.4x.png,%.4x-%.4x.png]");
  bl_conf_add_opt(conf, '\0', "lew", 0, "local_echo_wait",
                  "time (msec) to keep local echo mode [250]");
  bl_conf_add_opt(conf, '\0', "rz", 0, "resize_mode",
                  "screen display at resize [wrap]");
  bl_conf_add_opt(conf, '\0', "recvdir", 0, "receive_directory",
                  "directory to save received files [~/.mlterm/recv]");
  bl_conf_add_opt(conf, '\0', "fk", 1, "format_other_keys",
                  "send modified keys as parameter for CSI u [false]");
  bl_conf_add_opt(conf, '\0', "sdpr", 0, "simple_scrollbar_dpr",
                  "device pixel ratio for simple scrollbar [1]");
  bl_conf_add_opt(conf, '\0', "norepkey", 0, "mod_keys_to_stop_mouse_report",
                  "modifier keys to stop mouse reporting [shift,control]");
  bl_conf_add_opt(conf, '\0', "clp", 1, "use_clipping",
                  "Use clipping to keep from producing dots outside the text drawing area [false]");
  bl_conf_add_opt(conf, '\0', "adc", 1, "use_arabic_dynamic_comb",
                  "Use dynamic combining to show arabic presentation forms [true]");
#ifdef USE_WIN32API
  bl_conf_add_opt(conf, '\0', "winsize", 1, "output_xtwinops_in_resizing",
                  "output xtwinops sequence in resizing [false]");
#ifdef USE_CONPTY
  bl_conf_add_opt(conf, '\0', "conpty", 1, "use_conpty", "use conpty [true]");
#endif
#endif
#ifdef SELECTION_STYLE_CHANGEABLE
  bl_conf_add_opt(conf, '\0', "chsel", 1, "change_selection_immediately",
                  "change selection just after selecting text by mouse [true]");
#endif
#ifdef USE_IM_CURSOR_COLOR
  bl_conf_add_opt(conf, '\0', "imcolor", 0, "im_cursor_color",
                  "cursor color when input method is activated. [false]");
#endif
  bl_conf_set_end_opt(conf, 'e', NULL, "exec_cmd", "execute external command");
}

void ui_main_config_init(ui_main_config_t *main_config, bl_conf_t *conf, int argc, char **argv) {
  char *value;
  char *invalid_msg = "%s=%s is invalid.\n";

  memset(main_config, 0, sizeof(ui_main_config_t));

  if ((value = bl_conf_get_value(conf, "locale"))) {
    bl_locale_init(value);
  }

/*
 * xlib: "-d :0.0" etc.
 * console: "-d client:fd"
 * wayland: "-d wayland-0"
 */
#if defined(USE_XLIB) || defined(USE_CONSOLE) || defined(USE_WAYLAND)
  if (!(value = bl_conf_get_value(conf, "display")) ||
      !(main_config->disp_name = strdup(value)))
#endif
  {
    /*
     * default_display ("") is replaced by getenv("WAYLAND_DISPLAY") on wayland.
     * (See ui_display_open() in wayland/ui_display.c)
     */
    main_config->disp_name = default_display;
#ifdef USE_XLIB
    /* In case of starting mlterm with "-j genuine" option without X server. */
    if (getenv("DISPLAY") == NULL) {
      bl_setenv("DISPLAY", ":0", 0);
    }
#endif
  }

  if ((value = bl_conf_get_value(conf, "fontsize")) == NULL) {
    main_config->font_size = 16;
  } else if (!bl_str_to_uint(&main_config->font_size, value)) {
    bl_msg_printf(invalid_msg, "fontsize", value);

    /* default value is used. */
    main_config->font_size = 16;
  }

  if ((value = bl_conf_get_value(conf, "app_name"))) {
    main_config->app_name = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "title"))) {
    main_config->title = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "use_locked_title"))) {
    if (strcmp(value, "true") == 0) {
      main_config->use_locked_title = 1;
    }
  }

#ifdef USE_XLIB
  if ((value = bl_conf_get_value(conf, "icon_name"))) {
    main_config->icon_name = strdup(value);
  }
  if ((value = bl_conf_get_value(conf, "wm_role"))) {
    main_config->wm_role = strdup(value);
  }
#endif

/* BACKWARD COMPAT (3.1.7 or before) */
#if 1
  if ((value = bl_conf_get_value(conf, "conf_menu_path_1"))) {
    main_config->shortcut_strs[0] = malloc(5 + strlen(value) + 2 + 1);
    sprintf(main_config->shortcut_strs[0], "\"menu:%s\"", value);
  }

  if ((value = bl_conf_get_value(conf, "conf_menu_path_2"))) {
    main_config->shortcut_strs[1] = malloc(5 + strlen(value) + 2 + 1);
    sprintf(main_config->shortcut_strs[1], "\"menu:%s\"", value);
  }

  if ((value = bl_conf_get_value(conf, "conf_menu_path_3"))) {
    main_config->shortcut_strs[2] = malloc(5 + strlen(value) + 2 + 1);
    sprintf(main_config->shortcut_strs[2], "\"menu:%s\"", value);
  }

  if ((value = bl_conf_get_value(conf, "button3_behavior")) &&
      /* menu1,menu2,menu3,xterm values are ignored. */
      strncmp(value, "menu", 4) != 0) {
    main_config->shortcut_strs[3] = malloc(7 + strlen(value) + 2 + 1);
    /* XXX "abc" should be "exesel:\"abc\"" but it's not supported. */
    sprintf(main_config->shortcut_strs[3], "\"exesel:%s\"", value);
  }
#endif

  if ((value = bl_conf_get_value(conf, "scrollbar_view_name"))) {
    main_config->scrollbar_view_name = strdup(value);
  }

  main_config->use_char_combining = 1;

  if ((value = bl_conf_get_value(conf, "use_combining"))) {
    if (strcmp(value, "false") == 0) {
      main_config->use_char_combining = 0;
    }
  }

  if ((value = bl_conf_get_value(conf, "use_dynamic_comb"))) {
    if (strcmp(value, "true") == 0) {
      main_config->use_dynamic_comb = 1;
    }
  }

  if ((value = bl_conf_get_value(conf, "use_arabic_dynamic_comb"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      vt_set_use_arabic_dynamic_comb(flag);
    }
  }

  if ((value = bl_conf_get_value(conf, "logging_vt_seq"))) {
    if (strcmp(value, "true") == 0) {
      main_config->logging_vt_seq = 1;
    }
  }

  if ((value = bl_conf_get_value(conf, "vt_seq_format"))) {
    vt_set_use_ttyrec_format(strcmp(value, "ttyrec") == 0);
  }

  if ((value = bl_conf_get_value(conf, "logging_msg")) && strcmp(value, "false") == 0) {
    bl_set_msg_log_file_name(NULL);
  } else {
    bl_set_msg_log_file_name("mlterm/msg.log");
  }

  main_config->step_in_changing_font_size = 1;

  if ((value = bl_conf_get_value(conf, "step_in_changing_font_size"))) {
    u_int size;

    if (bl_str_to_uint(&size, value)) {
      main_config->step_in_changing_font_size = size;
    } else {
      bl_msg_printf(invalid_msg, "step_in_changing_font_size", value);
    }
  }

#ifdef USE_XLIB
#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
  main_config->type_engine = TYPE_CAIRO;
#elif !defined(USE_TYPE_XCORE) && defined(USE_TYPE_XFT)
  main_config->type_engine = TYPE_XFT;
#else
  main_config->type_engine = TYPE_XCORE;
#endif

  if ((value = bl_conf_get_value(conf, "type_engine"))) {
    main_config->type_engine = ui_get_type_engine_by_name(value);
  }
#else
  main_config->type_engine = TYPE_XCORE;
#endif

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO) || \
  defined(USE_FREETYPE) || defined(USE_WIN32GUI)
  if ((value = bl_conf_get_value(conf, "use_anti_alias"))) {
    if (strcmp(value, "true") == 0) {
      main_config->font_present |= FONT_AA;
      if (main_config->type_engine == TYPE_XCORE) {
/* forcibly use xft or cairo */
#if defined(USE_TYPE_XFT) && !defined(USE_TYPE_CAIRO)
        main_config->type_engine = TYPE_XFT;
#else
        main_config->type_engine = TYPE_CAIRO;
#endif
      }
    } else if (strcmp(value, "false") == 0) {
      main_config->font_present |= FONT_NOAA;
    }
  }
#endif

  if ((value = bl_conf_get_value(conf, "use_variable_column_width"))) {
    if (strcmp(value, "true") == 0) {
      main_config->font_present |= FONT_VAR_WIDTH;
    }
  }

  if ((value = bl_conf_get_value(conf, "vertical_mode"))) {
    if ((main_config->vertical_mode = vt_get_vertical_mode_by_name(value))) {
      main_config->font_present |= main_config->vertical_mode;

      /* See change_font_present() in ui_screen.c */
      if (main_config->font_present & FONT_VAR_WIDTH) {
        bl_msg_printf("Set use_variable_column_width=false forcibly.\n");
        main_config->font_present &= ~FONT_VAR_WIDTH;
      }
    }
  }

  if ((value = bl_conf_get_value(conf, "fg_color"))) {
    main_config->fg_color = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "bg_color"))) {
    main_config->bg_color = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "cursor_fg_color"))) {
    main_config->cursor_fg_color = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "cursor_bg_color"))) {
    main_config->cursor_bg_color = strdup(value);
  }

  main_config->alt_color_mode = 0;

  if ((value = bl_conf_get_value(conf, "bd_color"))) {
    main_config->bd_color = strdup(value);
    main_config->alt_color_mode |= ALT_COLOR_BOLD;
  }

  if ((value = bl_conf_get_value(conf, "ul_color"))) {
    main_config->ul_color = strdup(value);
    main_config->alt_color_mode |= ALT_COLOR_UNDERLINE;
  }

  if ((value = bl_conf_get_value(conf, "bl_color"))) {
    main_config->bl_color = strdup(value);
    main_config->alt_color_mode |= ALT_COLOR_BLINKING;
  }

  if ((value = bl_conf_get_value(conf, "rv_color"))) {
    main_config->rv_color = strdup(value);
    main_config->alt_color_mode |= ALT_COLOR_REVERSE;
  }

  if ((value = bl_conf_get_value(conf, "it_color"))) {
    main_config->it_color = strdup(value);
    main_config->alt_color_mode |= ALT_COLOR_ITALIC;
  }

  if ((value = bl_conf_get_value(conf, "co_color"))) {
    main_config->co_color = strdup(value);
    main_config->alt_color_mode |= ALT_COLOR_CROSSED_OUT;
  }

  if ((value = bl_conf_get_value(conf, "sb_fg_color"))) {
    main_config->sb_fg_color = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "sb_bg_color"))) {
    main_config->sb_bg_color = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "termtype"))) {
    main_config->term_type = strdup(value);
  } else {
    main_config->term_type = strdup("xterm");
  }

#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
  /*
   * The pty is always resized to fit the display size.
   * Don't use 80x24 for the default value because the screen is not drawn
   * correctly on startup if the display size / the us-ascii font size is
   * 80x24 by chance.
   */
  main_config->cols = 1;
  main_config->rows = 1;
#else
  main_config->cols = 80;
  main_config->rows = 24;

  if ((value = bl_conf_get_value(conf, "geometry"))) {
    /*
     * For each value not found, the argument is left unchanged.
     * (see man XParseGeometry(3))
     */
    main_config->geom_hint = XParseGeometry(value, &main_config->x, &main_config->y,
                                            &main_config->cols, &main_config->rows);

    if (main_config->cols == 0 || main_config->rows == 0) {
      bl_msg_printf("geometry option %s is illegal.\n", value);

      main_config->cols = 80;
      main_config->rows = 24;
    }
  }
#endif

  main_config->screen_width_ratio = 100;

#if 1 /* BACKWARD COMPAT (3.8.2 or before) */
  if ((value = bl_conf_get_value(conf, "screen_height_ratio"))) {
    u_int ratio;

    if (bl_str_to_uint(&ratio, value) && ratio) {
      main_config->screen_width_ratio = ratio;
    }
  }
#endif

  if ((value = bl_conf_get_value(conf, "screen_width_ratio"))) {
    u_int ratio;

    if (bl_str_to_uint(&ratio, value) && ratio) {
      main_config->screen_width_ratio = ratio;
    } else {
      bl_msg_printf(invalid_msg, "screen_width_ratio", value);
    }
  }

  main_config->use_multi_col_char = 1;

  if ((value = bl_conf_get_value(conf, "use_multi_column_char"))) {
    if (strcmp(value, "false") == 0) {
      main_config->use_multi_col_char = 0;
    }
  }

  if ((value = bl_conf_get_value(conf, "line_space"))) {
    int size;

    if (bl_str_to_int(&size, value)) {
      main_config->line_space = size;
    } else {
      bl_msg_printf(invalid_msg, "line_space", value);
    }
  }

  if ((value = bl_conf_get_value(conf, "letter_space"))) {
    int size;

    if (bl_str_to_int(&size, value)) {
      main_config->letter_space = size;
    } else {
      bl_msg_printf(invalid_msg, "letter_space", value);
    }
  }

  main_config->num_log_lines = VT_LOG_SIZE_UNIT;
  main_config->unlimit_log_size = 0;

  if ((value = bl_conf_get_value(conf, "logsize"))) {
    u_int size;

    if (strcmp(value, "unlimited") == 0) {
      main_config->unlimit_log_size = 1;
    } else if (bl_str_to_uint(&size, value)) {
      main_config->num_log_lines = size;
    } else {
      bl_msg_printf(invalid_msg, "logsize", value);
    }
  }

  main_config->tab_size = 8;

  if ((value = bl_conf_get_value(conf, "tabsize"))) {
    u_int size;

    if (bl_str_to_uint(&size, value)) {
      main_config->tab_size = size;
    } else {
      bl_msg_printf(invalid_msg, "tabsize", value);
    }
  }

#ifdef USE_BEOS
  main_config->use_login_shell = 1;
#else
  main_config->use_login_shell = 0;
#endif

  if ((value = bl_conf_get_value(conf, "use_login_shell"))) {
#ifdef USE_BEOS
    if (strcmp(value, "false") == 0) {
      main_config->use_login_shell = 0;
    }
#else
    if (strcmp(value, "true") == 0) {
      main_config->use_login_shell = 1;
    }
#endif
  }

  if ((value = bl_conf_get_value(conf, "big5_buggy"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      ui_set_big5_selection_buggy(flag);
    }
  }

  main_config->use_mdi = 1;

#if !defined(USE_QUARTZ) && !defined(USE_BEOS) /* see ui_layout.[ch] */
  if ((value = bl_conf_get_value(conf, "use_mdi"))) {
    if (strcmp(value, "false") == 0) {
      main_config->use_mdi = 0;
    }
  }
#endif

#if defined(USE_CONSOLE) || defined(COCOA_TOUCH)
  main_config->sb_mode = SBM_NONE;
#else
  if ((value = bl_conf_get_value(conf, "scrollbar_mode"))) {
    main_config->sb_mode = ui_get_sb_mode_by_name(value);
  } else {
/* XXX Backward compatibility with 3.4.5 or before */
#if 1
    if ((value = bl_conf_get_value(conf, "use_scrollbar"))) {
      main_config->sb_mode = (strcmp(value, "true") == 0 ? SBM_LEFT : SBM_NONE);
    } else
#endif
    {
      main_config->sb_mode = SBM_LEFT;
    }
  }
#endif

  if ((value = bl_conf_get_value(conf, "iso88591_font_for_usascii"))) {
    if (strcmp(value, "true") == 0) {
      main_config->iso88591_font_for_usascii = 1;
    }
  }

  if ((value = bl_conf_get_value(conf, "not_use_unicode_font"))) {
    if (strcmp(value, "true") == 0) {
      main_config->unicode_policy = NOT_USE_UNICODE_FONT;
    }
  }

  if ((value = bl_conf_get_value(conf, "unicode_noconv_areas"))) {
    vt_set_unicode_noconv_areas(value);
  }

  if ((value = bl_conf_get_value(conf, "unicode_full_width_areas"))) {
    vt_set_full_width_areas(value);
  }

  if ((value = bl_conf_get_value(conf, "unicode_half_width_areas"))) {
    vt_set_half_width_areas(value);
  }

  if ((value = bl_conf_get_value(conf, "only_use_unicode_font"))) {
    if (strcmp(value, "true") == 0) {
      if (main_config->unicode_policy == NOT_USE_UNICODE_FONT) {
        bl_msg_printf(
            "only_use_unicode_font and not_use_unicode_font options "
            "cannot be used at the same time.\n");

        /* default values are used */
        main_config->unicode_policy = 0;
      } else {
        main_config->unicode_policy = ONLY_USE_UNICODE_FONT;
      }
    }
  }
#ifdef FORCE_UNICODE
  else {
    main_config->unicode_policy = ONLY_USE_UNICODE_FONT;
  }
#endif

  if ((value = bl_conf_get_value(conf, "box_drawing_font"))) {
    if (strcmp(value, "decsp") == 0) {
      main_config->unicode_policy |= NOT_USE_UNICODE_BOXDRAW_FONT;
    } else if (strcmp(value, "unicode") == 0) {
      main_config->unicode_policy |= ONLY_USE_UNICODE_BOXDRAW_FONT;
    }
  }
#ifdef FORCE_UNICODE
  else {
    main_config->unicode_policy |= ONLY_USE_UNICODE_BOXDRAW_FONT;
  }
#endif

  if ((value = bl_conf_get_value(conf, "receive_string_via_ucs"))) {
    if (strcmp(value, "true") == 0) {
      main_config->receive_string_via_ucs = 1;
    }
  }

  /* "zh" and "ko" ? */
  if (strcmp(bl_get_lang(), "ja") == 0
#ifdef USE_WIN32API
      || strcmp(bl_get_lang(), "Japanese") == 0
#endif
     ) {
    main_config->col_size_of_width_a = 2;
  } else {
    main_config->col_size_of_width_a = 1;
  }

  if ((value = bl_conf_get_value(conf, "col_size_of_width_a"))) {
    u_int col_size_of_width_a;

    if (bl_str_to_uint(&col_size_of_width_a, value)) {
      main_config->col_size_of_width_a = col_size_of_width_a;
    } else {
      bl_msg_printf(invalid_msg, "col_size_of_width_a", value);
    }
  }

  if ((value = bl_conf_get_value(conf, "wall_picture"))) {
    if (*value != '\0') {
      main_config->pic_file_path = strdup(value);
    }
  }

  if ((value = bl_conf_get_value(conf, "use_transbg"))) {
    if (strcmp(value, "true") == 0) {
      main_config->use_transbg = 1;
    }
  }

  if (main_config->pic_file_path && main_config->use_transbg) {
    bl_msg_printf(
        "wall picture and transparent background cannot be used at the same "
        "time.\n");

    /* using wall picture */
    main_config->use_transbg = 0;
  }

  main_config->brightness = 100;

  if ((value = bl_conf_get_value(conf, "brightness"))) {
    u_int brightness;

    if (bl_str_to_uint(&brightness, value)) {
      main_config->brightness = brightness;
    } else {
      bl_msg_printf(invalid_msg, "brightness", value);
    }
  }

  main_config->contrast = 100;

  if ((value = bl_conf_get_value(conf, "contrast"))) {
    u_int contrast;

    if (bl_str_to_uint(&contrast, value)) {
      main_config->contrast = contrast;
    } else {
      bl_msg_printf(invalid_msg, "contrast", value);
    }
  }

  main_config->gamma = 100;

  if ((value = bl_conf_get_value(conf, "gamma"))) {
    u_int gamma;

    if (bl_str_to_uint(&gamma, value)) {
      main_config->gamma = gamma;
    } else {
      bl_msg_printf(invalid_msg, "gamma", value);
    }
  }

  main_config->alpha = 255;

  if ((value = bl_conf_get_value(conf, "alpha"))) {
    u_int alpha;

    if (bl_str_to_uint(&alpha, value)) {
      main_config->alpha = alpha;
    } else {
      bl_msg_printf(invalid_msg, "alpha", value);
    }
  }

  main_config->fade_ratio = 100;

  if ((value = bl_conf_get_value(conf, "fade_ratio"))) {
    u_int fade_ratio;

    if (bl_str_to_uint(&fade_ratio, value) && fade_ratio <= 100) {
      main_config->fade_ratio = fade_ratio;
    } else {
      bl_msg_printf(invalid_msg, "fade_ratio", value);
    }
  }

  main_config->encoding =
      get_encoding(bl_conf_get_value(conf, "encoding"), &main_config->is_auto_encoding);

#ifdef USE_CONSOLE
  ui_display_set_char_encoding(NULL,
                               get_encoding(bl_conf_get_value(conf, "console_encoding"), NULL));

  if ((value = bl_conf_get_value(conf, "console_sixel_colors"))) {
    ui_display_set_sixel_colors(NULL, value);
  }

  if ((value = bl_conf_get_value(conf, "cell_size"))) {
    u_int width;
    u_int height;

    if (sscanf(value, "%d,%d", &width, &height) == 2) {
      ui_display_set_default_cell_size(width, height);
    }
  }
#endif

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI) || defined(USE_IND) || \
    defined(USE_OT_LAYOUT)
  main_config->use_ctl = 1;

  if ((value = bl_conf_get_value(conf, "use_ctl"))) {
    if (strcmp(value, "false") == 0) {
      /*
       * If use_ot_layout is true, use_ctl = true forcibly.
       * See processing "use_ot_layout" option.
       */
      main_config->use_ctl = 0;
    }
  }
#endif

  main_config->bidi_mode = BIDI_NORMAL_MODE;

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)
  if ((value = bl_conf_get_value(conf, "bidi_mode"))) {
#if 1
    /* Compat with 3.3.6 or before. */
    if (strcmp(value, "cmd_l") == 0) {
      main_config->bidi_mode = BIDI_ALWAYS_LEFT;
      main_config->bidi_separators = strdup(" ");
    } else if (strcmp(value, "cmd_r") == 0) {
      main_config->bidi_mode = BIDI_ALWAYS_RIGHT;
      main_config->bidi_separators = strdup(" ");
    } else
#endif
    {
      main_config->bidi_mode = vt_get_bidi_mode_by_name(value);
    }
  }

  if ((value = bl_conf_get_value(conf, "bidi_separators"))) {
    free(main_config->bidi_separators);
    main_config->bidi_separators = strdup(value);
  }
#endif

  /* If value is "none" or not is also checked in ui_screen.c */
  if ((value = bl_conf_get_value(conf, "mod_meta_key")) && strcmp(value, "none") != 0) {
    main_config->mod_meta_key = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "mod_meta_mode"))) {
    main_config->mod_meta_mode = ui_get_mod_meta_mode_by_name(value);
  } else {
    main_config->mod_meta_mode = MOD_META_SET_MSB;
  }

  if ((value = bl_conf_get_value(conf, "bel_mode"))) {
    main_config->bel_mode = ui_get_bel_mode_by_name(value);
  } else {
    main_config->bel_mode = BEL_SOUND;
  }

  if ((value = bl_conf_get_value(conf, "use_urgent_bell"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      ui_set_use_urgent_bell(flag);
    }
  }

  main_config->use_vertical_cursor = 1;
  if ((value = bl_conf_get_value(conf, "use_vertical_cursor"))) {
    if (strcmp(value, "false") == 0) {
      main_config->use_vertical_cursor = 0;
    }
  }

#ifdef USE_XLIB
  if ((value = bl_conf_get_value(conf, "borderless"))) {
    if (strcmp(value, "true") == 0) {
      main_config->borderless = 1;
    }
  }
#endif

  main_config->bs_mode = BSM_DEFAULT;

  if ((value = bl_conf_get_value(conf, "static_backscroll_mode"))) {
    if (strcmp(value, "true") == 0) {
      main_config->bs_mode = BSM_STATIC;
    }
  }

  if ((value = bl_conf_get_value(conf, "exit_backscroll_by_pty"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      ui_exit_backscroll_by_pty(flag);
    }
  }

  if ((value = bl_conf_get_value(conf, "allow_change_shortcut"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      ui_allow_change_shortcut(flag);
    }
  }

  if ((value = bl_conf_get_value(conf, "icon_path"))) {
    main_config->icon_path = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "input_method"))) {
    main_config->input_method = strdup(value);
  } else {
    main_config->input_method = strdup("default");
  }

  if ((value = bl_conf_get_value(conf, "init_str"))) {
    if ((main_config->init_str = malloc(strlen(value) + 1))) {
      char *p1;
      char *p2;

      p1 = value;
      p2 = main_config->init_str;

      while (*p1) {
        if (*p1 == '\\') {
          p1++;

          if (*p1 == '\0') {
            break;
          } else if (*p1 == 'n') {
            *(p2++) = '\n';
          } else if (*p1 == 'r') {
            *(p2++) = '\r';
          } else if (*p1 == 't') {
            *(p2++) = '\t';
          } else if (*p1 == 'e') {
            *(p2++) = '\033';
          } else {
            *(p2++) = *p1;
          }
        } else {
          *(p2++) = *p1;
        }

        p1++;
      }

      *p2 = '\0';
    }
  }

  if ((value = bl_conf_get_value(conf, "parent_window"))) {
    u_long parent_window;
    char *end;

    parent_window = strtol(value, &end, 0);
    if (parent_window != 0 && parent_window != LONG_MIN && parent_window != LONG_MAX) {
      main_config->parent_window = parent_window;
    } else {
      bl_msg_printf(invalid_msg, "parent_window", value);
    }
  }

#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
  if ((value = bl_conf_get_value(conf, "default_server")) && *value) {
    main_config->default_server = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "always_show_dialog"))) {
    if (strcmp(value, "true") == 0) {
      main_config->show_dialog = 1;
    }
  }
#endif

#ifdef USE_LIBSSH2
  if ((value = bl_conf_get_value(conf, "ssh_public_key"))) {
    main_config->public_key = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "ssh_private_key"))) {
    main_config->private_key = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "cipher_list"))) {
    vt_pty_ssh_set_cipher_list(strdup(value));
  }

  if ((value = bl_conf_get_value(conf, "ssh_x11_forwarding"))) {
    if (strcmp(value, "true") == 0) {
      main_config->use_x11_forwarding = 1;
    }
  }

  if ((value = bl_conf_get_value(conf, "allow_scp"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      vt_set_use_scp_full(flag);
    }
  }

  if ((value = bl_conf_get_value(conf, "ssh_auto_reconnect"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      vt_pty_ssh_set_use_auto_reconnect(flag);
    }
  }
#endif

  if ((value = bl_conf_get_value(conf, "allow_osc52"))) {
    if (strcmp(value, "true") == 0) {
      main_config->allow_osc52 = 1;
    }
  }

  if ((value = bl_conf_get_value(conf, "blink_cursor"))) {
    if (strcmp(value, "true") == 0) {
      main_config->blink_cursor = 1;
    }
  }

  main_config->hmargin = main_config->vmargin = 2;

  if ((value = bl_conf_get_value(conf, "inner_border"))) {
    u_int hmargin;
    u_int vmargin;

    /* 640x480 => (640-224*2)x(480-224*2) => 192x32 on framebuffer. */
    if (sscanf(value, "%d,%d", &hmargin, &vmargin) == 2) {
      if (hmargin <= 224 && vmargin <= 224) {
        main_config->hmargin = hmargin;
        main_config->vmargin = vmargin;
      }
    } else if (bl_str_to_uint(&hmargin, value) && hmargin <= 224) {
      main_config->hmargin = main_config->vmargin = hmargin;
    }
  }

  main_config->layout_hmargin = main_config->layout_vmargin = 0;

  if ((value = bl_conf_get_value(conf, "layout_inner_border"))) {
    u_int hmargin;
    u_int vmargin;

    /* 640x480 => (640-224*2)x(480-224*2) => 192x32 on framebuffer. */
    if (sscanf(value, "%d,%d", &hmargin, &vmargin) == 2) {
      if (hmargin <= 224 && vmargin <= 224) {
        main_config->layout_hmargin = hmargin;
        main_config->layout_vmargin = vmargin;
      }
    } else if (bl_str_to_uint(&hmargin, value) && hmargin <= 224) {
      main_config->layout_hmargin = main_config->layout_vmargin = hmargin;
    }
  }

  main_config->use_bold_font = 1;

  if ((value = bl_conf_get_value(conf, "use_bold_font"))) {
    if (strcmp(value, "false") == 0) {
      main_config->use_bold_font = 0;
    }
  }

  main_config->use_italic_font = 1;

  if ((value = bl_conf_get_value(conf, "use_italic_font"))) {
    if (strcmp(value, "false") == 0) {
      main_config->use_italic_font = 0;
    }
  }

  if ((value = bl_conf_get_value(conf, "hide_underline"))) {
    if (strcmp(value, "true") == 0) {
      main_config->hide_underline = 1;
    }
  }

  if ((value = bl_conf_get_value(conf, "underline_offset"))) {
    int size;

    if (bl_str_to_int(&size, value)) {
      main_config->underline_offset = size;
    } else {
      bl_msg_printf(invalid_msg, "underline_offset", value);
    }
  }

  if ((value = bl_conf_get_value(conf, "baseline_offset"))) {
    int size;

    if (bl_str_to_int(&size, value)) {
      main_config->baseline_offset = size;
    } else {
      bl_msg_printf(invalid_msg, "baseline_offset", value);
    }
  }

  if ((value = bl_conf_get_value(conf, "word_separators"))) {
    vt_set_word_separators(value);
  }

  if ((value = bl_conf_get_value(conf, "regard_uri_as_word"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      vt_set_regard_uri_as_word(flag);
    }
  }

#ifdef SELECTION_STYLE_CHANGEABLE
  if ((value = bl_conf_get_value(conf, "change_selection_immediately"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      ui_set_change_selection_immediately(flag);
    }
  }
#endif

#ifndef __ANDROID__
  if (!(value = bl_conf_get_value(conf, "auto_restart")) || strcmp(value, "false") != 0) {
    vt_set_auto_restart_cmd(bl_get_prog_path());
  }
#endif

  if ((value = bl_conf_get_value(conf, "use_local_echo"))) {
    if (strcmp(value, "true") == 0) {
      main_config->use_local_echo = 1;
    }
  }

  if ((value = bl_conf_get_value(conf, "local_echo_wait"))) {
    u_int msec;
    if (bl_str_to_uint(&msec, value)) {
      vt_set_local_echo_wait(msec);
    }
  }

  if ((value = bl_conf_get_value(conf, "click_interval"))) {
    int interval;

    if (bl_str_to_int(&interval, value)) {
      ui_set_click_interval(interval);
    } else {
      bl_msg_printf(invalid_msg, "click_interval", value);
    }
  }

  if ((value = bl_conf_get_value(conf, "use_alt_buffer"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      vt_set_use_alt_buffer(flag);
    }
  }

  main_config->use_ansi_colors = 1;

  if ((value = bl_conf_get_value(conf, "use_ansi_colors"))) {
    if (strcmp(value, "false") == 0) {
      main_config->use_ansi_colors = 0;
    }

    ui_display_set_use_ansi_colors(main_config->use_ansi_colors);
  }

  if ((value = bl_conf_get_value(conf, "auto_detect_encodings"))) {
    vt_set_auto_detect_encodings(value);
  }

  if ((value = bl_conf_get_value(conf, "use_auto_detect"))) {
    if (strcmp(value, "true") == 0) {
      main_config->use_auto_detect = 1;
    }
  }

  if ((value = bl_conf_get_value(conf, "leftward_double_drawing"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      ui_set_use_leftward_double_drawing(flag);
    }
  }

  if ((value = bl_conf_get_value(conf, "working_directory"))) {
    main_config->work_dir = strdup(value);
  }

  if ((value = bl_conf_get_value(conf, "vt_color_mode"))) {
    vt_set_color_mode(value);
  }

  if ((value = bl_conf_get_value(conf, "primary_da"))) {
    vt_set_primary_da(value);
  }

  if ((value = bl_conf_get_value(conf, "secondary_da"))) {
    vt_set_secondary_da(value);
  }

  if ((value = bl_conf_get_value(conf, "mod_meta_prefix"))) {
    ui_set_mod_meta_prefix(bl_str_unescape(value));
  }

#ifdef USE_OT_LAYOUT
  main_config->use_ot_layout = 0;

  if ((value = bl_conf_get_value(conf, "use_ot_layout"))) {
    if (strcmp(value, "true") == 0) {
      main_config->use_ot_layout = 1;
      if (!main_config->use_ctl) {
        bl_msg_printf("Set use_ctl=true forcibly.");
        main_config->use_ctl = 1;
      }
    }
  }

  if ((value = bl_conf_get_value(conf, "ot_script"))) {
    vt_set_ot_layout_attr(value, OT_SCRIPT);
  }

  if ((value = bl_conf_get_value(conf, "ot_features"))) {
    vt_set_ot_layout_attr(value, OT_FEATURES);
  }
#endif

#ifdef USE_GRF
  if ((value = bl_conf_get_value(conf, "separate_wall_picture"))) {
    extern int separate_wall_picture;
    int flag = true_or_false(value);

    if (flag != -1) {
      separate_wall_picture = flag;
    }
  }
#endif

#ifdef ROTATABLE_DISPLAY
  if ((value = bl_conf_get_value(conf, "rotate_display"))) {
    ui_display_rotate(strcmp(value, "right") == 0 ? 1 : (strcmp(value, "left") == 0 ? -1 : 0));
  }
#endif

#if (defined(__ANDROID__) && defined(USE_LIBSSH2)) || defined(USE_WIN32API)
  if ((value = bl_conf_get_value(conf, "start_with_local_pty"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      start_with_local_pty = flag;
    }
  }
#endif

  if ((value = bl_conf_get_value(conf, "trim_trailing_newline_in_pasting"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      ui_set_trim_trailing_newline_in_pasting(flag);
    }
  }

  if ((value = bl_conf_get_value(conf, "broadcast"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      vt_set_broadcasting(flag);
    }
  }

  if ((value = bl_conf_get_value(conf, "ignore_broadcasted_chars"))) {
    if (strcmp(value, "true") == 0) {
      main_config->ignore_broadcasted_chars = 1;
    }
  }

  if ((value = bl_conf_get_value(conf, "emoji_path"))) {
    ui_emoji_set_path(value);
  }

  if ((value = bl_conf_get_value(conf, "emoji_file_format"))) {
    ui_emoji_set_file_format(value);
  }

  if ((value = bl_conf_get_value(conf, "resize_mode"))) {
    vt_set_resize_mode(vt_get_resize_mode_by_name(value));
  }

  if ((value = bl_conf_get_value(conf, "format_other_keys"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      vt_set_format_other_keys(flag);
    }
  }

  if ((value = bl_conf_get_value(conf, "receive_directory"))) {
    vt_set_recv_dir(value);
  }

  if ((value = bl_conf_get_value(conf, "simple_scrollbar_dpr"))) {
    int dpr;

    if (bl_str_to_int(&dpr, value) && dpr > 0) {
      ui_sb_view_set_dpr(dpr);
    }
  }

  if ((value = bl_conf_get_value(conf, "mod_keys_to_stop_mouse_report"))) {
    ui_set_mod_keys_to_stop_mouse_report(value);
  }

  if ((value = bl_conf_get_value(conf, "use_clipping"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      ui_set_use_clipping(flag);
    }
  }

#ifdef USE_WIN32API
  if ((value = bl_conf_get_value(conf, "output_xtwinops_in_resizing"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      vt_pty_win32_set_options(-1, flag);
    }
  }
#ifdef USE_CONPTY
  if ((value = bl_conf_get_value(conf, "use_conpty"))) {
    int flag = true_or_false(value);

    if (flag != -1) {
      vt_pty_win32_set_options(flag, -1);
    }
  }
#endif
#endif

#ifdef USE_IM_CURSOR_COLOR
  if ((value = bl_conf_get_value(conf, "im_cursor_color"))) {
    if (*value) {
      ui_set_im_cursor_color(value);
    }
  }
#endif

  if ((value = bl_conf_get_value(conf, "exec_cmd"))) {
    if (strcmp(value, "true") == 0) {
      if ((main_config->cmd_argv = malloc(sizeof(char *) * (argc + 1)))) {
        /*
         * !! Notice !!
         * cmd_path and strings in cmd_argv vector should be allocated
         * by the caller.
         */

        main_config->cmd_path = argv[0];

        memcpy(&main_config->cmd_argv[0], argv, sizeof(char *) * argc);
        main_config->cmd_argv[argc] = NULL;
      }
    } else {
      u_int argc;

      argc = bl_count_char_in_str(value, ' ') + 1;

      if ((main_config->cmd_argv = malloc(sizeof(char *) * (argc + 1) + strlen(value) + 1))) {
        value = strcpy((char*)(main_config->cmd_argv + argc + 1), value);
        bl_arg_str_to_array(main_config->cmd_argv, &argc, value);
        main_config->cmd_path = main_config->cmd_argv[0];
      }
    }
  }
}

void ui_main_config_final(ui_main_config_t *main_config) {
  if (main_config->disp_name != default_display) {
    free(main_config->disp_name);
  }
  free(main_config->app_name);
  free(main_config->title);
  free(main_config->icon_name);
  free(main_config->wm_role);
  free(main_config->term_type);
  free(main_config->scrollbar_view_name);
  free(main_config->pic_file_path);
  free(main_config->shortcut_strs[0]);
  free(main_config->shortcut_strs[1]);
  free(main_config->shortcut_strs[2]);
  free(main_config->shortcut_strs[3]);
  free(main_config->fg_color);
  free(main_config->bg_color);
  free(main_config->cursor_fg_color);
  free(main_config->cursor_bg_color);
  free(main_config->bd_color);
  free(main_config->it_color);
  free(main_config->ul_color);
  free(main_config->bl_color);
  free(main_config->co_color);
  free(main_config->sb_fg_color);
  free(main_config->sb_bg_color);
  free(main_config->mod_meta_key);
  free(main_config->icon_path);
  free(main_config->input_method);
  free(main_config->init_str);
  free(main_config->bidi_separators);

#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
  free(main_config->default_server);
#endif

#ifdef USE_LIBSSH2
  free(main_config->public_key);
  free(main_config->private_key);
#endif

  free(main_config->work_dir);
  free(main_config->cmd_argv);
}
