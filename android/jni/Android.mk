LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mlterm
ifneq (,$(wildcard fribidi/fribidi.c))
	FRIBIDI_SRC_FILES := fribidi/fribidi.c fribidi/fribidi-arabic.c \
		fribidi/fribidi-bidi.c fribidi/fribidi-bidi-types.c \
		fribidi/fribidi-deprecated.c fribidi/fribidi-joining.c \
		fribidi/fribidi-joining-types.c fribidi/fribidi-mem.c \
		fribidi/fribidi-mirroring.c fribidi/fribidi-run.c fribidi/fribidi-shape.c \
		vtemu/libctl/vt_bidi.c vtemu/libctl/vt_shape_bidi.c \
		vtemu/libctl/vt_line_bidi.c
	FRIBIDI_CFLAGS := -DUSE_FRIBIDI
endif
ifneq (,$(wildcard freetype/$(TARGET_ARCH_ABI)/lib/libfreetype.a))
	FT_CFLAGS := -DUSE_FREETYPE -Ifreetype/$(TARGET_ARCH_ABI)/include/freetype2
	FT_LDLIBS := freetype/$(TARGET_ARCH_ABI)/lib/libfreetype.a
	ifneq (,$(wildcard libotf/otfopen.c))
		OTL_SRC_FILES := libotf/otfopen.c libotf/otfdrive.c libotf/otferror.c \
				uitoolkit/libotl/otf.c
		OTL_CFLAGS := -Ilibotf -Iuitoolkit/libotl -DUSE_OT_LAYOUT -DNO_DYNAMIC_LOAD_OTL
	else
		OTL_SRC_FILES :=
		OTL_CFLAGS :=
	endif
else
	FT_CFLAGS :=
	FT_LDLIBS :=
	OTL_SRC_FILES :=
	OTL_CFLAGS :=
endif
ifneq (,$(wildcard libssh2/$(TARGET_ARCH_ABI)/lib/libssh2.a))
	LIBSSH2_SRC_FILES := vtemu/vt_pty_ssh.c vtemu/vt_pty_mosh.c
	LIBSSH2_CFLAGS := -DUSE_LIBSSH2 -Ilibssh2/$(TARGET_ARCH_ABI)/include -DNO_DYNAMIC_LOAD_SSH
	LIBSSH2_LDLIBS := libssh2/$(TARGET_ARCH_ABI)/lib/libssh2.a libssh2/$(TARGET_ARCH_ABI)/lib/libcrypto.a -lz
else
	LIBSSH2_SRC_FILES :=
	LIBSSH2_CFLAGS :=
	LIBSSH2_LDLIBS :=
