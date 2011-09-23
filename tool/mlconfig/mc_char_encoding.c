/*
 *	$Id$
 */

#include  "mc_char_encoding.h"
#include  <stdio.h>
#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <ctype.h>
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>
#include  "mc_combo.h"
#include  "mc_io.h"
#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int new_encoding_idx;  /* remember encoding as index in encodings[] */
static int old_encoding_idx;
static int is_changed;

static char *  encodings[] =
{
	N_("auto") ,

	N_("--- Unicode ---"),
	"UTF-8" ,

	N_("--- ISO 8859 encodings ---"),
	"ISO-8859-1 (Latin-1)" ,
	"ISO-8859-2 (Latin-2)" ,
	"ISO-8859-3 (Latin-3)" ,
	"ISO-8859-4 (Latin-4)" ,
	"ISO-8859-5 (Latin/Cyrillic)" ,
	"ISO-8859-6 (Latin/Arabic)" ,
	"ISO-8859-7 (Latin/Greek)" ,
	"ISO-8859-8 (Latin/Hebrew)" ,
	"ISO-8859-9 (Latin-5)" ,
	"ISO-8859-10 (Latin-6)" ,
	"ISO-8859-11 (TIS-620 Thai)" ,
	"ISO-8859-13 (Latin-7)" ,
	"ISO-8859-14 (Latin-8)" ,
	"ISO-8859-15 (Latin-9)" ,
	"ISO-8859-16 (Latin-10)" ,

	N_("--- Other 8bit ---"),
	"KOI8-R (Russian)" ,
	"KOI8-U (Ukrainian)" ,
	"KOI8-T (Tajik)" ,
	"GEORGIAN-PS (Georgian)" ,
	"TCVN5712 (Vietnamese)" ,
	"VISCII (Vietnamese)" ,
	"CP1250 (EastEurope)" ,
	"CP1251 (Bulgarian,Belarusian)" ,
	"CP1252 (Latin-1)" ,
	"CP1253 (Greek)" ,
	"CP1254 (Turkish)" ,
	"CP1255 (Hebrew)" ,
	"CP1256 (Arabic)" ,
	"CP1257 (Baltic)" ,
	"CP1258 (Vietnamese)" ,
	"ISCII-ASSAMESE (Indics)" ,
	"ISCII-BENGALI (Indics)" ,
	"ISCII-GUJARATI (Indics)" ,
	"ISCII-HINDI (Indics)" ,
	"ISCII-KANNAdA (Indics)" ,
	"ISCII-MALAYALAM (Indics)" ,
	"ISCII-ORIYA (Indics)" ,
	"ISCII-PUNJABI (Indics)" ,
	"ISCII-ROMAN (Indics)" ,
	"ISCII-TAMIL (Indics)" ,
	"ISCII-TELUGU (Indics)" ,

	N_("--- Japanese ---"),
	"EUC-JP" ,
	"EUC-JISX0213" ,
	"ISO-2022-JP" ,
	"ISO-2022-JP2" ,
	"ISO-2022-JP3" ,
	"SJIS" ,
	"SJISX0213" ,

	N_("--- Korean ---"),
	"EUC-KR" ,
	"UHC" ,
	"JOHAB" ,
	"ISO-2022-KR" ,

	N_("--- traditional Chinese ---"),
	"BIG-5" ,
	"EUC-TW" ,
	"BIG5HKSCS" ,

	N_("--- simplified Chinese ---"),
	"EUC-CN (GB2312)" ,
	"GBK" ,
	"GB18030" ,
	"HZ" ,
	"ISO-2022-CN" ,
	NULL
} ;

static char *encodings_l10n[sizeof(encodings)/sizeof(encodings[0])];

/* --- static functions --- */

/* translate combobox items (encodings)
 *   - the top item ("auto") and
 *   - items which start with "-" (explanations)
 */
static void prepare_encodings_l10n(void)
{
	int j;

	encodings_l10n[0] = strdup(_(encodings[0]));
	for(j=1; encodings[j]; j++) {
		if (encodings[j][0] == '-') {
			encodings_l10n[j] = strdup(_(encodings[j]));
			if (encodings_l10n[j][0] != '-') {
				free(encodings_l10n[j]);
				encodings_l10n[j] = encodings[j];
			}
		} else {
			encodings_l10n[j] = encodings[j];
		}
	}
	encodings_l10n[j] = NULL;
}

/* compare two encoding names, returns non-zero if equal
 */
static int compare(const char *e1, const char *e2)
{
	while(1) {
		if (*e1 == '_' || *e1 == '-') {e1++; continue;}
		if (*e2 == '_' || *e2 == '-') {e2++; continue;}
		if ((*e1 == 0 || *e1 == ' ') && (*e2 == 0 || *e2 == ' '))
			return 1;
		if (toupper(*e1) != toupper(*e2)) return 0;
		e1++; e2++;
	}
}

static int get_index(const char *encoding)
{
	int j;
	for(j=0; encodings[j]; j++)
		if (compare(encodings[j], encoding)) return j;

	/* Returns "auto" for unknown encoding names.
	 * Also, there is a possibility of translated "auto".
	 */
	return 0;
}

static char *savename(int index)
{
	static char buf[256];
	char *p;

	if (index == -1) return "UNKNOWN";

	strncpy(buf, encodings[index], 255);
	buf[255] = 0;
	p = strchr(buf, ' ');
	if (p) *p = 0;

	return buf;
}

static gint
encoding_selected(GtkWidget *widget, gpointer data)
{
	const char *p;

	p = gtk_entry_get_text(GTK_ENTRY(widget));
	if (*p == '-') return 1;
	new_encoding_idx = get_index(p);

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s encoding is selected.\n" , new_encoding) ;
#endif

	return 1;
}


/* -- global functions --- */

GtkWidget *
mc_char_encoding_config_widget_new(void)
{
	int isauto, idx;
	char *encoding;
	GtkWidget *widget;

	isauto = mc_get_flag_value("is_auto_encoding");
	encoding = mc_get_str_value("encoding");
	if (isauto) {
		static char autostr[256];

		idx = 0;
		sprintf(autostr, _("auto (currently %s)"),
			savename(get_index(encoding)));
		encodings[0] = autostr;
	} else
		idx = get_index(encoding);

	prepare_encodings_l10n();
	widget = mc_combo_new(_("Encoding"), encodings_l10n,
		sizeof(encodings) / sizeof(encodings[0]) - 1,
		encodings_l10n[idx], 1, encoding_selected, NULL);
	if (widget == NULL) return NULL;

	new_encoding_idx = old_encoding_idx = idx;
	is_changed = 0;

	return widget;
}

void
mc_update_char_encoding(void)
{
	if (new_encoding_idx != old_encoding_idx) is_changed = 1;

	if (is_changed) {
		mc_set_str_value("encoding" , savename(new_encoding_idx));
		old_encoding_idx = new_encoding_idx;
	}
}
