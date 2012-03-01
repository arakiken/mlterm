/*
 *	$Id$
 */

#include  <kiklib/kik_config.h>	/* HAVE_WINDOWS_H, WORDS_BIGENDIAN */
#ifdef  HAVE_WINDOWS_H
#include  <windows.h>	/* In Cygwin <windows.h> is not included and error happens in jni.h. */
#endif

#include  "mlterm_MLTermPty.h"

#include  <string.h>		/* strlen */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <mkf/mkf_utf16_conv.h>
#include  <ml_str_parser.h>
#include  <ml_term_manager.h>


#if  1
#define  __DEBUG
#endif


typedef struct  native_obj
{
	ml_term_t *  term ;
	ml_pty_event_listener_t   pty_listener ;
	ml_config_event_listener_t  config_listener ;

} native_obj_t ;


/* --- static variables --- */

static mkf_parser_t *  str_parser ;
static mkf_parser_t *  utf8_parser ;
static mkf_conv_t *  utf16_conv ;


/* --- static functions --- */

static void
pty_closed(
	void *  p
	)
{
	((native_obj_t *)p)->term = NULL ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " PTY CLOSED\n") ;
#endif
}


static void
set_config(
	void *  p ,
	char *  dev ,
	char *  key ,
	char *  value
	)
{
	native_obj_t *  nativeObj ;

	nativeObj = p ;

	if( strcmp( key , "encoding") == 0)
	{
		ml_char_encoding_t   encoding ;

		if( ( encoding = ml_get_char_encoding( value)) != ML_UNKNOWN_ENCODING)
		{
			ml_term_set_auto_encoding( nativeObj->term ,
				strcasecmp( value , "auto") == 0 ? 1 : 0) ;

			ml_term_change_encoding( nativeObj->term , encoding) ;
		}
	}
}


/*
 * 2: New style should be added.
 * 1: Previous style is continued.
 * 0: No style.
 */
static int
need_style(
	ml_char_t *  ch ,
	ml_char_t *  prev_ch
	)
{
	int  need_style ;

	if( ml_char_fg_color( ch) != ML_FG_COLOR ||
	    ml_char_bg_color( ch) != ML_BG_COLOR ||
	    ml_char_is_underlined( ch) ||
	    (ml_char_font( ch) & FONT_BOLD) )
	{
		need_style = 2 ;
	}
	else
	{
		need_style = 0 ;
	}

	if( prev_ch &&
	    ml_char_fg_color( ch) == ml_char_fg_color( prev_ch) &&
	    ml_char_bg_color( ch) == ml_char_bg_color( prev_ch) &&
	    ml_char_is_underlined( ch) == ml_char_is_underlined( prev_ch) &&
	    (ml_char_font( ch) & ml_char_font( prev_ch) & FONT_BOLD) )
	{
		if( need_style)
		{
			/* Continual style */
			return  1 ;
		}
	}

	return  need_style ;
}


/* --- global functions --- */