endif
LOCAL_SRC_FILES := baselib/src/bl_map.c baselib/src/bl_args.c \
		baselib/src/bl_mem.c baselib/src/bl_conf.c baselib/src/bl_file.c \
		baselib/src/bl_path.c baselib/src/bl_conf_io.c baselib/src/bl_str.c \
		baselib/src/bl_cycle_index.c baselib/src/bl_langinfo.c baselib/src/bl_time.c \
		baselib/src/bl_locale.c baselib/src/bl_privilege.c baselib/src/bl_unistd.c \
		baselib/src/bl_sig_child.c baselib/src/bl_dialog.c baselib/src/bl_pty_streams.c \
		baselib/src/bl_utmp_none.c baselib/src/bl_dlfcn.c baselib/src/bl_dlfcn_dl.c \
		baselib/src/bl_util.c \
		encodefilter/src/ef_parser.c encodefilter/src/ef_iso2022_parser.c encodefilter/src/ef_iso8859_parser.c \
		encodefilter/src/ef_xct_parser.c encodefilter/src/ef_eucjp_parser.c encodefilter/src/ef_euckr_parser.c \
		encodefilter/src/ef_euccn_parser.c encodefilter/src/ef_iso2022jp_parser.c \
		encodefilter/src/ef_iso2022kr_parser.c encodefilter/src/ef_sjis_parser.c encodefilter/src/ef_big5_parser.c \
		encodefilter/src/ef_euctw_parser.c encodefilter/src/ef_utf16_parser.c encodefilter/src/ef_iso2022cn_parser.c \
		encodefilter/src/ef_hz_parser.c encodefilter/src/ef_utf8_parser.c encodefilter/src/ef_johab_parser.c \
		encodefilter/src/ef_8bit_parser.c encodefilter/src/ef_utf32_parser.c encodefilter/src/ef_codepoint_parser.c \
		encodefilter/src/ef_iso8859_conv.c encodefilter/src/ef_iso2022_conv.c encodefilter/src/ef_iso2022jp_conv.c \
		encodefilter/src/ef_iso2022kr_conv.c encodefilter/src/ef_sjis_conv.c encodefilter/src/ef_utf8_conv.c \
		encodefilter/src/ef_big5_conv.c encodefilter/src/ef_euctw_conv.c encodefilter/src/ef_iso2022cn_conv.c \
		encodefilter/src/ef_hz_conv.c encodefilter/src/ef_utf16_conv.c encodefilter/src/ef_eucjp_conv.c \
		encodefilter/src/ef_euckr_conv.c encodefilter/src/ef_euccn_conv.c encodefilter/src/ef_johab_conv.c \
		encodefilter/src/ef_8bit_conv.c encodefilter/src/ef_xct_conv.c encodefilter/src/ef_utf32_conv.c \
		encodefilter/src/ef_ucs4_map.c encodefilter/src/ef_locale_ucs4_map.c encodefilter/src/ef_zh_cn_map.c \
		encodefilter/src/ef_zh_tw_map.c encodefilter/src/ef_zh_hk_map.c encodefilter/src/ef_ko_kr_map.c \
		encodefilter/src/ef_viet_map.c encodefilter/src/ef_ja_jp_map.c encodefilter/src/ef_ru_map.c \
		encodefilter/src/ef_uk_map.c encodefilter/src/ef_tg_map.c encodefilter/src/ef_ucs_property.c \
		encodefilter/src/ef_jis_property.c \
		encodefilter/src/ef_char.c encodefilter/src/ef_sjis_env.c encodefilter/src/ef_tblfunc_loader.c \
		encodefilter/src/ef_ucs4_iso8859.c encodefilter/src/ef_ucs4_viscii.c \
		encodefilter/src/ef_ucs4_tcvn5712_1.c encodefilter/src/ef_ucs4_koi8.c encodefilter/src/ef_ucs4_georgian_ps.c \
		encodefilter/src/ef_ucs4_cp125x.c encodefilter/src/ef_ucs4_iscii.c encodefilter/src/ef_ucs4_jisx0201.c \
		encodefilter/src/ef_ucs4_jisx0208.c encodefilter/src/ef_ucs4_jisx0212.c encodefilter/src/ef_ucs4_jisx0213.c \
		encodefilter/src/ef_ucs4_ksc5601.c encodefilter/src/ef_ucs4_uhc.c encodefilter/src/ef_ucs4_johab.c \
		encodefilter/src/ef_ucs4_gb2312.c encodefilter/src/ef_ucs4_gbk.c encodefilter/src/ef_ucs4_big5.c \
		encodefilter/src/ef_ucs4_cns11643.c encodefilter/src/ef_gb18030_2000_intern.c \
		vtemu/vt_char.c vtemu/vt_str.c vtemu/vt_line.c vtemu/vt_model.c \
		vtemu/vt_char_encoding.c vtemu/vt_color.c vtemu/vt_edit.c \
		vtemu/vt_line_shape.c vtemu/vt_edit_util.c \
		vtemu/vt_edit_scroll.c vtemu/vt_cursor.c vtemu/vt_logical_visual.c \
		vtemu/vt_logs.c vtemu/vt_screen.c vtemu/vt_shape.c vtemu/vt_str_parser.c \
		vtemu/vt_term.c vtemu/vt_parser.c vtemu/vt_term_manager.c vtemu/vt_bidi.c \
		vtemu/vt_config_menu.c vtemu/vt_config_proto.c \
		vtemu/vt_termcap.c vtemu/vt_pty.c vtemu/vt_pty_unix.c vtemu/vt_drcs.c \
		libind/indian.c libind/lex.split.c vtemu/libctl/vt_iscii.c \
		vtemu/libctl/vt_shape_iscii.c vtemu/libctl/vt_line_iscii.c \
		vtemu/vt_ot_layout.c \
		$(OTL_SRC_FILES) $(FRIBIDI_SRC_FILES) $(LIBSSH2_SRC_FILES) \
		uitoolkit/fb/ui.c uitoolkit/fb/ui_font.c uitoolkit/ui_mod_meta_mode.c uitoolkit/ui_shortcut.c \
		uitoolkit/ui_bel_mode.c uitoolkit/ui_font_cache.c uitoolkit/ui_picture.c \
		uitoolkit/fb/ui_color.c uitoolkit/ui_font_config.c uitoolkit/ui_sb_mode.c \
		uitoolkit/ui_color_cache.c uitoolkit/ui_font_manager.c uitoolkit/ui_type_engine.c \
		uitoolkit/ui_color_manager.c uitoolkit/fb/ui_gc.c uitoolkit/fb/ui_decsp_font.c \
		uitoolkit/fb/ui_connect_dialog.c uitoolkit/ui_im.c \
		uitoolkit/fb/ui_window.c uitoolkit/fb/ui_display.c uitoolkit/ui_im_candidate_screen.c \
		uitoolkit/ui_screen.c uitoolkit/fb/ui_xic.c uitoolkit/fb/ui_dnd.c \
		uitoolkit/ui_im_status_screen.c uitoolkit/ui_screen_manager.c uitoolkit/ui_draw_str.c \
		uitoolkit/fb/ui_imagelib.c uitoolkit/ui_event_source.c \
		uitoolkit/ui_main_config.c uitoolkit/ui_selection.c \
		uitoolkit/ui_layout.c uitoolkit/ui_simple_sb_view.c \
		uitoolkit/ui_sb_view_factory.c uitoolkit/ui_scrollbar.c \
		uitoolkit/fb/ui_selection_encoding.c uitoolkit/ui_emoji.c \
		main/daemon.c main/main_loop.c main/main.c
LOCAL_CFLAGS := -DNO_DYNAMIC_LOAD_TABLE -DNO_DYNAMIC_LOAD_CTL -DSTATIC_LINK_INDIC_TABLES -DUSE_IND -Ilibind $(FRIBIDI_CFLAGS) $(FT_CFLAGS) $(OTL_CFLAGS) $(LIBSSH2_CFLAGS) -DLIBDIR=\"/sdcard/.mlterm/lib/\" -DNO_DYNAMIC_LOAD_TYPE -DUSE_TYPE_XCORE -DLIBEXECDIR=\"/sdcard/.mlterm/libexec/\" -DUSE_FRAMEBUFFER -DBUILTIN_IMAGELIB #-DBL_DEBUG -DDEBUG
LOCAL_LDLIBS := -llog -landroid $(FT_LDLIBS) $(LIBSSH2_LDLIBS)
LOCAL_C_INCLUDES := baselib encodefilter vtemu uitoolkit fribidi
LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
