/*	$NetBSD: wskbdutil.c,v 1.19 2017/11/03 19:20:27 maya Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

static struct compose_tab_s {
	keysym_t elem[2];
	keysym_t result;
} compose_tab[] = {
#if 0
	{ { KS_plus,			KS_plus },		KS_numbersign },
	{ { KS_a,			KS_a },			KS_at },
	{ { KS_parenleft,		KS_parenleft },		KS_bracketleft },
	{ { KS_slash,			KS_slash },		KS_backslash },
	{ { KS_parenright,		KS_parenright },	KS_bracketright },
	{ { KS_parenleft,		KS_minus },		KS_braceleft },
	{ { KS_slash,			KS_minus },		KS_bar },
	{ { KS_parenright,		KS_minus },		KS_braceright },
	{ { KS_exclam,			KS_exclam },		KS_exclamdown },
	{ { KS_c,			KS_slash },		KS_cent },
	{ { KS_l,			KS_minus },		KS_sterling },
	{ { KS_y,			KS_minus },		KS_yen },
	{ { KS_s,			KS_o },			KS_section },
	{ { KS_x,			KS_o },			KS_currency },
	{ { KS_c,			KS_o },			KS_copyright },
	{ { KS_less,			KS_less },		KS_guillemotleft },
	{ { KS_greater,			KS_greater },		KS_guillemotright },
	{ { KS_question,		KS_question },		KS_questiondown },
#endif
	{ { KS_dead_acute,		KS_space },		KS_acute },
	{ { KS_dead_grave,		KS_space },		KS_grave },
	{ { KS_dead_tilde,		KS_space },		KS_asciitilde },
	{ { KS_dead_circumflex,		KS_space },		KS_asciicircum },
	{ { KS_dead_circumflex,		KS_A },			KS_Acircumflex },
	{ { KS_dead_diaeresis,		KS_A },			KS_Adiaeresis },
	{ { KS_dead_grave,		KS_A },			KS_Agrave },
	{ { KS_dead_abovering,		KS_A },			KS_Aring },
	{ { KS_dead_tilde,		KS_A },			KS_Atilde },
	{ { KS_dead_cedilla,		KS_C },			KS_Ccedilla },
	{ { KS_dead_acute,		KS_E },			KS_Eacute },
	{ { KS_dead_circumflex,		KS_E },			KS_Ecircumflex },
	{ { KS_dead_diaeresis,		KS_E },			KS_Ediaeresis },
	{ { KS_dead_grave,		KS_E },			KS_Egrave },
	{ { KS_dead_acute,		KS_I },			KS_Iacute },
	{ { KS_dead_circumflex,		KS_I },			KS_Icircumflex },
	{ { KS_dead_diaeresis,		KS_I },			KS_Idiaeresis },
	{ { KS_dead_grave,		KS_I },			KS_Igrave },
	{ { KS_dead_tilde,		KS_N },			KS_Ntilde },
	{ { KS_dead_acute,		KS_O },			KS_Oacute },
	{ { KS_dead_circumflex,		KS_O },			KS_Ocircumflex },
	{ { KS_dead_diaeresis,		KS_O },			KS_Odiaeresis },
	{ { KS_dead_grave,		KS_O },			KS_Ograve },
	{ { KS_dead_tilde,		KS_O },			KS_Otilde },
	{ { KS_dead_acute,		KS_U },			KS_Uacute },
	{ { KS_dead_circumflex,		KS_U },			KS_Ucircumflex },
	{ { KS_dead_diaeresis,		KS_U },			KS_Udiaeresis },
	{ { KS_dead_grave,		KS_U },			KS_Ugrave },
	{ { KS_dead_acute,		KS_Y },			KS_Yacute },
	{ { KS_dead_acute,		KS_a },			KS_aacute },
	{ { KS_dead_circumflex,		KS_a },			KS_acircumflex },
	{ { KS_dead_diaeresis,		KS_a },			KS_adiaeresis },
	{ { KS_dead_grave,		KS_a },			KS_agrave },
	{ { KS_dead_abovering,		KS_a },			KS_aring },
	{ { KS_dead_tilde,		KS_a },			KS_atilde },
	{ { KS_dead_cedilla,		KS_c },			KS_ccedilla },
	{ { KS_dead_acute,		KS_e },			KS_eacute },
	{ { KS_dead_circumflex,		KS_e },			KS_ecircumflex },
	{ { KS_dead_diaeresis,		KS_e },			KS_ediaeresis },
	{ { KS_dead_grave,		KS_e },			KS_egrave },
	{ { KS_dead_acute,		KS_i },			KS_iacute },
	{ { KS_dead_circumflex,		KS_i },			KS_icircumflex },
	{ { KS_dead_diaeresis,		KS_i },			KS_idiaeresis },
	{ { KS_dead_grave,		KS_i },			KS_igrave },
	{ { KS_dead_tilde,		KS_n },			KS_ntilde },
	{ { KS_dead_acute,		KS_o },			KS_oacute },
	{ { KS_dead_circumflex,		KS_o },			KS_ocircumflex },
	{ { KS_dead_diaeresis,		KS_o },			KS_odiaeresis },
	{ { KS_dead_grave,		KS_o },			KS_ograve },
	{ { KS_dead_tilde,		KS_o },			KS_otilde },
	{ { KS_dead_acute,		KS_u },			KS_uacute },
	{ { KS_dead_circumflex,		KS_u },			KS_ucircumflex },
	{ { KS_dead_diaeresis,		KS_u },			KS_udiaeresis },
	{ { KS_dead_grave,		KS_u },			KS_ugrave },
	{ { KS_dead_acute,		KS_y },			KS_yacute },
	{ { KS_dead_diaeresis,		KS_y },			KS_ydiaeresis },
#if 0
	{ { KS_quotedbl,		KS_A },			KS_Adiaeresis },
	{ { KS_quotedbl,		KS_E },			KS_Ediaeresis },
	{ { KS_quotedbl,		KS_I },			KS_Idiaeresis },
	{ { KS_quotedbl,		KS_O },			KS_Odiaeresis },
	{ { KS_quotedbl,		KS_U },			KS_Udiaeresis },
	{ { KS_quotedbl,		KS_a },			KS_adiaeresis },
	{ { KS_quotedbl,		KS_e },			KS_ediaeresis },
	{ { KS_quotedbl,		KS_i },			KS_idiaeresis },
	{ { KS_quotedbl,		KS_o },			KS_odiaeresis },
	{ { KS_quotedbl,		KS_u },			KS_udiaeresis },
	{ { KS_quotedbl,		KS_y },			KS_ydiaeresis },
	{ { KS_acute,			KS_A },			KS_Aacute },
	{ { KS_asciicircum,		KS_A },			KS_Acircumflex },
	{ { KS_grave,			KS_A },			KS_Agrave },
	{ { KS_asterisk,		KS_A },			KS_Aring },
	{ { KS_asciitilde,		KS_A },			KS_Atilde },
	{ { KS_cedilla,			KS_C },			KS_Ccedilla },
	{ { KS_acute,			KS_E },			KS_Eacute },
	{ { KS_asciicircum,		KS_E },			KS_Ecircumflex },
	{ { KS_grave,			KS_E },			KS_Egrave },
	{ { KS_acute,			KS_I },			KS_Iacute },
	{ { KS_asciicircum,		KS_I },			KS_Icircumflex },
	{ { KS_grave,			KS_I },			KS_Igrave },
	{ { KS_asciitilde,		KS_N },			KS_Ntilde },
	{ { KS_acute,			KS_O },			KS_Oacute },
	{ { KS_asciicircum,		KS_O },			KS_Ocircumflex },
	{ { KS_grave,			KS_O },			KS_Ograve },
	{ { KS_asciitilde,		KS_O },			KS_Otilde },
	{ { KS_acute,			KS_U },			KS_Uacute },
	{ { KS_asciicircum,		KS_U },			KS_Ucircumflex },
	{ { KS_grave,			KS_U },			KS_Ugrave },
	{ { KS_acute,			KS_Y },			KS_Yacute },
	{ { KS_acute,			KS_a },			KS_aacute },
	{ { KS_asciicircum,		KS_a },			KS_acircumflex },
	{ { KS_grave,			KS_a },			KS_agrave },
	{ { KS_asterisk,		KS_a },			KS_aring },
	{ { KS_asciitilde,		KS_a },			KS_atilde },
	{ { KS_cedilla,			KS_c },			KS_ccedilla },
	{ { KS_acute,			KS_e },			KS_eacute },
	{ { KS_asciicircum,		KS_e },			KS_ecircumflex },
	{ { KS_grave,			KS_e },			KS_egrave },
	{ { KS_acute,			KS_i },			KS_iacute },
	{ { KS_asciicircum,		KS_i },			KS_icircumflex },
	{ { KS_grave,			KS_i },			KS_igrave },
	{ { KS_asciitilde,		KS_n },			KS_ntilde },
	{ { KS_acute,			KS_o },			KS_oacute },
	{ { KS_asciicircum,		KS_o },			KS_ocircumflex },
	{ { KS_grave,			KS_o },			KS_ograve },
	{ { KS_asciitilde,		KS_o },			KS_otilde },
	{ { KS_acute,			KS_u },			KS_uacute },
	{ { KS_asciicircum,		KS_u },			KS_ucircumflex },
	{ { KS_grave,			KS_u },			KS_ugrave },
	{ { KS_acute,			KS_y },			KS_yacute },
#endif
	{ { KS_dead_semi,		KS_gr_A },		KS_gr_At  },
	{ { KS_dead_semi,		KS_gr_E },		KS_gr_Et  },
	{ { KS_dead_semi,		KS_gr_H },		KS_gr_Ht  },
	{ { KS_dead_semi,		KS_gr_I },		KS_gr_It  },
	{ { KS_dead_semi,		KS_gr_O },		KS_gr_Ot  },
	{ { KS_dead_semi,		KS_gr_Y },		KS_gr_Yt  },
	{ { KS_dead_semi,		KS_gr_V },		KS_gr_Vt  },
	{ { KS_dead_colon,		KS_gr_I },		KS_gr_Id  },
	{ { KS_dead_colon,		KS_gr_Y },		KS_gr_Yd  },
	{ { KS_dead_semi,		KS_gr_a },		KS_gr_at  },
	{ { KS_dead_semi,		KS_gr_e },		KS_gr_et  },
	{ { KS_dead_semi,		KS_gr_h },		KS_gr_ht  },
	{ { KS_dead_semi,		KS_gr_i },		KS_gr_it  },
	{ { KS_dead_semi,		KS_gr_o },		KS_gr_ot  },
	{ { KS_dead_semi,		KS_gr_y },		KS_gr_yt  },
	{ { KS_dead_semi,		KS_gr_v },		KS_gr_vt  },
	{ { KS_dead_colon,		KS_gr_i },		KS_gr_id  },
	{ { KS_dead_colon,		KS_gr_y },		KS_gr_yd  },

	/* Latin 2*/

	{ { KS_dead_acute,		KS_S },			KS_Sacute },
	{ { KS_dead_acute,		KS_Z },			KS_Zacute },
	{ { KS_dead_acute,		KS_s },			KS_sacute },
	{ { KS_dead_acute,		KS_z },			KS_zacute },
	{ { KS_dead_acute,		KS_R },			KS_Racute },
	{ { KS_dead_acute,		KS_A },			KS_Aacute },
	{ { KS_dead_acute,		KS_L },			KS_Lacute },
	{ { KS_dead_acute,		KS_C },			KS_Cacute },
	{ { KS_dead_acute,		KS_E },			KS_Eacute },
	{ { KS_dead_acute,		KS_I },			KS_Iacute },
	{ { KS_dead_acute,		KS_N },			KS_Nacute },
	{ { KS_dead_acute,		KS_O },			KS_Oacute },
	{ { KS_dead_acute,		KS_U },			KS_Uacute },
	{ { KS_dead_acute,		KS_Y },			KS_Yacute },
	{ { KS_dead_acute,		KS_r },			KS_racute },
	{ { KS_dead_acute,		KS_a },			KS_aacute },
	{ { KS_dead_acute,		KS_l },			KS_lacute },
	{ { KS_dead_acute,		KS_c },			KS_cacute },
	{ { KS_dead_acute,		KS_e },			KS_eacute },
	{ { KS_dead_acute,		KS_i },			KS_iacute },
	{ { KS_dead_acute,		KS_n },			KS_nacute },
	{ { KS_dead_acute,		KS_o },			KS_oacute },
	{ { KS_dead_acute,		KS_u },			KS_uacute },
	{ { KS_dead_acute,		KS_y },			KS_yacute },
	{ { KS_dead_breve,		KS_A },			KS_Abreve },
	{ { KS_dead_breve,		KS_a },			KS_abreve },
	{ { KS_dead_caron,		KS_L },			KS_Lcaron },
	{ { KS_dead_caron,		KS_S },			KS_Scaron },
	{ { KS_dead_caron,		KS_T },			KS_Tcaron },
	{ { KS_dead_caron,		KS_Z },			KS_Zcaron },
	{ { KS_dead_caron,		KS_l },			KS_lcaron },
	{ { KS_dead_caron,		KS_s },			KS_scaron },
	{ { KS_dead_caron,		KS_t },			KS_tcaron },
	{ { KS_dead_caron,		KS_z },			KS_zcaron },
	{ { KS_dead_caron,		KS_C },			KS_Ccaron },
	{ { KS_dead_caron,		KS_E },			KS_Ecaron },
	{ { KS_dead_caron,		KS_D },			KS_Dcaron },
	{ { KS_dead_caron,		KS_N },			KS_Ncaron },
	{ { KS_dead_caron,		KS_R },			KS_Rcaron },
	{ { KS_dead_caron,		KS_c },			KS_ccaron },
	{ { KS_dead_caron,		KS_e },			KS_ecaron },
	{ { KS_dead_caron,		KS_d },			KS_dcaron },
	{ { KS_dead_caron,		KS_n },			KS_ncaron },
	{ { KS_dead_caron,		KS_r },			KS_rcaron },
	{ { KS_dead_cedilla,		KS_S },			KS_Scedilla },
	{ { KS_dead_cedilla,		KS_s },			KS_scedilla },
	{ { KS_dead_cedilla,		KS_C },			KS_Ccedilla },
	{ { KS_dead_cedilla,		KS_T },			KS_Tcedilla },
	{ { KS_dead_cedilla,		KS_c },			KS_ccedilla },
	{ { KS_dead_cedilla,		KS_t },			KS_tcedilla },
	{ { KS_dead_circumflex,		KS_A },			KS_Acircumflex },
	{ { KS_dead_circumflex,		KS_I },			KS_Icircumflex },
	{ { KS_dead_circumflex,		KS_O },			KS_Ocircumflex },
	{ { KS_dead_circumflex,		KS_a },			KS_acircumflex },
	{ { KS_dead_circumflex,		KS_i },			KS_icircumflex },
	{ { KS_dead_circumflex,		KS_o },			KS_ocircumflex },
	{ { KS_dead_diaeresis,		KS_A },			KS_Adiaeresis },
	{ { KS_dead_diaeresis,		KS_E },			KS_Ediaeresis },
	{ { KS_dead_diaeresis,		KS_O },			KS_Odiaeresis },
	{ { KS_dead_diaeresis,		KS_U },			KS_Udiaeresis },
	{ { KS_dead_diaeresis,		KS_a },			KS_adiaeresis },
	{ { KS_dead_diaeresis,		KS_e },			KS_ediaeresis },
	{ { KS_dead_diaeresis,		KS_o },			KS_odiaeresis },
	{ { KS_dead_diaeresis,		KS_u },			KS_udiaeresis },
	{ { KS_dead_dotaccent,		KS_Z },			KS_Zabovedot },
	{ { KS_dead_dotaccent,		KS_z },			KS_zabovedot },
	{ { KS_dead_hungarumlaut,	KS_O },			KS_Odoubleacute },
	{ { KS_dead_hungarumlaut,	KS_U },			KS_Udoubleacute },
	{ { KS_dead_hungarumlaut,	KS_o },			KS_odoubleacute },
	{ { KS_dead_hungarumlaut,	KS_u },			KS_udoubleacute },
	{ { KS_dead_ogonek,		KS_A },			KS_Aogonek },
	{ { KS_dead_ogonek,		KS_a },			KS_aogonek },
	{ { KS_dead_ogonek,		KS_E },			KS_Eogonek },
	{ { KS_dead_ogonek,		KS_e },			KS_eogonek },
	{ { KS_dead_abovering,		KS_U },			KS_Uabovering },
	{ { KS_dead_abovering,		KS_u },			KS_uabovering },
	{ { KS_dead_slash,		KS_L },			KS_Lstroke },
	{ { KS_dead_slash,		KS_l },			KS_lstroke }
};

