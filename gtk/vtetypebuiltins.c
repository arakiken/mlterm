/* Generated data (by glib-mkenums) */

#include <vte/vte.h>

#ifndef VTE_CHECK_VERSION
#define VTE_CHECK_VERSION(a, b, c) (0)
#endif

#if VTE_CHECK_VERSION(0, 38, 0)

/*
 * Following is based on vtetypebuiltins.h of vte-0.39.1
 */

/* enumerations from "vteenums.h" */
GType vte_cursor_blink_mode_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
        {VTE_CURSOR_BLINK_SYSTEM, "VTE_CURSOR_BLINK_SYSTEM", "system"},
        {VTE_CURSOR_BLINK_ON, "VTE_CURSOR_BLINK_ON", "on"},
        {VTE_CURSOR_BLINK_OFF, "VTE_CURSOR_BLINK_OFF", "off"},
        {0, NULL, NULL}};
    GType g_define_type_id =
        g_enum_register_static(g_intern_static_string("VteCursorBlinkMode"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

GType vte_cursor_shape_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
        {VTE_CURSOR_SHAPE_BLOCK, "VTE_CURSOR_SHAPE_BLOCK", "block"},
        {VTE_CURSOR_SHAPE_IBEAM, "VTE_CURSOR_SHAPE_IBEAM", "ibeam"},
        {VTE_CURSOR_SHAPE_UNDERLINE, "VTE_CURSOR_SHAPE_UNDERLINE", "underline"},
        {0, NULL, NULL}};
    GType g_define_type_id =
        g_enum_register_static(g_intern_static_string("VteCursorShape"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

GType vte_erase_binding_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
        {VTE_ERASE_AUTO, "VTE_ERASE_AUTO", "auto"},
        {VTE_ERASE_ASCII_BACKSPACE, "VTE_ERASE_ASCII_BACKSPACE", "ascii-backspace"},
        {VTE_ERASE_ASCII_DELETE, "VTE_ERASE_ASCII_DELETE", "ascii-delete"},
        {VTE_ERASE_DELETE_SEQUENCE, "VTE_ERASE_DELETE_SEQUENCE", "delete-sequence"},
        {VTE_ERASE_TTY, "VTE_ERASE_TTY", "tty"},
        {0, NULL, NULL}};
    GType g_define_type_id =
        g_enum_register_static(g_intern_static_string("VteEraseBinding"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

GType vte_pty_error_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
        {VTE_PTY_ERROR_PTY_HELPER_FAILED, "VTE_PTY_ERROR_PTY_HELPER_FAILED", "pty-helper-failed"},
        {VTE_PTY_ERROR_PTY98_FAILED, "VTE_PTY_ERROR_PTY98_FAILED", "pty98-failed"},
        {0, NULL, NULL}};
    GType g_define_type_id = g_enum_register_static(g_intern_static_string("VtePtyError"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

GType vte_pty_flags_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GFlagsValue values[] = {
        {VTE_PTY_NO_LASTLOG, "VTE_PTY_NO_LASTLOG", "no-lastlog"},
        {VTE_PTY_NO_UTMP, "VTE_PTY_NO_UTMP", "no-utmp"},
        {VTE_PTY_NO_WTMP, "VTE_PTY_NO_WTMP", "no-wtmp"},
        {VTE_PTY_NO_HELPER, "VTE_PTY_NO_HELPER", "no-helper"},
        {VTE_PTY_NO_FALLBACK, "VTE_PTY_NO_FALLBACK", "no-fallback"},
        {VTE_PTY_DEFAULT, "VTE_PTY_DEFAULT", "default"},
        {0, NULL, NULL}};
    GType g_define_type_id = g_flags_register_static(g_intern_static_string("VtePtyFlags"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

GType vte_write_flags_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {{VTE_WRITE_DEFAULT, "VTE_WRITE_DEFAULT", "default"},
                                        {0, NULL, NULL}};
    GType g_define_type_id =
        g_enum_register_static(g_intern_static_string("VteWriteFlags"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

#else /* VTE_CHECK_VERSION(0,38,0) */

/*
 * Following is based on vtetypebuiltins.h of vte-0.24.0.
 */

/* enumerations from "vte.h" */
GType vte_terminal_erase_binding_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
      {VTE_ERASE_AUTO, "VTE_ERASE_AUTO", "auto"},
      {VTE_ERASE_ASCII_BACKSPACE, "VTE_ERASE_ASCII_BACKSPACE", "ascii-backspace"},
      {VTE_ERASE_ASCII_DELETE, "VTE_ERASE_ASCII_DELETE", "ascii-delete"},
      {VTE_ERASE_DELETE_SEQUENCE, "VTE_ERASE_DELETE_SEQUENCE", "delete-sequence"},
#if VTE_CHECK_VERSION(0, 20, 4)
      {VTE_ERASE_TTY, "VTE_ERASE_TTY", "tty"},
#endif
      {0, NULL, NULL}
    };
    GType g_define_type_id =
        g_enum_register_static(g_intern_static_string("VteTerminalEraseBinding"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

#if VTE_CHECK_VERSION(0, 17, 1)
GType vte_terminal_cursor_blink_mode_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
        {VTE_CURSOR_BLINK_SYSTEM, "VTE_CURSOR_BLINK_SYSTEM", "system"},
        {VTE_CURSOR_BLINK_ON, "VTE_CURSOR_BLINK_ON", "on"},
        {VTE_CURSOR_BLINK_OFF, "VTE_CURSOR_BLINK_OFF", "off"},
        {0, NULL, NULL}};
    GType g_define_type_id =
        g_enum_register_static(g_intern_static_string("VteTerminalCursorBlinkMode"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}
#endif

#if VTE_CHECK_VERSION(0, 20, 0)
GType vte_terminal_cursor_shape_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
        {VTE_CURSOR_SHAPE_BLOCK, "VTE_CURSOR_SHAPE_BLOCK", "block"},
        {VTE_CURSOR_SHAPE_IBEAM, "VTE_CURSOR_SHAPE_IBEAM", "ibeam"},
        {VTE_CURSOR_SHAPE_UNDERLINE, "VTE_CURSOR_SHAPE_UNDERLINE", "underline"},
        {0, NULL, NULL}};
    GType g_define_type_id =
        g_enum_register_static(g_intern_static_string("VteTerminalCursorShape"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}
#endif

#if VTE_CHECK_VERSION(0, 23, 4)
GType vte_terminal_write_flags_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
        {VTE_TERMINAL_WRITE_DEFAULT, "VTE_TERMINAL_WRITE_DEFAULT", "default"}, {0, NULL, NULL}};
    GType g_define_type_id =
        g_enum_register_static(g_intern_static_string("VteTerminalWriteFlags"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}
#endif

#if VTE_CHECK_VERSION(0, 26, 0)
GType vte_pty_flags_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GFlagsValue values[] = {
        {VTE_PTY_NO_LASTLOG, "VTE_PTY_NO_LASTLOG", "no-lastlog"},
        {VTE_PTY_NO_UTMP, "VTE_PTY_NO_UTMP", "no-utmp"},
        {VTE_PTY_NO_WTMP, "VTE_PTY_NO_WTMP", "no-wtmp"},
        {VTE_PTY_NO_HELPER, "VTE_PTY_NO_HELPER", "no-helper"},
        {VTE_PTY_NO_FALLBACK, "VTE_PTY_NO_FALLBACK", "no-fallback"},
        {VTE_PTY_DEFAULT, "VTE_PTY_DEFAULT", "default"},
        {0, NULL, NULL}};
    GType g_define_type_id = g_flags_register_static(g_intern_static_string("VtePtyFlags"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

GType vte_pty_error_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
        {VTE_PTY_ERROR_PTY_HELPER_FAILED, "VTE_PTY_ERROR_PTY_HELPER_FAILED", "pty-helper-failed"},
        {VTE_PTY_ERROR_PTY98_FAILED, "VTE_PTY_ERROR_PTY98_FAILED", "pty98-failed"},
        {0, NULL, NULL}};
    GType g_define_type_id = g_enum_register_static(g_intern_static_string("VtePtyError"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}
#endif

GType vte_terminal_anti_alias_get_type(void) {
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter(&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
        {VTE_ANTI_ALIAS_USE_DEFAULT, "VTE_ANTI_ALIAS_USE_DEFAULT", "use-default"},
        {VTE_ANTI_ALIAS_FORCE_ENABLE, "VTE_ANTI_ALIAS_FORCE_ENABLE", "force-enable"},
        {VTE_ANTI_ALIAS_FORCE_DISABLE, "VTE_ANTI_ALIAS_FORCE_DISABLE", "force-disable"},
        {0, NULL, NULL}};
    GType g_define_type_id =
        g_enum_register_static(g_intern_static_string("VteTerminalAntiAlias"), values);

    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

#endif /* VTE_CHECK_VERSION(0,38,0) */

/* Generated data ends here */
