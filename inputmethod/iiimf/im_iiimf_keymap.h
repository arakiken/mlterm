static inline void
xksym_to_iiimfkey(
	KeySym ksym ,
	IIIMP_int32 *  kchar ,
	IIIMP_int32 *  kcode
	)
{
	switch (ksym)
	{
	/* Latin 1 */
	case  XK_space:
		*kcode = IIIMF_KEYCODE_SPACE ;
		*kchar = 0x0020 ;
		break;
	case  XK_exclam:
		*kcode = IIIMF_KEYCODE_EXCLAMATION_MARK ;
		*kchar = 0x0021 ;
		break ;
	case  XK_quotedbl:
		*kcode = IIIMF_KEYCODE_QUOTEDBL ;
		*kchar = 0x0022 ;
		break ;
	case  XK_numbersign:
		*kcode = IIIMF_KEYCODE_NUMBER_SIGN ;
		*kchar = 0x0023 ;
		break ;
	case  XK_dollar:
		*kcode = IIIMF_KEYCODE_DOLLAR ;
		*kchar = 0x0024 ;
		break ;
	case  XK_percent:
		*kcode = IIIMF_KEYCODE_UNDEFINED ; /* FIXME */
		*kchar = 0x0025 ;
		break ;
	case  XK_ampersand:
		*kcode = IIIMF_KEYCODE_AMPERSAND ;
		*kchar = 0x0026 ;
		break ;
	case  XK_apostrophe:
	/* case  XK_quoteright: */
		*kcode = IIIMF_KEYCODE_UNDEFINED ; /* FIXME */
		*kchar = 0x0027 ;
		break ;
	case  XK_parenleft:
		*kcode = IIIMF_KEYCODE_LEFT_PARENTHESIS ;
		*kchar = 0x0028 ;
		break ;
	case  XK_parenright:
		*kcode = IIIMF_KEYCODE_RIGHT_PARENTHESIS ;
		*kchar = 0x0029 ;
		break ;
	case  XK_asterisk:
		*kcode = IIIMF_KEYCODE_ASTERISK ;
		*kchar = 0x002a ;
		break ;
	case  XK_plus:
		*kcode = IIIMF_KEYCODE_PLUS ;
		*kchar = 0x002b ;
		break ;
	case  XK_comma:
		*kcode = IIIMF_KEYCODE_COMMA ;
		*kchar = 0x002c ;
		break ;
	case  XK_minus:
		*kcode = IIIMF_KEYCODE_MINUS ;
		*kchar = 0x002d ;
		break ;
	case  XK_period:
		*kcode = IIIMF_KEYCODE_PERIOD ;
		*kchar = 0x002e ;
		break ;
	case  XK_slash:
		*kcode = IIIMF_KEYCODE_SLASH ;
		*kchar = 0x002f ;
		break ;
	case  XK_0:
		*kcode = IIIMF_KEYCODE_0 ;
		*kchar = 0x0030 ;
		break ;
	case  XK_1:
		*kcode = IIIMF_KEYCODE_1 ;
		*kchar = 0x0031 ;
		break ;
	case  XK_2:
		*kcode = IIIMF_KEYCODE_2 ;
		*kchar = 0x0032 ;
		break ;
	case  XK_3:
		*kcode = IIIMF_KEYCODE_3 ;
		*kchar = 0x0033 ;
		break ;
	case  XK_4:
		*kcode = IIIMF_KEYCODE_4 ;
		*kchar = 0x0034 ;
		break ;
	case  XK_5:
		*kcode = IIIMF_KEYCODE_5 ;
		*kchar = 0x0035 ;
		break ;
	case  XK_6:
		*kcode = IIIMF_KEYCODE_6 ;
		*kchar = 0x0036 ;
		break ;
	case  XK_7:
		*kcode = IIIMF_KEYCODE_7 ;
		*kchar = 0x0037 ;
		break ;
	case  XK_8:
		*kcode = IIIMF_KEYCODE_8 ;
		*kchar = 0x0038 ;
		break ;
	case  XK_9:
		*kcode = IIIMF_KEYCODE_9 ;
		*kchar = 0x0039 ;
		break ;
	case  XK_colon:
		*kcode = IIIMF_KEYCODE_COLON ;
		*kchar = 0x003a ;
		break ;
	case  XK_semicolon:
		*kcode = IIIMF_KEYCODE_SEMICOLON ;
		*kchar = 0x003b ;
		break ;
	case  XK_less:
		*kcode = IIIMF_KEYCODE_LESS ;
		*kchar = 0x003c ;
		break ;
	case  XK_equal:
		*kcode = IIIMF_KEYCODE_EQUALS ; /* XXX: right? */
		*kchar = 0x003d ;
		break ;
	case  XK_greater:
		*kcode = IIIMF_KEYCODE_GREATER ;
		*kchar = 0x003e ;
		break ;
	case  XK_question:
		*kcode = IIIMF_KEYCODE_UNDEFINED ; /* FIXME */
		*kchar = 0x003f ;
		break ;
	case  XK_at:
		*kcode = IIIMF_KEYCODE_AT ;
		*kchar = 0x0040 ;
		break ;
	case  XK_A:
		*kcode = IIIMF_KEYCODE_A ;
		*kchar = 0x0041 ;
		break ;
	case  XK_B:
		*kcode = IIIMF_KEYCODE_B ;
		*kchar = 0x0042 ;
		break ;
	case  XK_C:
		*kcode = IIIMF_KEYCODE_C ;
		*kchar = 0x0043 ;
		break ;
	case  XK_D:
		*kcode = IIIMF_KEYCODE_D ;
		*kchar = 0x0044 ;
		break ;
	case  XK_E:
		*kcode = IIIMF_KEYCODE_E ;
		*kchar = 0x0045 ;
		break ;
	case  XK_F:
		*kcode = IIIMF_KEYCODE_F ;
		*kchar = 0x0046 ;
		break ;
	case  XK_G:
		*kcode = IIIMF_KEYCODE_G ;
		*kchar = 0x0047 ;
		break ;
	case  XK_H:
		*kcode = IIIMF_KEYCODE_H ;
		*kchar = 0x0048 ;
		break ;
	case  XK_I:
		*kcode = IIIMF_KEYCODE_I ;
		*kchar = 0x0049 ;
		break ;
	case  XK_J:
		*kcode = IIIMF_KEYCODE_J ;
		*kchar = 0x004a ;
		break ;
	case  XK_K:
		*kcode = IIIMF_KEYCODE_K ;
		*kchar = 0x004b ;
		break ;
	case  XK_L:
		*kcode = IIIMF_KEYCODE_L ;
		*kchar = 0x004c ;
		break ;
	case  XK_M:
		*kcode = IIIMF_KEYCODE_M ;
		*kchar = 0x004d ;
		break ;
	case  XK_N:
		*kcode = IIIMF_KEYCODE_N ;
		*kchar = 0x004e ;
		break ;
	case  XK_O:
		*kcode = IIIMF_KEYCODE_O ;
		*kchar = 0x004f ;
		break ;
	case  XK_P:
		*kcode = IIIMF_KEYCODE_P ;
		*kchar = 0x0050 ;
		break ;
	case  XK_Q:
		*kcode = IIIMF_KEYCODE_Q ;
		*kchar = 0x0051 ;
		break ;
	case  XK_R:
		*kcode = IIIMF_KEYCODE_R ;
		*kchar = 0x0052 ;
		break ;
	case  XK_S:
		*kcode = IIIMF_KEYCODE_S ;
		*kchar = 0x0053 ;
		break ;
	case  XK_T:
		*kcode = IIIMF_KEYCODE_T ;
		*kchar = 0x0054 ;
		break ;
	case  XK_U:
		*kcode = IIIMF_KEYCODE_U ;
		*kchar = 0x0055 ;
		break ;
	case  XK_V:
		*kcode = IIIMF_KEYCODE_V ;
		*kchar = 0x0056 ;
		break ;
	case  XK_W:
		*kcode = IIIMF_KEYCODE_W ;
		*kchar = 0x0057 ;
		break ;
	case  XK_X:
		*kcode = IIIMF_KEYCODE_X ;
		*kchar = 0x0058 ;
		break ;
	case  XK_Y:
		*kcode = IIIMF_KEYCODE_Y ;
		*kchar = 0x0059 ;
		break ;
	case  XK_Z:
		*kcode = IIIMF_KEYCODE_Z ;
		*kchar = 0x005a ;
		break ;
	case  XK_bracketleft:
		*kcode = IIIMF_KEYCODE_OPEN_BRACKET ; /* XXX: right? */
		*kchar = 0x005b ;
		break ;
	case  XK_backslash:
		*kcode = IIIMF_KEYCODE_BACK_SLASH ;
		*kchar = 0x005c ;
		break ;
	case  XK_bracketright:
		*kcode = IIIMF_KEYCODE_CLOSE_BRACKET ;
		*kchar = 0x005d ;
		break ;
	case  XK_asciicircum:
		*kcode = IIIMF_KEYCODE_CIRCUMFLEX ;
		*kchar = 0x005e ;
		break ;
	case  XK_underscore:
		*kcode = IIIMF_KEYCODE_UNDERSCORE ;
		*kchar = 0x005f ;
		break ;
	case  XK_grave:
/*	case  XK_quoteleft: */
		*kcode = IIIMF_KEYCODE_UNDEFINED ; /* FIXME */
		*kchar = 0x0060 ;
		break ;
	case  XK_a:
		*kcode = IIIMF_KEYCODE_A ;
		*kchar = 0x0061 ;
		break ;
	case  XK_b:
		*kcode = IIIMF_KEYCODE_B ;
		*kchar = 0x0062 ;
		break ;
	case  XK_c:
		*kcode = IIIMF_KEYCODE_C ;
		*kchar = 0x0063 ;
		break ;
	case  XK_d:
		*kcode = IIIMF_KEYCODE_D ;
		*kchar = 0x0064 ;
		break ;
	case  XK_e:
		*kcode = IIIMF_KEYCODE_E ;
		*kchar = 0x0065 ;
		break ;
	case  XK_f:
		*kcode = IIIMF_KEYCODE_F ;
		*kchar = 0x0066 ;
		break ;
	case  XK_g:
		*kcode = IIIMF_KEYCODE_G ;
		*kchar = 0x0067 ;
		break ;
	case  XK_h:
		*kcode = IIIMF_KEYCODE_H ;
		*kchar = 0x0068 ;
		break ;
	case  XK_i:
		*kcode = IIIMF_KEYCODE_I ;
		*kchar = 0x0069 ;
		break ;
	case  XK_j:
		*kcode = IIIMF_KEYCODE_J ;
		*kchar = 0x006a ;
		break ;
	case  XK_k:
		*kcode = IIIMF_KEYCODE_K ;
		*kchar = 0x006b ;
		break ;
	case  XK_l:
		*kcode = IIIMF_KEYCODE_L ;
		*kchar = 0x006c ;
		break ;
	case  XK_m:
		*kcode = IIIMF_KEYCODE_M ;
		*kchar = 0x006d ;
		break ;
	case  XK_n:
		*kcode = IIIMF_KEYCODE_N ;
		*kchar = 0x006e ;
		break ;
	case  XK_o:
		*kcode = IIIMF_KEYCODE_O ;
		*kchar = 0x006f ;
		break ;
	case  XK_p:
		*kcode = IIIMF_KEYCODE_P ;
		*kchar = 0x0070 ;
		break ;
	case  XK_q:
		*kcode = IIIMF_KEYCODE_Q ;
		*kchar = 0x0071 ;
		break ;
	case  XK_r:
		*kcode = IIIMF_KEYCODE_R ;
		*kchar = 0x0072 ;
		break ;
	case  XK_s:
		*kcode = IIIMF_KEYCODE_S ;
		*kchar = 0x0073 ;
		break ;
	case  XK_t:
		*kcode = IIIMF_KEYCODE_T ;
		*kchar = 0x0074 ;
		break ;
	case  XK_u:
		*kcode = IIIMF_KEYCODE_U ;
		*kchar = 0x0075 ;
		break ;
	case  XK_v:
		*kcode = IIIMF_KEYCODE_V ;
		*kchar = 0x0076 ;
		break ;
	case  XK_w:
		*kcode = IIIMF_KEYCODE_W ;
		*kchar = 0x0077 ;
		break ;
	case  XK_x:
		*kcode = IIIMF_KEYCODE_X ;
		*kchar = 0x0078 ;
		break ;
	case  XK_y:
		*kcode = IIIMF_KEYCODE_Y ;
		*kchar = 0x0079 ;
		break ;
	case  XK_z:
		*kcode = IIIMF_KEYCODE_Z ;
		*kchar = 0x007a ;
		break ;
	case  XK_braceleft:
		*kcode = IIIMF_KEYCODE_BRACELEFT ;
		*kchar = 0x007b ;
		break ;
	case  XK_bar:
		*kcode = IIIMF_KEYCODE_UNDEFINED ;
		*kchar = 0x007c ;
		break ;
	case  XK_braceright:
		*kcode = IIIMF_KEYCODE_BRACERIGHT ;
		*kchar = 0x007d ;
		break ;
	case  XK_asciitilde:
		*kcode = IIIMF_KEYCODE_UNDEFINED ;
		*kchar = 0x007e ;
		break ;
	case  XK_nobreakspace:
		*kcode = IIIMF_KEYCODE_UNDEFINED ;
		*kchar = 0x00a0 ;
		break;
	case  XK_exclamdown:
		*kcode = IIIMF_KEYCODE_INVERTED_EXCLAMATION_MARK ;
		*kchar = 0x00a1 ;
		break;
	case  XK_cent:
	case  XK_sterling:
	case  XK_currency:
	case  XK_yen:
	case  XK_brokenbar:
	case  XK_section:
	case  XK_diaeresis:
	case  XK_copyright:
	case  XK_ordfeminine:
	case  XK_guillemotleft:
	case  XK_notsign:
	case  XK_hyphen:
	case  XK_registered:
	case  XK_macron:
	case  XK_degree:
	case  XK_plusminus:
	case  XK_twosuperior:
	case  XK_threesuperior:
	case  XK_acute:
	case  XK_mu:
	case  XK_paragraph:
	case  XK_periodcentered:
	case  XK_cedilla:
	case  XK_onesuperior:
	case  XK_masculine:
	case  XK_guillemotright:
	case  XK_onequarter:
	case  XK_onehalf:
	case  XK_threequarters:
	case  XK_questiondown:
	case  XK_Agrave:
	case  XK_Aacute:
	case  XK_Acircumflex:
	case  XK_Atilde:
	case  XK_Adiaeresis:
	case  XK_Aring:
	case  XK_AE:
	case  XK_Ccedilla:
	case  XK_Egrave:
	case  XK_Eacute:
	case  XK_Ecircumflex:
	case  XK_Ediaeresis:
	case  XK_Igrave:
	case  XK_Iacute:
	case  XK_Icircumflex:
	case  XK_Idiaeresis:
	case  XK_ETH:
/*	case  XK_Eth:*/
	case  XK_Ntilde:
	case  XK_Ograve:
	case  XK_Oacute:
	case  XK_Ocircumflex:
	case  XK_Otilde:
	case  XK_Odiaeresis:
	case  XK_multiply:
	case  XK_Ooblique:
/*	case  XK_Oslash:*/
	case  XK_Ugrave:
	case  XK_Uacute:
	case  XK_Ucircumflex:
	case  XK_Udiaeresis:
	case  XK_Yacute:
	case  XK_THORN:
/*	case  XK_Thorn:*/
	case  XK_ssharp:
	case  XK_agrave:
	case  XK_aacute:
	case  XK_acircumflex:
	case  XK_atilde:
	case  XK_adiaeresis:
	case  XK_aring:
	case  XK_ae:
	case  XK_ccedilla:
	case  XK_egrave:
	case  XK_eacute:
	case  XK_ecircumflex:
	case  XK_ediaeresis:
	case  XK_igrave:
	case  XK_iacute:
	case  XK_icircumflex:
	case  XK_idiaeresis:
	case  XK_eth:
	case  XK_ntilde:
	case  XK_ograve:
	case  XK_oacute:
	case  XK_ocircumflex:
	case  XK_otilde:
	case  XK_odiaeresis:
	case  XK_division:
	case  XK_oslash:
/*	case  XK_ooblique:*/
	case  XK_ugrave:
	case  XK_uacute:
	case  XK_ucircumflex:
	case  XK_udiaeresis:
	case  XK_yacute:
	case  XK_thorn:
	case  XK_ydiaeresis:
		*kcode = IIIMF_KEYCODE_UNDEFINED ;
		*kchar = ksym ;
		break;

/*
 * TODO: Latin[23489], Katakana, Arabic, Cyrillic, Greek, Technical, Special
 *       Publishing, APL, Hebrew, Thai, Korean , American, Georgian, Azeri,
 *       Vietnamese,
 */
	/* TTY Functions */
	case  XK_BackSpace:
		*kcode = IIIMF_KEYCODE_BACK_SPACE ;
		return ;
	case  XK_Tab:
		*kcode = IIIMF_KEYCODE_TAB ;
		return ;
	case  XK_Clear:
		*kcode = IIIMF_KEYCODE_CLEAR ;
		return ;
	case  XK_Return:
		*kcode = IIIMF_KEYCODE_ENTER ;
		return ;
	case  XK_Pause:
		*kcode = IIIMF_KEYCODE_PAUSE ;
		return ;
	case  XK_Scroll_Lock:
		*kcode = IIIMF_KEYCODE_SCROLL_LOCK ;
		return ;
	case  XK_Escape:
		*kcode = IIIMF_KEYCODE_ESCAPE ;
		return ;
	case  XK_Delete:
		*kcode =  IIIMF_KEYCODE_DELETE ;
		return ;
	/* TODO:
	 * - International & multi-key character composition
	 * - Japanese keyboard support
	 */
	/* Cursor control & motion */
	case XK_Home:
		*kcode = IIIMF_KEYCODE_HOME ;
		break ;
	case XK_Left:
		*kcode = IIIMF_KEYCODE_LEFT ;
		break ;
	case XK_Up:
		*kcode = IIIMF_KEYCODE_UP ;
		break ;
	case XK_Right:
		*kcode = IIIMF_KEYCODE_RIGHT ;
		break ;
	case XK_Down:
		*kcode = IIIMF_KEYCODE_DOWN ;
		break ;
	case XK_Prior:
/*	case XK_Page_Up:*/
		*kcode = IIIMF_KEYCODE_PAGE_UP ;
		break ;
	case XK_Next:
/*	case XK_Page_Down:*/
		*kcode = IIIMF_KEYCODE_PAGE_DOWN ;
		break ;
	case XK_End:
		*kcode = IIIMF_KEYCODE_END ;
		break ;
	case XK_Begin:
		*kcode = IIIMF_KEYCODE_UNDEFINED ;
		break ;
	/*
	 * TODO
	 * - Misc Functions
	 * - Keypad Functions, keypad numbers cleverly chosen to map to ascii
	 * - Auxilliary Functions
	 * - Modifiers
	 * - ISO 9995 Function and Modifier Keys
	 * - 3270 Terminal Keys
	 */
	default:
		*kcode =  IIIMF_KEYCODE_UNDEFINED ;
		return ;
	}
}