#define COMPOSE_SIZE	__arraycount(compose_tab)

static int compose_tab_inorder = 0;

static inline int
compose_tab_cmp(struct compose_tab_s *i, struct compose_tab_s *j)
{
	if (i->elem[0] == j->elem[0])
		return(i->elem[1] - j->elem[1]);
	else
		return(i->elem[0] - j->elem[0]);
}

static keysym_t
wskbd_compose_value(keysym_t *compose_buf)
{
	int i, j, r;
	struct compose_tab_s v;

	if (! compose_tab_inorder) {
		/* Insertion sort. */
		for (i = 1; i < COMPOSE_SIZE; i++) {
			v = compose_tab[i];
			/* find correct slot, moving others up */
			for (j = i; --j >= 0 && compose_tab_cmp(& v, & compose_tab[j]) < 0; )
				compose_tab[j + 1] = compose_tab[j];
			compose_tab[j + 1] = v;
		}
		compose_tab_inorder = 1;
	}

	for (j = 0, i = COMPOSE_SIZE; i != 0; i /= 2) {
		if (compose_tab[j + i/2].elem[0] == compose_buf[0]) {
			if (compose_tab[j + i/2].elem[1] == compose_buf[1])
				return(compose_tab[j + i/2].result);
			r = compose_tab[j + i/2].elem[1] < compose_buf[1];
		} else
			r = compose_tab[j + i/2].elem[0] < compose_buf[0];
		if (r) {
			j += i/2 + 1;
			i--;
		}
	}

	return(KS_voidSymbol);
}
