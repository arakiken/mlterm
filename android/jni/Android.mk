LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mlterm

JNI_PATH := $(LOCAL_PATH)/

ifneq (,$(wildcard $(JNI_PATH)fribidi/fribidi.c))
	FRIBIDI_SRC_FILES := fribidi/fribidi.c fribidi/fribidi-arabic.c \
		fribidi/fribidi-bidi.c fribidi/fribidi-bidi-types.c \
		fribidi/fribidi-deprecated.c fribidi/fribidi-joining.c \
		fribidi/fribidi-joining-types.c fribidi/fribidi-mem.c \
		fribidi/fribidi-mirroring.c fribidi/fribidi-run.c fribidi/fribidi-shape.c \
		vtemu/libctl/vt_bidi.c vtemu/libctl/vt_shape_bidi.c \
		vtemu/libctl/vt_line_bidi.c
	FRIBIDI_CFLAGS := -DUSE_FRIBIDI
	FRIBIDI_INCLUDES := $(JNI_PATH)fribidi
endif
ifneq (,$(wildcard $(JNI_PATH)freetype/$(TARGET_ARCH_ABI)/lib/libfreetype.a))
	FT_CFLAGS := -DUSE_FREETYPE
	FT_INCLUDES := $(JNI_PATH)freetype/$(TARGET_ARCH_ABI)/include/freetype2
	FT_LDLIBS := $(JNI_PATH)freetype/$(TARGET_ARCH_ABI)/lib/libfreetype.a
	#FT_LDLIBS := -L$(JNI_PATH)freetype/$(TARGET_ARCH_ABI)/lib
	#FT_STATICLIBS := freetype
	ifneq (,$(wildcard $(JNI_PATH)libotf/otfopen.c))
		OTL_SRC_FILES := libotf/otfopen.c libotf/otfdrive.c libotf/otferror.c uitoolkit/libotl/otf.c
		OTL_CFLAGS := -DUSE_OT_LAYOUT -DNO_DYNAMIC_LOAD_OTL
		OTL_INCLUDES := $(JNI_PATH)libotf $(JNI_PATH)uitoolkit/libotl
	else
		OTL_SRC_FILES :=
		OTL_CFLAGS :=
		OTL_INCLUDES :=
	endif
else
	FT_CFLAGS :=
	FT_INCLUDES :=
	FT_LDLIBS :=
	#FT_STATICLIBS :=
	OTL_SRC_FILES :=
	OTL_CFLAGS :=
endif

ifneq (,$(wildcard $(JNI_PATH)libssh2/$(TARGET_ARCH_ABI)/lib/libssh2.a))
	LIBSSH2_SRC_FILES := vtemu/vt_pty_ssh.c
	LIBSSH2_CFLAGS := -DUSE_LIBSSH2 -DNO_DYNAMIC_LOAD_SSH
	LIBSSH2_INCLUDES := $(JNI_PATH)libssh2/$(TARGET_ARCH_ABI)/include
	LIBSSH2_LDLIBS := $(JNI_PATH)libssh2/$(TARGET_ARCH_ABI)/lib/libssh2.a $(JNI_PATH)libssh2/$(TARGET_ARCH_ABI)/lib/libcrypto.a -lz
	#LIBSSH2_LDLIBS := -L$(JNI_PATH)libssh2/$(TARGET_ARCH_ABI)/lib -lz
	#LIBSSH2_STATICLIBS := ssh2 crypto

	ifneq (,$(wildcard $(JNI_PATH)protobuf/$(TARGET_ARCH_ABI)/lib/libprotobuf.a))
		MOSH_SRC_FILES := vtemu/libptymosh/userinput.pb.cc vtemu/libptymosh/vt_pty_mosh.cpp \
			vtemu/libptymosh/hostinput.pb.cc vtemu/libptymosh/transportinstruction.pb.cc \
			vtemu/libptymosh/compressor.cc vtemu/libptymosh/network.cc \
			vtemu/libptymosh/transportfragment.cc vtemu/libptymosh/base64.cc vtemu/libptymosh/crypto.cc \
			vtemu/libptymosh/ocb.cc vtemu/libptymosh/parser.cc vtemu/libptymosh/parseraction.cc \
			vtemu/libptymosh/parserstate.cc vtemu/libptymosh/terminal.cc \
			vtemu/libptymosh/terminaldispatcher.cc vtemu/libptymosh/terminaldisplay.cc \
			vtemu/libptymosh/terminaldisplayinit.cc vtemu/libptymosh/terminalframebuffer.cc \
			vtemu/libptymosh/terminalfunctions.cc vtemu/libptymosh/terminaluserinput.cc \
			vtemu/libptymosh/timestamp.cc vtemu/libptymosh/completeterminal.cc \
			vtemu/libptymosh/user.cc vtemu/libptymosh/terminaloverlay.cc
		MOSH_INCLUDES := $(JNI_PATH)protobuf/$(TARGET_ARCH_ABI)/include $(JNI_PATH)vtemu/libptymosh
		# If you use -lc++_shared, add "APP_STL := c++_shared" to Aplication.mk
		# (See https://developer.android.com/ndk/guides/cpp-support?hl=ja)
		MOSH_LDLIBS := $(JNI_PATH)protobuf/$(TARGET_ARCH_ABI)/lib/libprotobuf.a -lc++_static
	else
		MOSH_SRC_FILES := vtemu/vt_pty_mosh.c
	endif
