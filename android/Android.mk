LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := MLTermPty
LOCAL_SRC_FILES := kiklib/kiklib/kik_debug.c kiklib/kiklib/kik_map.c kiklib/kiklib/kik_args.c \
		kiklib/kiklib/kik_mem.c kiklib/kiklib/kik_conf.c kiklib/kiklib/kik_file.c \
		kiklib/kiklib/kik_path.c kiklib/kiklib/kik_conf_io.c kiklib/kiklib/kik_str.c \
		kiklib/kiklib/kik_cycle_index.c kiklib/kiklib/kik_langinfo.c kiklib/kiklib/kik_time.c \
		kiklib/kiklib/kik_locale.c kiklib/kiklib/kik_privilege.c kiklib/kiklib/kik_unistd.c \
		kiklib/kiklib/kik_sig_child.c kiklib/kiklib/kik_dialog.c kiklib/kiklib/kik_pty_streams.c \
		kiklib/kiklib/kik_utmp_none.c kiklib/kiklib/kik_dlfcn_dl.c \
		mkf/mkf/mkf_parser.c mkf/mkf/mkf_iso2022_parser.c mkf/mkf/mkf_iso8859_parser.c \
		mkf/mkf/mkf_xct_parser.c mkf/mkf/mkf_eucjp_parser.c mkf/mkf/mkf_euckr_parser.c \
		mkf/mkf/mkf_euccn_parser.c mkf/mkf/mkf_iso2022jp_parser.c \
		mkf/mkf/mkf_iso2022kr_parser.c mkf/mkf/mkf_sjis_parser.c mkf/mkf/mkf_big5_parser.c \
		mkf/mkf/mkf_euctw_parser.c mkf/mkf/mkf_utf16_parser.c mkf/mkf/mkf_iso2022cn_parser.c \
		mkf/mkf/mkf_hz_parser.c mkf/mkf/mkf_utf8_parser.c mkf/mkf/mkf_johab_parser.c \
		mkf/mkf/mkf_8bit_parser.c mkf/mkf/mkf_utf32_parser.c mkf/mkf/mkf_codepoint_parser.c \
		mkf/mkf/mkf_iso8859_conv.c mkf/mkf/mkf_iso2022_conv.c mkf/mkf/mkf_iso2022jp_conv.c \
		mkf/mkf/mkf_iso2022kr_conv.c mkf/mkf/mkf_sjis_conv.c mkf/mkf/mkf_utf8_conv.c \
		mkf/mkf/mkf_big5_conv.c mkf/mkf/mkf_euctw_conv.c mkf/mkf/mkf_iso2022cn_conv.c \
		mkf/mkf/mkf_hz_conv.c mkf/mkf/mkf_utf16_conv.c mkf/mkf/mkf_eucjp_conv.c \
		mkf/mkf/mkf_euckr_conv.c mkf/mkf/mkf_euccn_conv.c mkf/mkf/mkf_johab_conv.c \
		mkf/mkf/mkf_8bit_conv.c mkf/mkf/mkf_xct_conv.c mkf/mkf/mkf_utf32_conv.c \
		mkf/mkf/mkf_ucs4_map.c mkf/mkf/mkf_locale_ucs4_map.c mkf/mkf/mkf_zh_cn_map.c \
		mkf/mkf/mkf_zh_tw_map.c mkf/mkf/mkf_zh_hk_map.c mkf/mkf/mkf_ko_kr_map.c \
		mkf/mkf/mkf_viet_map.c mkf/mkf/mkf_ja_jp_map.c mkf/mkf/mkf_ru_map.c \
		mkf/mkf/mkf_uk_map.c mkf/mkf/mkf_tg_map.c mkf/mkf/mkf_ucs_property.c \
		mkf/mkf/mkf_jisx0208_1983_property.c mkf/mkf/mkf_jisx0213_2000_property.c \
		mkf/mkf/mkf_char.c mkf/mkf/mkf_sjis_env.c mkf/mkf/mkf_tblfunc_loader.c \
		mkf/mkf/mkf_ucs4_usascii.c mkf/mkf/mkf_ucs4_iso8859.c mkf/mkf/mkf_ucs4_viscii.c \
		mkf/mkf/mkf_ucs4_tcvn5712_1.c mkf/mkf/mkf_ucs4_koi8.c mkf/mkf/mkf_ucs4_georgian_ps.c \
		mkf/mkf/mkf_ucs4_cp125x.c mkf/mkf/mkf_ucs4_iscii.c mkf/mkf/mkf_ucs4_jisx0201.c \
		mkf/mkf/mkf_ucs4_jisx0208.c mkf/mkf/mkf_ucs4_jisx0212.c mkf/mkf/mkf_ucs4_jisx0213.c \
		mkf/mkf/mkf_ucs4_ksc5601.c mkf/mkf/mkf_ucs4_uhc.c mkf/mkf/mkf_ucs4_johab.c \
		mkf/mkf/mkf_ucs4_gb2312.c mkf/mkf/mkf_ucs4_gbk.c mkf/mkf/mkf_ucs4_big5.c \
		mkf/mkf/mkf_ucs4_cns11643.c mkf/mkf/mkf_gb18030_2000_intern.c \
		mlterm/ml_char.c mlterm/ml_str.c mlterm/ml_line.c mlterm/ml_model.c \
		mlterm/ml_char_encoding.c mlterm/ml_color.c mlterm/ml_edit.c mlterm/ml_edit_util.c \
		mlterm/ml_edit_scroll.c mlterm/ml_cursor.c mlterm/ml_logical_visual.c \
		mlterm/ml_logs.c mlterm/ml_screen.c mlterm/ml_shape.c mlterm/ml_str_parser.c \
		mlterm/ml_term.c mlterm/ml_vt100_parser.c mlterm/ml_term_manager.c mlterm/ml_bidi.c \
		mlterm/ml_iscii.c mlterm/ml_config_menu.c mlterm/ml_config_proto.c mlterm/ml_pty.c \
		mlterm/ml_pty_unix.c java/MLTermPty.c
LOCAL_CFLAGS := -Ikiklib -Imkf -Imlterm -Ijava -DNO_DYNAMIC_LOAD_CTL

include $(BUILD_SHARED_LIBRARY)
