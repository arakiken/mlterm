/*
 *	$Id$
 *
 *	Note that protocols except ssh aren't supported if USE_LIBSSH2 is defined.
 */

#include  "x_connect_dialog.h"

#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_path.h>


#if  defined(USE_WIN32API) || (defined(USE_WIN32GUI) && defined(USE_LIBSSH2))


/* --- static variables --- */

static int  selected_proto = -1 ;

static char **  server_list ;
static char *  default_server ;

/* These variables are set in IDOK. If empty string is input, nothing is set (==NULL). */
static char *  selected_server ;
static char *  selected_port ;
static char *  selected_user ;
static char *  selected_pass ;
static char *  selected_encoding ;


/* --- static functions --- */

/*
 * Parsing "<proto>://<user>@<host>:<port>:<encoding>".
 */
static int
parse(
	int *  protoid ,		/* If seq doesn't have proto, -1 is set. */
	char **  user ,			/* If seq doesn't have user, NULL is set. */
	char **  host ,
	char **  port ,
	char **  encoding ,		/* If seq doesn't have encoding, NULL is set. */
	char *  seq			/* broken in this function. */
	)
{
	char *  proto ;

	if( ! kik_parse_uri( &proto , user , host , port , NULL , encoding , seq))
	{
		return  0 ;
	}

	if( proto)
	{
		if( strcmp( proto , "ssh") == 0)
		{
			*protoid = IDD_SSH ;
		}
		else if( strcmp( proto , "telnet") == 0)
		{
			*protoid = IDD_TELNET ;
		}
		else if( strcmp( proto , "rlogin") == 0)
		{
			*protoid = IDD_RLOGIN ;
		}
		else
		{
			*protoid = -1 ;
		}
	}
	else
	{
		*protoid = -1 ;
	}

	return  1 ;
}

static char *
get_window_text(
	HWND  win
	)
{
	char *  p ;
	int  len ;

	if( ( len = GetWindowTextLength( win)) > 0 && ( p = malloc( len + 1)) )
	{
		if( GetWindowText( win , p , len + 1) > 0)
		{
			return  p ;
		}
		
		free( p) ;
	}

	return  NULL ;
}

