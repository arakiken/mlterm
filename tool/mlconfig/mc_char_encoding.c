/*
 *	$Id$
 */

#include  "mc_char_encoding.h"

#include  <string.h>
#include  <stdlib.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int new_encoding_idx;
static int old_encoding_idx;
static int is_changed;

static char *  encodings[] =
{
	"auto" ,
	
	"ISO-8859-1" ,
	"ISO-8859-2" ,
	"ISO-8859-3" ,
	"ISO-8859-4" ,
	"ISO-8859-5" ,
	"ISO-8859-6" ,
	"ISO-8859-7" ,
	"ISO-8859-8" ,
	"ISO-8859-9" ,
	"ISO-8859-10" ,
	"ISO-8859-11 (TIS-620)" ,
	"ISO-8859-13" ,
	"ISO-8859-14" ,
	"ISO-8859-15" ,
	"ISO-8859-16" ,
	"TCVN5712" ,

	"ISCII" ,
	"VISCII" ,
	"KOI8-R" ,
	"KOI8-U" ,

	"UTF-8" ,

	"EUC-JP" ,
	"EUC-JISX0213" ,
	"ISO-2022-JP" ,
	"ISO-2022-JP2" ,
	"ISO-2022-JP3" ,
	"SJIS" ,
	"SJISX0213" ,

	"EUC-KR" ,
	"UHC" ,
	"JOHAB" ,
	"ISO-2022-KR" ,

	"BIG-5" ,
	"EUC-TW" ,

	"BIG5HKSCS" ,

	"EUC-CN (GB2312)" ,
	"GBK" ,
	"GB18030" ,
	"HZ" ,

	"ISO-2022-CN" ,
	NULL
} ;


/* --- static functions --- */

/* compare two encoding names */
static int compare(char *e1, char *e2)
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

static int get_index(char *encoding)
{
	int j;
	for(j=0; encodings[j]; j++)
		if (compare(encodings[j], encoding)) return j;
	return -1;
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
	new_encoding_idx = get_index(gtk_entry_get_text(GTK_ENTRY(widget)));

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s encoding is selected.\n" , new_encoding) ;
#endif

	return 1;
}


/* -- global functions --- */

GtkWidget *
mc_char_encoding_config_widget_new(void)
{
	int idx;
	GtkWidget *widget;

	idx = get_index(mc_get_str_value("encoding"));

	widget = mc_combo_new(_("Encoding"), encodings,
		sizeof(encodings) / sizeof(encodings[0]) - 1,
		encodings[idx], 1, encoding_selected, NULL);
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
