/*
 *	$Id$
 */

#include  "mc_char_encoding.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_encoding ;
static char *  old_encoding ;

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

static char *
regularize(
	char *  encoding
	)
{
	if( strcmp( encoding , "ISO-8859-11 (TIS-620)") == 0)
	{
		return  "ISO-8859-11" ;
	}
	else
	{
		return  encoding ;
	}
}

static char *
unregularize(
	char *  encoding
	)
{
	char *  regularized_encodings[] =
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

	int  count ;

	for( count = 0 ; count < sizeof( regularized_encodings) / sizeof( regularized_encodings[0]) ;
		count ++)
	{
		if( strcmp( regularized_encodings[count] , encoding) == 0)
		{
			return  encodings[count] ;
		}
	}

	return  "UNKNOWN" ;
}

static gint
encoding_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_encoding = regularize( gtk_entry_get_text(GTK_ENTRY(widget))) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s encoding is selected.\n" , new_encoding) ;
#endif

	return  1 ;
}


/* -- global functions --- */

GtkWidget *
mc_char_encoding_config_widget_new(void)
{
	char *  encoding ;
	GtkWidget *  widget ;

	encoding = unregularize( mc_get_str_value( "encoding")) ;

	if( ( widget = mc_combo_new(_("Encoding"), encodings,
			sizeof(encodings) / sizeof(encodings[0]),
			encoding, 1, encoding_selected, NULL)) == NULL)
	{
		return  NULL ;
	}

	new_encoding = old_encoding = regularize( encoding) ;

	return  widget ;
}

void
mc_update_char_encoding(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "encoding" , new_encoding , save) ;
	}
	else
	{
		if( strcmp( new_encoding , old_encoding) != 0)
		{
			mc_set_str_value( "encoding" , new_encoding , save) ;
			old_encoding = new_encoding ;
		}
	}
}
