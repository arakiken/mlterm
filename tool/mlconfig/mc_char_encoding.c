/*
 *	$Id$
 */

#include  "mc_char_encoding.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

/*
 * !!! Notice !!!
 * the order should be the same as ml_char_encoding_t in ml_char_encoding.h
 */
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

static ml_char_encoding_t  selected_encoding = 0 ;


/* --- static functions --- */

static gint
encoding_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	int  counter ;
	char *  text ;

	text = gtk_entry_get_text(GTK_ENTRY(widget)) ;

	for( counter = 0 ; counter < sizeof( encodings) / sizeof( encodings[0]) ; counter ++)
	{
		if( strcmp( text , encodings[counter]) == 0)
		{
			break ;
		}
	}
	
	selected_encoding = counter ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s encoding is selected.\n" , selected_encoding) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_char_encoding_config_widget_new(
	ml_char_encoding_t  encoding
	)
{
	selected_encoding = encoding ;

	return  mc_combo_new( "Encoding" , encodings , sizeof(encodings) / sizeof(encodings[0]) ,
		encodings[selected_encoding] , 1 , encoding_selected , NULL) ;
}

ml_char_encoding_t
mc_get_char_encoding(void)
{
	return  selected_encoding ;
}