LRESULT CALLBACK dialog_proc(
	HWND  dlgwin ,
	UINT  msg ,
	WPARAM  wparam ,
	LPARAM  lparam
	)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		{
			HWND  win ;
			HWND  focus_win ;
			char *  user_env ;

			focus_win = None ;
			win = GetDlgItem( dlgwin , IDD_LIST) ;
			
			if( server_list)
			{
				int  count ;

				for( count = 0 ; server_list[count] ; count++)
				{
					SendMessage( win , CB_ADDSTRING , 0 ,
						(LPARAM)server_list[count]) ;
				}

				if( count > 1)
				{
					focus_win = win ;
				}
			}
			else
			{
				EnableWindow( win , FALSE) ;
			}

			selected_proto = IDD_SSH ;
			user_env = getenv( "USERNAME") ;
			
			if( default_server)
			{
				LRESULT  res ;
				char *  user ;
				int  proto ;
				char *  server ;
				char *  port ;
				char *  encoding ;

				res = SendMessage( win , CB_FINDSTRINGEXACT , 0 ,
						(LPARAM)default_server) ;
				if( res != CB_ERR)
				{
					SendMessage( win , CB_SETCURSEL , res , 0) ;
				}

				if( parse( &proto , &user , &server , &port , &encoding ,
						kik_str_alloca_dup( default_server)) )
				{
					SetWindowText( GetDlgItem( dlgwin , IDD_SERVER) , server) ;

					if( port)
					{
						SetWindowText( GetDlgItem( dlgwin , IDD_PORT) ,
							port) ;
					}

					if( user || ( user = user_env))
					{
						SetWindowText( GetDlgItem( dlgwin , IDD_USER) ,
							user) ;
					}

				#ifndef  USE_LIBSSH2
					if( proto != -1)
					{
						selected_proto = proto ;
					}
				#endif

					if( encoding)
					{
						SetWindowText( GetDlgItem( dlgwin , IDD_ENCODING) ,
							encoding) ;
					}
				}
			}
			else if( user_env)
			{
				SetWindowText( GetDlgItem( dlgwin , IDD_USER) , user_env) ;
			}

		#ifdef  USE_LIBSSH2
			EnableWindow( GetDlgItem( dlgwin , IDD_TELNET) , FALSE) ;
			EnableWindow( GetDlgItem( dlgwin , IDD_RLOGIN) , FALSE) ;
			CheckRadioButton( dlgwin , IDD_SSH , IDD_RLOGIN , IDD_SSH) ;
		#else
			CheckRadioButton( dlgwin , IDD_SSH , IDD_RLOGIN , selected_proto) ;
		#endif

			if( focus_win)
			{
				SetFocus( focus_win) ;
			}
			else
			{
				SetFocus( GetDlgItem( dlgwin , selected_proto)) ;
			}

			return  FALSE ;
		}
		
	case WM_COMMAND:
		switch( LOWORD(wparam))
		{
		case  IDOK:
			{
				selected_server = get_window_text(
							GetDlgItem( dlgwin , IDD_SERVER)) ;
				selected_port = get_window_text(
							GetDlgItem( dlgwin , IDD_PORT)) ;
				selected_user = get_window_text(
							GetDlgItem( dlgwin , IDD_USER)) ;
				selected_pass = get_window_text(
							GetDlgItem( dlgwin , IDD_PASS)) ;
				selected_encoding = get_window_text(
							GetDlgItem( dlgwin , IDD_ENCODING)) ;
				
				EndDialog( dlgwin , IDOK) ;
				
				break ;
			}
			
		case  IDCANCEL:
			selected_proto = -1 ;
			EndDialog( dlgwin , IDCANCEL) ;
			
			break ;

		case  IDD_LIST:
			if( HIWORD(wparam) == CBN_SELCHANGE)
			{
				Window  win ;
				LRESULT  idx ;
				LRESULT  len ;
				char *  seq ;
				char *  user ;
				int  proto ;
				char *  server ;
				char *  port ;
				char *  encoding ;

				win = GetDlgItem( dlgwin , IDD_LIST) ;

				if( ( idx = SendMessage( win , CB_GETCURSEL , 0 , 0)) == CB_ERR ||
				    ( len = SendMessage( win , CB_GETLBTEXTLEN , idx , 0)) == CB_ERR ||
				    ( seq = alloca( len + 1)) == NULL ||
				    ( SendMessage( win , CB_GETLBTEXT , idx , seq)) == CB_ERR )
				{
					seq = NULL ;
				}

				if( seq &&
				    parse( &proto , &user , &server , &port , &encoding , seq))
				{
					SetWindowText( GetDlgItem( dlgwin , IDD_SERVER) , server) ;

					if( port)
					{
						SetWindowText( GetDlgItem( dlgwin , IDD_PORT) ,
							port) ;
					}
					
					if( user || ( user = getenv( "USERNAME")) || ( user = ""))
					{
						SetWindowText( GetDlgItem( dlgwin , IDD_USER) ,
							user) ;
					}

					if( proto == -1)
					{
						selected_proto = IDD_SSH ;
					}
					else
					{
						selected_proto = proto ;
					}
					
					if( encoding)
					{
						SetWindowText( GetDlgItem( dlgwin , IDD_ENCODING) ,
							encoding) ;
					}
					
					CheckRadioButton( dlgwin , IDD_SSH , IDD_RLOGIN ,
							selected_proto) ;
				}
			}

			break ;
			
		case  IDD_SSH:
		case  IDD_TELNET:
		case  IDD_RLOGIN:
			selected_proto = LOWORD(wparam) ;
			CheckRadioButton( dlgwin, IDD_SSH, IDD_RLOGIN, selected_proto) ;
			
			return  TRUE ;

		default:
			return  FALSE ;
		}
		
	default:
		return  FALSE;
	}
	
	return  TRUE ;
}


/* --- global functions --- */