JNIEXPORT jlong JNICALL
Java_mlterm_MLTermPty_nativeOpen(
	JNIEnv *  env ,
	jobject  obj ,
	jstring  jstr_host ,
	jstring  jstr_pass ,	/* can be NULL */
	jint  cols ,
	jint  rows
	)
{
	char *  argv[2] ;
	native_obj_t *  nativeObj ;
	char *  host ;
	char *  pass ;

#if  defined(USE_WIN32API) && defined(USE_LIBSSH2)
	static int  wsa_inited = 0 ;

	if( ! wsa_inited)
	{
		WSADATA  wsadata ;
		WSAStartup( MAKEWORD(2,0) , &wsadata) ;
		wsa_inited = 1 ;
	}
#endif

	if( ! str_parser)
	{
		kik_sig_child_init() ;
		ml_term_manager_init(1) ;

		str_parser = ml_str_parser_new() ;
		utf8_parser = ml_parser_new( ML_UTF8) ;
	#ifdef  WORDS_BIGENDIAN
		utf16_conv = mkf_utf16_conv_new() ;
	#else
		utf16_conv = mkf_utf16le_conv_new() ;
	#endif
	}

	if( ! ( nativeObj = calloc( sizeof( native_obj_t) , 1)))
	{
		return  0 ;
	}

	if( ! ( nativeObj->term = ml_create_term( cols , rows , 8 , 0 , ML_UTF8 , 1 , 0 , 1 ,
					1 , 1 , 1 , 0 , 1 , 1 , 0 , BSM_STATIC , 0)))
	{
		goto  error ;
	}

#ifdef  USE_WIN32API
	argv[0] = NULL ;
#else	/* USE_WIN32API */
	argv[0] = getenv( "SHELL") ;
	argv[1] = NULL ;
#endif	/* USE_WIN32API */

	nativeObj->pty_listener.self = nativeObj ;
	nativeObj->pty_listener.closed = pty_closed ;

	nativeObj->config_listener.self = nativeObj ;
	nativeObj->config_listener.set = set_config ;

	ml_term_attach( nativeObj->term , NULL , &nativeObj->config_listener ,
		NULL , &nativeObj->pty_listener) ;

	host = (*env)->GetStringUTFChars( env , jstr_host , NULL) ;
	if( jstr_pass)
	{
		pass = (*env)->GetStringUTFChars( env , jstr_pass , NULL) ;
	}
	else
	{
		pass = NULL ;
	}

	if( ml_term_open_pty( nativeObj->term , argv[0] , argv , NULL ,
		host , pass , NULL , NULL))
	{
		return  nativeObj ;
	}

	(*env)->ReleaseStringUTFChars( env , jstr_host , host) ;
	if( pass)
	{
		(*env)->ReleaseStringUTFChars( env , jstr_pass , pass) ;
	}

error:
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Failed to open pty.\n") ;
#endif
	free( nativeObj) ;

	return  0 ;
}

