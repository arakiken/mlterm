/*
 *	update: <2001/11/26(22:51:49)>
 *	$Id$
 */

#include  "mc_encoding.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

/*
 * !!! Notice !!!
 * the order should be the same as ml_encoding_type_t in ml_encoding.h
 */
static char *  encodings[] =
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
	"TIS620" ,
	"ISO885913" ,
	"ISO885914" ,
	"ISO885915" ,
	"ISO885916" ,
	"TCVN5712" ,

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

	"EUCCN" ,
	"GBK" ,
	"GB18030" ,
	"HZ" ,

	"ISO2022CN" ,
} ;

static ml_encoding_type_t  selected_encoding = 0 ;


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
mc_encoding_config_widget_new(
	ml_encoding_type_t  encoding
	)
{
	selected_encoding = encoding ;

	return  mc_combo_new( "Encoding" , encodings , sizeof(encodings) / sizeof(encodings[0]) ,
		encodings[selected_encoding] , 1 , encoding_selected , NULL) ;
}

ml_encoding_type_t
mc_get_encoding(void)
{
	return  selected_encoding ;
}