int
x_connect_dialog(
	char **  uri ,		/* Should be free'ed by those who call this. */
	char **  pass ,		/* Same as above. If pass is not input, "" is set. */
	char *  display_name ,
	Window  parent_window ,
	char **  sv_list ,
	char *   def_server	/* (<user>@)(<proto>:)<server address>(:<encoding>). */
	)
{
	int  ret ;
	char *  proto ;

	server_list = sv_list ;
	default_server = def_server ;

#ifdef  DEBUG
	{
		char **  p ;
		kik_debug_printf( "DEFAULT server %s\n" , default_server) ;
		if( server_list)
		{
			kik_debug_printf( "SERVER LIST ") ;
			p = server_list ;
			while( *p)
			{
				kik_msg_printf( "%s " , *p) ;
				p ++ ;
			}
			kik_msg_printf( "\n") ;
		}
	}
#endif
	
	DialogBox( GetModuleHandle(NULL) , "ConnectDialog" , parent_window ,
		(DLGPROC)dialog_proc) ;

	ret = 0 ;
	
	if( selected_server == NULL)
	{
		goto  end ;
	}
	else if( selected_proto == IDD_SSH)
	{
		proto = "ssh://" ;
	}
	else if( selected_proto == IDD_TELNET)
	{
		proto = "telnet://" ;
	}
	else if( selected_proto == IDD_RLOGIN)
	{
		proto = "rlogin://" ;
	}
	else
	{
		goto  end ;
	}

	if( ! ( *uri = malloc( strlen(proto) +
			        (selected_user ? strlen(selected_user) + 1 : 0) +
			        strlen(selected_server) + 1 +
				(selected_port ? strlen(selected_port) + 1 : 0) +
			        (selected_encoding ? strlen(selected_encoding) + 1 : 0))) )
	{
		goto  end ;
	}

	(*uri)[0] = '\0' ;

	strcat( *uri , proto) ;

	if( selected_user)
	{
		strcat( *uri , selected_user) ;
		strcat( *uri , "@") ;
	}

	strcat( *uri , selected_server) ;

	if( selected_port)
	{
		strcat( *uri , ":") ;
		strcat( *uri , selected_port) ;
	}

	if( selected_encoding)
	{
		strcat( *uri , ":") ;
		strcat( *uri , selected_encoding) ;
	}

	*pass = selected_pass ? selected_pass : strdup( "") ;

	/* Successfully */
	ret = 1 ;

end:
	selected_proto = -1 ;
	
	server_list = NULL ;
	default_server = NULL ;
	
	free( selected_server) ;
	selected_server = NULL ;
	free( selected_port) ;
	selected_port = NULL ;
	free( selected_user) ;
	selected_user = NULL ;
	free( selected_encoding) ;
	selected_encoding = NULL ;

	if( ret == 0)
	{
		free( selected_pass) ;
		selected_pass = NULL ;
	}

	return  ret ;
}


#elif  defined(USE_LIBSSH2)


#include  <ctype.h>
#include  <kiklib/kik_util.h>
#include  <ml_pty.h>


#define  LINESPACE   10
#define  BEGENDSPACE  8

#define  CLEAR_DRAW   1
#define  DRAW         2
#define  DRAW_EXPOSE  3