JNIEXPORT void JNICALL
Java_mlterm_MLTermPty_nativeClose(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj
	)
{
	if( nativeObj && ((native_obj_t*)nativeObj)->term)
	{
		ml_destroy_term( ((native_obj_t*)nativeObj)->term) ;
	}

	free( (native_obj_t*)nativeObj) ;
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeIsActive(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj
	)
{
	ml_close_dead_terms() ;

	if( nativeObj && ((native_obj_t*)nativeObj)->term)
	{
		return  JNI_TRUE ;
	}
	else
	{
		return  JNI_FALSE ;
	}
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeRead(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj
	)
{
	if( nativeObj && ((native_obj_t*)nativeObj)->term &&
	    ml_term_parse_vt100_sequence( ((native_obj_t*)nativeObj)->term))
	{
		return  JNI_TRUE ;
	}
	else
	{
		return  JNI_FALSE ;
	}
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeWrite(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj ,
	jstring  jstr
	)
{
	char *  str ;
	u_char  buf[128] ;
	size_t  len ;

	if( ! nativeObj || ! ((native_obj_t*)nativeObj)->term)
	{
		return  JNI_FALSE ;
	}

	str = (*env)->GetStringUTFChars( env , jstr , NULL) ;
#if  0
	kik_debug_printf( "WRITE TO PTY: %s" , str) ;
#endif
	(*utf8_parser->init)( utf8_parser) ;
	(*utf8_parser->set_str)( utf8_parser , str , strlen( str)) ;
	while( ! utf8_parser->is_eos &&
	       ( len = ml_term_convert_to( ((native_obj_t*)nativeObj)->term ,
				buf , sizeof(buf) , utf8_parser)) > 0)
	{
		ml_term_write( ((native_obj_t*)nativeObj)->term , buf , len , 0) ;
	}
#if  0
	kik_debug_printf( " => DONE\n") ;
#endif
	(*env)->ReleaseStringUTFChars( env , jstr , str) ;

	return  JNI_TRUE ;
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeGetRedrawString(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj ,
	jint  row ,
	jobject  region
	)
{
	ml_line_t *  line ;
	char *  buf ;
	size_t  buf_len ;
	int  mod_beg ;
	u_int  num_of_chars ;
	u_int  count ;
	u_int  start ;
	size_t  redraw_len ;
	jobject *  styles ;
	u_int  num_of_styles ;
	static jfieldID  region_str ;
	static jfieldID  region_start ;
	static jfieldID  region_styles ;
	static jclass  style_class ;
	static jfieldID  style_length ;
	static jfieldID  style_start ;
	static jfieldID  style_fg_color ;
	static jfieldID  style_fg_pixel ;
	static jfieldID  style_bg_color ;
	static jfieldID  style_bg_pixel ;
	static jfieldID  style_underline ;
	static jfieldID  style_bold ;
	jobjectArray  array ;

	if( ! nativeObj || ! ((native_obj_t*)nativeObj)->term ||
	    ! ( line = ml_term_get_line_in_screen( ((native_obj_t*)nativeObj)->term , row)) ||
	    ! ml_line_is_modified( line))
	{
		return  JNI_FALSE ;
	}

	if( ! region_str)
	{
		jclass  class ;

		class = (*env)->FindClass( env , "mlterm/RedrawRegion") ;
		region_str = (*env)->GetFieldID( env , class , "str" , "Ljava/lang/String;") ;
		region_start = (*env)->GetFieldID( env , class , "start" , "I") ;
		region_styles = (*env)->GetFieldID( env , class , "styles" , "[Lmlterm/Style;") ;
	}

	mod_beg = ml_line_get_beg_of_modified( line) ;
	num_of_chars = ml_line_get_num_of_redrawn_chars( line , 1) ;

	buf_len = (mod_beg + num_of_chars) * sizeof(int16_t) * 2 /* SURROGATE_PAIR */
			+ 2 /* NULL */ ;
	buf = alloca( buf_len) ;

	num_of_styles = 0 ;

	if( num_of_chars > 0)
	{
		if( mod_beg > 0)
		{
			(*str_parser->init)( str_parser) ;
			ml_str_parser_set_str( str_parser , line->chars , mod_beg) ;
			start = (*utf16_conv->convert)( utf16_conv ,
					buf , buf_len , str_parser) / 2 ;
		}
		else
		{
			start = 0 ;
		}

		styles = alloca( sizeof(jobject) * num_of_chars) ;
		redraw_len = 0 ;
		(*str_parser->init)( str_parser) ;
		for( count = 0 ; count < num_of_chars ; count++)
		{
			size_t  len ;
			int  ret ;

			if( ml_char_bytes_equal( line->chars + mod_beg + count , ml_nl_ch()))
			{
				/* Drawing will collapse, but region.str mustn't contain '\n'. */
				continue ;
			}

			ml_str_parser_set_str( str_parser , line->chars + mod_beg + count , 1) ;
			if( ( len = (*utf16_conv->convert)( utf16_conv , buf + redraw_len ,
						buf_len - redraw_len , str_parser)) < 2)
			{
				continue ;
			}

			ret = need_style( line->chars + mod_beg + count ,
					count > 0 ? line->chars + mod_beg + count - 1 : NULL) ;
			if( ret == 1)
			{
				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
						style_length ,
						(*env)->GetIntField( env ,
							styles[num_of_styles - 1] , style_length)
							+ len / 2) ;
			}
			else if( ret == 2)
			{
				ml_color_t  color ;
				u_int8_t  red ;
				u_int8_t  green ;
				u_int8_t  blue ;

				if( ! style_class)
				{
					style_class = (*env)->NewGlobalRef( env ,
							(*env)->FindClass( env , "mlterm/Style")) ;
					style_length = (*env)->GetFieldID( env , style_class ,
								"length" , "I") ;
					style_start = (*env)->GetFieldID( env , style_class ,
								"start" , "I") ;
					style_fg_color = (*env)->GetFieldID( env , style_class ,
								"fg_color" , "I") ;
					style_fg_pixel = (*env)->GetFieldID( env , style_class ,
								"fg_pixel" , "I") ;
					style_bg_color = (*env)->GetFieldID( env , style_class ,
								"bg_color" , "I") ;
					style_bg_pixel = (*env)->GetFieldID( env , style_class ,
								"bg_pixel" , "I") ;
					style_underline = (*env)->GetFieldID( env , style_class ,
								"underline" , "Z") ;
					style_bold = (*env)->GetFieldID( env , style_class ,
								"bold" , "Z") ;
				}

				styles[num_of_styles++] = (*env)->AllocObject( env , style_class) ;

				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_length , len / 2) ;

				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_start , redraw_len / 2) ;

				color = ml_char_fg_color( line->chars + mod_beg + count) ;
				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_fg_color , color) ;
				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_fg_pixel ,
					ml_get_color_rgb( color , &red , &green , &blue) ?
						((red << 16) | (green << 8) | blue) : -1) ;

				color = ml_char_bg_color( line->chars + mod_beg + count) ;
				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_bg_color , color) ;
				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_bg_pixel ,
					ml_get_color_rgb( color , &red , &green , &blue) ?
						((red << 16) | (green << 8) | blue) : -1) ;

				(*env)->SetBooleanField( env , styles[num_of_styles - 1] ,
					style_underline ,
					ml_char_is_underlined( line->chars + mod_beg + count) ?
						JNI_TRUE : JNI_FALSE) ;

				(*env)->SetBooleanField( env , styles[num_of_styles - 1] ,
					style_bold ,
					ml_char_font( line->chars + mod_beg + count) & FONT_BOLD ?
						JNI_TRUE : JNI_FALSE) ;
			}

			redraw_len += len ;
		}
	}
	else
	{
		start = 0 ;
		redraw_len = 0 ;
	}