else
	LIBSSH2_SRC_FILES :=
	LIBSSH2_CFLAGS :=
	LIBSSH2_INCLUDES :=
	#LIBSSH2_LDLIBS :=
	#LIBSSH2_STATICLIBS :=
	MOSH_SRC_FILES :=
	MOSH_INCLUDES :=
	MOSH_LDLIBS :=
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
		vtemu/vt_ot_layout.c vtemu/vt_transfer.c vtemu/zmodem.c \
		$(OTL_SRC_FILES) $(FRIBIDI_SRC_FILES) $(LIBSSH2_SRC_FILES) $(MOSH_SRC_FILES) \
		uitoolkit/fb/ui.c uitoolkit/fb/ui_font.c uitoolkit/ui_mod_meta_mode.c uitoolkit/ui_shortcut.c \
		uitoolkit/ui_bel_mode.c uitoolkit/ui_font_cache.c uitoolkit/ui_picture.c \
		uitoolkit/fb/ui_color.c uitoolkit/ui_font_config.c uitoolkit/ui_sb_mode.c \
		uitoolkit/ui_color_cache.c uitoolkit/ui_font_manager.c uitoolkit/ui_type_engine.c \
		uitoolkit/ui_color_manager.c uitoolkit/fb/ui_gc.c uitoolkit/fb/ui_decsp_font.c \
		uitoolkit/fb/ui_connect_dialog.c uitoolkit/ui_im.c uitoolkit/ui_copymode.c \
		uitoolkit/fb/ui_window.c uitoolkit/fb/ui_display.c uitoolkit/ui_im_candidate_screen.c \
		uitoolkit/ui_screen.c uitoolkit/fb/ui_xic.c uitoolkit/fb/ui_dnd.c \
		uitoolkit/ui_im_status_screen.c uitoolkit/ui_screen_manager.c uitoolkit/ui_draw_str.c \
		uitoolkit/fb/ui_imagelib.c uitoolkit/ui_event_source.c \
		uitoolkit/ui_main_config.c uitoolkit/ui_selection.c \
		uitoolkit/ui_layout.c uitoolkit/ui_simple_sb_view.c \
		uitoolkit/ui_sb_view_factory.c uitoolkit/ui_scrollbar.c \
		uitoolkit/fb/ui_selection_encoding.c uitoolkit/ui_emoji.c uitoolkit/test.c \
		main/daemon.c main/main_loop.c main/main.c

LOCAL_CFLAGS := -DNO_DYNAMIC_LOAD_TABLE -DNO_DYNAMIC_LOAD_CTL -DSTATIC_LINK_INDIC_TABLES -DUSE_IND $(FRIBIDI_CFLAGS) $(FT_CFLAGS) $(OTL_CFLAGS) $(LIBSSH2_CFLAGS) $(MOSH_CFLAGS) -DLIBDIR=\"/sdcard/.mlterm/lib/\" -DNO_DYNAMIC_LOAD_TYPE -DUSE_TYPE_XCORE -DLIBEXECDIR=\"/sdcard/.mlterm/libexec/\" -DUSE_FRAMEBUFFER -DBUILTIN_IMAGELIB -DNO_DYNAMIC_LOAD_TRANSFER -DUSE_COMPACT_TRUECOLOR $(LOCAL_CFLAGS_DEB)

LOCAL_LDLIBS := $(FT_LDLIBS) $(MOSH_LDLIBS) $(LIBSSH2_LDLIBS) -llog -landroid

LOCAL_C_INCLUDES := $(JNI_PATH)baselib $(JNI_PATH)encodefilter $(JNI_PATH)vtemu $(JNI_PATH)uitoolkit $(JNI_PATH)libind $(FRIBIDI_INCLUDES) $(FT_INCLUDES) $(OTL_INCLUDES) $(LIBSSH2_INCLUDES) $(MOSH_INCLUDES)

LOCAL_STATIC_LIBRARIES := android_native_app_glue #$(FT_STATICLIBS) $(LIBSSH2_STATICLIBS)

#APP_ALLOW_MISSING_DEPS := true
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

# android ndk disables C++ exception by default.
LOCAL_CPPFLAGS := -fexceptions -frtti

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
