/*
 *	$Id$
 */

#include  "mc_char_encoding.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_encoding ;
static int  is_changed ;

static char *  encodings[] =
{
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

	"EUC-CN" ,
	"GBK" ,
	"GB18030" ,
	"HZ" ,

	"ISO-2022-CN" ,
} ;


/* --- static functions --- */

static gint
encoding_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_encoding = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	is_changed = 1 ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s encoding is selected.\n" , selected_encoding) ;
#endif

	return  1 ;
}

static char *
unregularized(
	char *  encoding
	)
{
	int  count ;
	char *  regularized[] =
	{
		"ISO88591" ,
		"ISO88592" ,
		"ISO88593" ,
		"ISO88594" ,
		"ISO88595" ,
		"ISO88596" ,
		"ISO88597" ,
		"ISO88598" ,
		"ISO88599" ,
		"ISO885910" ,
		"ISO885911" ,
		"ISO885913" ,
		"ISO885914" ,
		"ISO885915" ,
		"ISO885916" ,
		"TCVN5712" ,

		"ISCII" ,
		"VISCII" ,
		"KOI8R" ,
		"KOI8U" ,

		"UTF8" ,

		"EUCJP" ,
		"EUCJISX0213" ,
		"ISO2022JP" ,
		"ISO2022JP2" ,
		"ISO2022JP3" ,
		"SJIS" ,
		"SJISX0213" ,

		"EUCKR" ,
		"UHC" ,
		"JOHAB" ,
		"ISO2022KR" ,

		"BIG5" ,
		"EUCTW" ,

		"BIG5HKSCS" ,

		"EUCCN" ,
		"GBK" ,
		"GB18030" ,
		"HZ" ,

		"ISO2022CN" ,
	} ;

	for( count = 0 ; count < sizeof( regularized) / sizeof( regularized[0]) ; count ++)
	{
		if( strcmp( regularized[count] , encoding) == 0)
		{
			return  encodings[count] ;
		}
	}

	return  "UNKNOWN" ;
}


/* -- global functions --- */

GtkWidget *
mc_char_encoding_config_widget_new(
	char *  encoding
	)
{
	selected_encoding = unregularized( encoding) ;

	return  mc_combo_new(_("Encoding"), encodings,
			     sizeof(encodings) / sizeof(encodings[0]),
			     selected_encoding, 1, encoding_selected, NULL);
}

char *
mc_get_char_encoding(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  selected_encoding ;
}