#ifdef  WORDS_BIGENDIAN
	buf[redraw_len++] = '\0' ;
	buf[redraw_len++] = '\n' ;
#else
	buf[redraw_len++] = '\n' ;
	buf[redraw_len++] = '\0' ;
#endif

	(*env)->SetObjectField( env , region , region_str ,
			((*env)->NewString)( env , buf , redraw_len / 2)) ;
	(*env)->SetIntField( env , region , region_start , start) ;

	if( num_of_styles > 0)
	{
		u_int  count ;

		array = (*env)->NewObjectArray( env , num_of_styles , style_class , styles[0]) ;
		for( count = 1 ; count < num_of_styles ; count++)
		{
			(*env)->SetObjectArrayElement( env , array , count , styles[count]) ;
		}
	}
	else
	{
		array = NULL ;
	}

	(*env)->SetObjectField( env , region , region_styles , array) ;

	ml_line_set_updated( line) ;

	return  JNI_TRUE ;
}

JNIEXPORT jint JNICALL
Java_mlterm_MLTermPty_nativeGetCaretRow(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj
	)
{
	if( ! nativeObj || ! ((native_obj_t*)nativeObj)->term)
	{
		return  0 ;
	}

	return  ml_term_cursor_row_in_screen( ((native_obj_t*)nativeObj)->term) ;
}

JNIEXPORT jint JNICALL
Java_mlterm_MLTermPty_nativeGetCaretCol(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj
	)
{
	ml_line_t *  line ;
	int  char_index ;

	if( ! nativeObj || ! ((native_obj_t*)nativeObj)->term ||
	    ! ( line = ml_term_get_cursor_line( ((native_obj_t*)nativeObj)->term)))
	{
		return  0 ;
	}

	char_index = ml_term_cursor_char_index( ((native_obj_t*)nativeObj)->term) ;

	if( char_index > 0)
	{
		char *  buf ;
		size_t  buf_len ;

		buf_len = char_index * sizeof(int16_t) * 2 /* SURROGATE_PAIR */ ;
		buf = alloca( buf_len) ;

		(*str_parser->init)( str_parser) ;
		ml_str_parser_set_str( str_parser , line->chars , char_index) ;

		return  (*utf16_conv->convert)( utf16_conv ,
				buf , buf_len , str_parser) / 2 ;
	}
	else
	{
		return  0 ;
	}
}