int
x_connect_dialog(
	char **  uri ,		/* Should be free'ed by those who call this. */
	char **  pass ,		/* Same as above. If pass is not input, "" is set. */
	char *  display_name ,
	Window  parent_window ,
	char **  sv_list ,
	char *   def_server	/* (<user>@)(<proto>:)<server address>(:<encoding>). */
	)
{
	Display *  display ;
	int  screen ;
	Window  window ;
	GC  gc ;
	XFontStruct *  font ;
	u_int  width ;
	u_int  height ;
	u_int  ncolumns ;
	char *  title ;
	size_t  pass_len ;

	if( ! ( title = alloca( (ncolumns = 20 + strlen( def_server)))))
	{
		return  0 ;
	}
	sprintf( title , "Enter password for %s" , def_server) ;
	
	if( ! ( display = XOpenDisplay( display_name)))
	{
		return  0 ;
	}

	screen = DefaultScreen( display) ;
	gc = DefaultGC( display , screen) ;

	if( ! ( font = XLoadQueryFont( display , "-*-r-normal--*-*-*-*-c-*-iso8859-1")))
	{
		XCloseDisplay( display) ;

		return  0 ;
	}

	XSetFont( display , gc , font->fid) ;

	width = font->max_bounds.width * ncolumns + BEGENDSPACE ;
	height = (font->ascent + font->descent + LINESPACE) * 2 ;

	if( ! ( window = XCreateSimpleWindow( display ,
				DefaultRootWindow( display) ,
				(DisplayWidth( display , screen) - width) / 2 ,
				(DisplayHeight( display , screen) - height) / 2 ,
				width , height , 0 ,
				BlackPixel( display , screen) , WhitePixel( display , screen))))
	{
		XFreeFont( display , font) ;
		XCloseDisplay( display) ;

		return  0 ;
	}

	XStoreName( display , window , title) ;
	XSetIconName( display , window , title) ;
	XSelectInput( display , window , KeyReleaseMask|ExposureMask|StructureNotifyMask) ;
	XMapWindow( display , window) ;

	*pass = strdup( "") ;
	pass_len = 1 ;

	while( 1)
	{
		XEvent  ev ;
		int  redraw = 0 ;

		XWindowEvent( display , window ,
			KeyReleaseMask|ExposureMask|StructureNotifyMask , &ev) ;

		if( ev.type == KeyRelease)
		{
			char  buf[10] ;
			void *  p ;
			size_t  len ;

			if( ( len = XLookupString( &ev.xkey , buf , sizeof(buf) ,
							NULL , NULL)) > 0)
			{
				if( buf[0] == 0x08)	/* Backspace */
				{
					if( pass_len > 1)
					{
						(*pass)[--pass_len] = '\0' ;
						redraw = CLEAR_DRAW ;
					}
				}
				else if( isprint( (int)buf[0]))
				{
					if( ! ( p = realloc( *pass , (pass_len += len))))
					{
						break ;
					}

					memcpy( (*pass = p) + pass_len - len - 1 , buf , len) ;
					(*pass)[pass_len - 1] = '\0' ;

					redraw = DRAW ;
				}
				else
				{
					/* Exit loop */
					break ;
				}
			}
		}
		else if( ev.type == Expose)
		{
			redraw = DRAW_EXPOSE ;
		}
		else if( ev.type == MapNotify)
		{
			XSetInputFocus( display , window , RevertToPointerRoot , CurrentTime) ;
		}

		if( redraw)
		{
			XPoint  points[5] =
			{
				{ BEGENDSPACE / 2 , font->ascent + font->descent + LINESPACE } ,
				{ width - BEGENDSPACE / 2 ,
					font->ascent + font->descent + LINESPACE } ,
				{ width - BEGENDSPACE / 2 ,
					(font->ascent + font->descent) * 2 + LINESPACE * 3 / 2} ,
				{ BEGENDSPACE / 2 ,
					(font->ascent + font->descent) * 2 + LINESPACE * 3 / 2} ,
				{ BEGENDSPACE / 2 ,
					font->ascent + font->descent + LINESPACE } ,
			} ;

			if( redraw == DRAW_EXPOSE)
			{
				XDrawString( display , window , gc ,
					BEGENDSPACE / 2 , font->ascent + LINESPACE / 2 ,
					title , strlen(title)) ;

				XDrawLines( display , window , gc , points , 5 , CoordModeOrigin) ;
			}
			else  if( redraw == CLEAR_DRAW)
			{
				XClearArea( display , window , points[0].x + 1 , points[0].y + 1 ,
					points[2].x - points[0].x - 1 ,
					points[2].y - points[0].y - 1 , False) ;
			}
			
			if( *pass)
			{
				char *  input ;
				size_t  count ;

				if( ! ( input = alloca( pass_len - 1)))
				{
					break ;
				}

				for( count = 0 ; count < pass_len - 1 ; count++)
				{
					input[count] = '*' ;
				}

				XDrawString( display , window , gc ,
					BEGENDSPACE / 2 + font->max_bounds.width / 2 ,
					font->ascent * 2 + font->descent + LINESPACE * 3 / 2 ,
					input , K_MIN(pass_len - 1,ncolumns - 1)) ;
			}
		}
	}

	XDestroyWindow( display , window) ;
	XFreeFont( display , font) ;
	XCloseDisplay( display) ;

	*uri = strdup( def_server) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Connecting to %s %s\n" , *uri , *pass) ;
#endif

	return  1 ;
}


#endif
