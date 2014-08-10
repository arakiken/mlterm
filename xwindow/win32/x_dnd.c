/*
 * $Id$
 */

#ifndef  DISABLE_XDND

#include  "../x_window.h"
#include  "../x_dnd.h"

#include  <kiklib/kik_def.h>	/* USE_WIN32API */
#if  defined(__CYGWIN__) || defined(__MSYS__)
#include  <kiklib/kik_path.h>	/* cygwin_conv_to_posix_path */
#endif

#ifndef  USE_WIN32API
#include  <mkf/mkf_utf8_conv.h>
#include  <mkf/mkf_utf8_parser.h>
#include  <mkf/mkf_utf16_conv.h>
#include  <mkf/mkf_utf16_parser.h>
#endif


/* --- static functions --- */

static size_t
conv_utf16_to_utf8(
	u_char *  dst ,
	size_t  dst_len ,
	u_char *  src ,
	size_t  src_len
	)
{
	size_t  conv_len ;
	mkf_parser_t *  utf16_parser ;
	mkf_conv_t *  utf8_conv ;

	utf16_parser = mkf_utf16le_parser_new() ;
	utf8_conv = mkf_utf8_conv_new() ;

	(*utf16_parser->init)( utf16_parser) ;
	(*utf16_parser->set_str)( utf16_parser , src , src_len) ;
	(*utf8_conv->init)( utf8_conv) ;

	if( ( conv_len = (*utf8_conv->convert)( utf8_conv , dst ,
				dst_len , utf16_parser)) == dst_len)
	{
		conv_len -- ;
	}

	dst[conv_len] = '\0' ;

	(*utf16_parser->delete)( utf16_parser) ;
	(*utf8_conv->delete)( utf8_conv) ;

	return  conv_len ;
}


/* --- global functions --- */

/*
 * XFilterEvent(event, w) analogue.
 * return 0 if the event should be processed in the mlterm mail loop.
 * return 1 if nothing to be done is left for the event.
 */
int
x_dnd_filter_event(
	XEvent *  event ,
	x_window_t *  win
	)
{
	HDROP  drop ;
	UINT  num ;
	int  count ;
	int  do_scp ;
#ifndef  USE_WIN32API
	mkf_conv_t *  utf16_conv ;
	mkf_parser_t *  utf8_parser ;
#endif

	if( event->msg != WM_DROPFILES)
	{
		return  0 ;
	}

	/* Shift+DnD => SCP */
	do_scp = GetKeyState(VK_SHIFT) ;

#ifndef  USE_WIN32API
	utf8_parser = mkf_utf8_parser_new() ;
	utf16_conv = mkf_utf16le_conv_new() ;
#endif

	drop = (HDROP)event->wparam ;
	num = DragQueryFile( drop , 0xffffffff , NULL , 0) ;
	for( count = 0 ; count < num ; count ++)
	{
		WCHAR  utf16_path[MAX_PATH] ;
		
		if( ( num = DragQueryFileW( drop , count , utf16_path ,
					sizeof(utf16_path) / sizeof(utf16_path[0]))) > 0)
		{
			size_t  path_len ;
			u_char  utf8_path[MAX_PATH] ;

		#ifdef  USE_WIN32API
			if( do_scp)
			{
				if( win->set_xdnd_config &&
				    conv_utf16_to_utf8( utf8_path , sizeof(utf8_path) ,
					utf16_path , num * sizeof(utf16_path[0])) > 0)
				{
					(*win->set_xdnd_config)( win , NULL ,
						"scp" , utf8_path) ;
				}
			}
			else
			{
				path_len = num * sizeof(utf16_path[0]) ;

				if( win->utf_selection_notified)
				{
					(*win->utf_selection_notified)( win ,
						(u_char*)utf16_path , path_len) ;
				}
			}
		#else
			u_char  posix_path[MAX_PATH] ;

			if( conv_utf16_to_utf8( utf8_path , sizeof(utf8_path) ,
				utf16_path , num * sizeof(utf16_path[0])) == 0)
			{
				continue ;
			}

			cygwin_conv_to_posix_path( utf8_path , posix_path) ;

			if( do_scp)
			{
				if( win->set_xdnd_config)
				{
					(*win->set_xdnd_config)( win , NULL ,
						"scp" , posix_path) ;
				}
			}
			else if( win->utf_selection_notified)
			{
				(*utf8_parser->init)( utf8_parser) ;
				(*utf8_parser->set_str)( utf8_parser , posix_path ,
					strlen(posix_path) + 1) ;
				(*utf16_conv->init)( utf16_conv) ;

				if( ( path_len = (*utf16_conv->convert)( utf16_conv ,
							(u_char*)utf16_path ,
							sizeof(utf16_path) , utf8_parser)) > 0)
				{
					(*win->utf_selection_notified)( win ,
						(u_char*)utf16_path , path_len) ;
				}
			}
		#endif
		}
	}

#ifndef  USE_WIN32API
	(*utf8_parser->delete)( utf8_parser) ;
	(*utf16_conv->delete)( utf16_conv) ;
#endif

	DragFinish( drop) ;

	return  1 ;
}

#endif	/* DISABLE_XDND */
