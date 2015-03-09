LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mlterm
ifeq ($(ENABLE_FRIBIDI),true)
	FRIBIDI_SRC_FILES := fribidi/fribidi.c fribidi/fribidi-arabic.c \
		fribidi/fribidi-bidi.c fribidi/fribidi-bidi-types.c \
		fribidi/fribidi-deprecated.c fribidi/fribidi-joining.c \
		fribidi/fribidi-joining-types.c fribidi/fribidi-mem.c \
		fribidi/fribidi-mirroring.c fribidi/fribidi-run.c fribidi/fribidi-shape.c \
		mlterm/libctl/ml_bidi.c mlterm/libctl/ml_shape_bidi.c \
		mlterm/libctl/ml_line_bidi.c
	FRIBIDI_CFLAGS=-DUSE_FRIBIDI
endif
ifeq ($(TARGET_ARCH_ABI),mips)
	FT_CFLAGS :=
	FT_LDLIBS :=
else
	FT_CFLAGS := -DUSE_FREETYPE -Ifreetype/$(TARGET_ARCH_ABI)/include/freetype2
	FT_LDLIBS := freetype/$(TARGET_ARCH_ABI)/lib/libfreetype.a
endif
LOCAL_SRC_FILES := kiklib/src/kik_map.c kiklib/src/kik_args.c \
		kiklib/src/kik_mem.c kiklib/src/kik_conf.c kiklib/src/kik_file.c \
		kiklib/src/kik_path.c kiklib/src/kik_conf_io.c kiklib/src/kik_str.c \
		kiklib/src/kik_cycle_index.c kiklib/src/kik_langinfo.c kiklib/src/kik_time.c \
		kiklib/src/kik_locale.c kiklib/src/kik_privilege.c kiklib/src/kik_unistd.c \
		kiklib/src/kik_sig_child.c kiklib/src/kik_dialog.c kiklib/src/kik_pty_streams.c \
		kiklib/src/kik_utmp_none.c kiklib/src/kik_dlfcn.c kiklib/src/kik_dlfcn_dl.c \
		mkf/lib/mkf_parser.c mkf/lib/mkf_iso2022_parser.c mkf/lib/mkf_iso8859_parser.c \
		mkf/lib/mkf_xct_parser.c mkf/lib/mkf_eucjp_parser.c mkf/lib/mkf_euckr_parser.c \
		mkf/lib/mkf_euccn_parser.c mkf/lib/mkf_iso2022jp_parser.c \
		mkf/lib/mkf_iso2022kr_parser.c mkf/lib/mkf_sjis_parser.c mkf/lib/mkf_big5_parser.c \
		mkf/lib/mkf_euctw_parser.c mkf/lib/mkf_utf16_parser.c mkf/lib/mkf_iso2022cn_parser.c \
		mkf/lib/mkf_hz_parser.c mkf/lib/mkf_utf8_parser.c mkf/lib/mkf_johab_parser.c \
		mkf/lib/mkf_8bit_parser.c mkf/lib/mkf_utf32_parser.c mkf/lib/mkf_codepoint_parser.c \
		mkf/lib/mkf_iso8859_conv.c mkf/lib/mkf_iso2022_conv.c mkf/lib/mkf_iso2022jp_conv.c \
		mkf/lib/mkf_iso2022kr_conv.c mkf/lib/mkf_sjis_conv.c mkf/lib/mkf_utf8_conv.c \
		mkf/lib/mkf_big5_conv.c mkf/lib/mkf_euctw_conv.c mkf/lib/mkf_iso2022cn_conv.c \
		mkf/lib/mkf_hz_conv.c mkf/lib/mkf_utf16_conv.c mkf/lib/mkf_eucjp_conv.c \
		mkf/lib/mkf_euckr_conv.c mkf/lib/mkf_euccn_conv.c mkf/lib/mkf_johab_conv.c \
		mkf/lib/mkf_8bit_conv.c mkf/lib/mkf_xct_conv.c mkf/lib/mkf_utf32_conv.c \
		mkf/lib/mkf_ucs4_map.c mkf/lib/mkf_locale_ucs4_map.c mkf/lib/mkf_zh_cn_map.c \
		mkf/lib/mkf_zh_tw_map.c mkf/lib/mkf_zh_hk_map.c mkf/lib/mkf_ko_kr_map.c \
		mkf/lib/mkf_viet_map.c mkf/lib/mkf_ja_jp_map.c mkf/lib/mkf_ru_map.c \
		mkf/lib/mkf_uk_map.c mkf/lib/mkf_tg_map.c mkf/lib/mkf_ucs_property.c \
		mkf/lib/mkf_jisx0208_1983_property.c mkf/lib/mkf_jisx0213_2000_property.c \
		mkf/lib/mkf_char.c mkf/lib/mkf_sjis_env.c mkf/lib/mkf_tblfunc_loader.c \
		mkf/lib/mkf_ucs4_iso8859.c mkf/lib/mkf_ucs4_viscii.c \
		mkf/lib/mkf_ucs4_tcvn5712_1.c mkf/lib/mkf_ucs4_koi8.c mkf/lib/mkf_ucs4_georgian_ps.c \
		mkf/lib/mkf_ucs4_cp125x.c mkf/lib/mkf_ucs4_iscii.c mkf/lib/mkf_ucs4_jisx0201.c \
		mkf/lib/mkf_ucs4_jisx0208.c mkf/lib/mkf_ucs4_jisx0212.c mkf/lib/mkf_ucs4_jisx0213.c \
		mkf/lib/mkf_ucs4_ksc5601.c mkf/lib/mkf_ucs4_uhc.c mkf/lib/mkf_ucs4_johab.c \
		mkf/lib/mkf_ucs4_gb2312.c mkf/lib/mkf_ucs4_gbk.c mkf/lib/mkf_ucs4_big5.c \
		mkf/lib/mkf_ucs4_cns11643.c mkf/lib/mkf_gb18030_2000_intern.c \
		mlterm/ml_char.c mlterm/ml_str.c mlterm/ml_line.c mlterm/ml_model.c \
		mlterm/ml_char_encoding.c mlterm/ml_color.c mlterm/ml_edit.c mlterm/ml_edit_util.c \
		mlterm/ml_edit_scroll.c mlterm/ml_cursor.c mlterm/ml_logical_visual.c \
		mlterm/ml_logs.c mlterm/ml_screen.c mlterm/ml_shape.c mlterm/ml_str_parser.c \
		mlterm/ml_term.c mlterm/ml_vt100_parser.c mlterm/ml_term_manager.c mlterm/ml_bidi.c \
		mlterm/ml_config_menu.c mlterm/ml_config_proto.c \
		mlterm/ml_termcap.c mlterm/ml_pty.c mlterm/ml_pty_unix.c mlterm/ml_drcs.c \
		libind/indian.c libind/lex.split.c mlterm/libctl/ml_iscii.c \
		mlterm/libctl/ml_shape_iscii.c mlterm/libctl/ml_line_iscii.c \
		$(FRIBIDI_SRC_FILES) \
		xwindow/fb/x.c xwindow/fb/x_font.c xwindow/x_mod_meta_mode.c xwindow/x_shortcut.c \
		xwindow/x_bel_mode.c xwindow/x_font_cache.c xwindow/x_picture.c \
		xwindow/fb/x_color.c xwindow/x_font_config.c xwindow/x_sb_mode.c \
		xwindow/x_color_cache.c xwindow/x_font_manager.c xwindow/x_type_engine.c \
		xwindow/x_color_manager.c xwindow/fb/x_gc.c xwindow/x_type_loader.c \
		xwindow/fb/x_connect_dialog.c xwindow/x_im.c \
		xwindow/fb/x_window.c xwindow/fb/x_display.c xwindow/x_im_candidate_screen.c \
		xwindow/x_screen.c xwindow/fb/x_xic.c xwindow/fb/x_dnd.c \
		xwindow/x_im_status_screen.c xwindow/x_screen_manager.c xwindow/x_draw_str.c \
		xwindow/fb/x_imagelib.c xwindow/x_event_source.c \
		xwindow/x_main_config.c xwindow/x_selection.c \
		xwindow/x_sb_screen.c xwindow/x_simple_sb_view.c \
		xwindow/x_sb_view_factory.c xwindow/x_scrollbar.c \
		main/daemon.c main/main_loop.c main/main.c
LOCAL_CFLAGS := -DNO_DYNAMIC_LOAD_TABLE -DNO_DYNAMIC_LOAD_CTL -DSTATIC_LINK_INDIC_TABLES -DUSE_IND -Ilibind $(FRIBIDI_CFLAGS) $(FT_CFLAGS) -DLIBDIR=\"/sdcard/.mlterm/lib/\" -DNO_DYNAMIC_LOAD_TYPE -DUSE_TYPE_XCORE -DLIBEXECDIR=\"/sdcard/.mlterm/libexec/\" -DUSE_FRAMEBUFFER #-DKIK_DEBUG -DDEBUG
LOCAL_LDLIBS := -llog -landroid $(FT_LDLIBS)
LOCAL_C_INCLUDES := kiklib mkf mlterm xwindow fribidi
LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
