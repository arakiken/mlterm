/*
 *	$Id$
 */

#include  "x_connect_dialog.h"


#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_debug.h>


/* --- static variables --- */

static int  selected_proto = -1 ;

static char **  server_list ;
static char *  default_server ;
static char *  selected_server ;

static char *  selected_user ;
static char *  selected_pass ;
static char *  selected_encoding ;


/* --- static functions --- */

/*
 * Parsing "<user>@<proto>:<host>:<encoding>".
 */
static int
parse(
	char **  user ,		/* If seq doesn't have user, NULL is set. */
	int *  proto ,		/* If seq doesn't have proto, -1 is set. */
	char **  server ,
	char **  encoding ,	/* If seq doesn't have encoding, NULL is set. */
	char *  seq		/* broken in this function. */
	)
{
	char *  p ;
	size_t  len ;

	/*
	 * This hack enables the way of calling this function like
	 * 'parse( ... , kik_str_alloca_dup( "string"))'
	 */
	if( ! seq)
	{
		return  0 ;
	}

	if( ( p = strchr( seq , '@')))
	{
		*p = '\0' ;
		*user = seq ;
		seq = p + 1 ;
	}
	else
	{
		*user = NULL ;
	}

	len = strlen( seq) ;
	*proto = -1 ;
	if( len > 4 && strncmp( seq , "ssh:" , 4) == 0)
	{
		seq += 4 ;
		*proto = IDD_SSH ;
	}
	else if( len > 7)
	{
		if( strncmp( seq , "telnet:" , 7) == 0)
		{
			seq += 7 ;
			*proto = IDD_TELNET ;
		}
		else if( strncmp( seq , "rlogin:" , 7) == 0)
		{
			seq += 7 ;
			*proto = IDD_RLOGIN ;
		}
	}

	*server = seq ;
	
	if( ( p = strchr( seq , ':')))
	{
		*p = '\0' ;
		*encoding = p + 1 ;
	}
	else
	{
		*encoding = NULL ;
	}
	
	return  1 ;
}


LRESULT CALLBACK dialog_proc(
	HWND dlgwin ,
	UINT msg ,
	WPARAM wparam ,
	LPARAM lparam
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
				char *  encoding ;

				res = SendMessage( win , CB_FINDSTRINGEXACT , 0 ,
						(LPARAM)default_server) ;
				if( res != CB_ERR)
				{
					SendMessage( win , CB_SETCURSEL , res , 0) ;
				}

				if( parse( &user , &proto , &server , &encoding ,
						kik_str_alloca_dup( default_server)) )
				{
					SetWindowText( GetDlgItem( dlgwin , IDD_SERVER) , server) ;

					if( user || ( user = user_env))
					{
						SetWindowText( GetDlgItem( dlgwin , IDD_USER) ,
							user) ;
					}
					
					if( proto != -1)
					{
						selected_proto = proto ;
					}

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

			CheckRadioButton( dlgwin , IDD_SSH , IDD_RLOGIN , selected_proto) ;
			
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
				Window  win ;
				int  len ;
				char *  p ;
				
				win = GetDlgItem( dlgwin , IDD_SERVER) ;
				if( ( len = GetWindowTextLength( win)) > 0 &&
					( p = malloc( len + 1)) )
				{
					if( GetWindowText( win , p , len + 1) == 0)
					{
						free( p) ;
					}
					else
					{
						selected_server = p ;
					}
				}

				win = GetDlgItem( dlgwin , IDD_USER) ;
				if( ( len = GetWindowTextLength( win)) > 0 &&
					( p = malloc( len + 1)) )
				{
					if( GetWindowText( win , p , len + 1) == 0)
					{
						free( p) ;
					}
					else
					{
						selected_user = p ;
					}
				}
				
				win = GetDlgItem( dlgwin , IDD_PASS) ;
				if( ( len = GetWindowTextLength( win)) > 0 &&
					( p = malloc( len + 1)) )
				{
					if( GetWindowText( win , p , len + 1) == 0)
					{
						free( p) ;
					}
					else
					{
						selected_pass = p ;
					}
				}
				
				win = GetDlgItem( dlgwin , IDD_ENCODING) ;
				if( ( len = GetWindowTextLength( win)) > 0 &&
					( p = malloc( len + 1)) )
				{
					if( GetWindowText( win , p , len + 1) == 0)
					{
						free( p) ;
					}
					else
					{
						selected_encoding = p ;
					}
				}
				
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
				char *  encoding ;

				win = GetDlgItem( dlgwin , IDD_LIST) ;

				if( ( idx = SendMessage( win , CB_GETCURSEL , 0 , 0)) == CB_ERR ||
				    ( len = SendMessage( win , CB_GETLBTEXTLEN , idx , 0)) == CB_ERR ||
				    ( seq = alloca( len + 1)) == NULL ||
				    ( SendMessage( win , CB_GETLBTEXT , idx , seq)) == CB_ERR )
				{
					seq = NULL ;
				}

				if( seq && parse( &user , &proto , &server , &encoding , seq))
				{
					SetWindowText( GetDlgItem( dlgwin , IDD_SERVER) , server) ;

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
					
					if( encoding || ( encoding = ""))
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
	char **  server ,/* Should be free'ed by those who call this. Format:<proto>:<server> */
	char **  user ,		/* Same as above. If user is not input, NULL is set. */
	char **  pass ,		/* Same as above. If pass is not input, NULL is set. */
	char **  encoding ,	/* Same as above. If encoding is not input, NULL is set. */
	Window  parent_window ,
	char **  sv_list ,
	char *   def_server	/* (<user>@)(<proto>:)<server address>. */
	)
{
	size_t  len ;
	int  ret ;
	char *  format ;

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
		len = 4 + strlen( selected_server) + 1 ;
		format = "ssh:%s" ;
	}
	else if( selected_proto == IDD_TELNET)
	{
		len = 7 + strlen( selected_server) + 1 ;
		format = "telnet:%s" ;
	}
	else if( selected_proto == IDD_RLOGIN)
	{
		len = 7 + strlen( selected_server) + 1 ;
		format = "rlogin:%s" ;
	}
	else
	{
		goto  end ;
	}

	if( ( *server = malloc( len)) == NULL)
	{
		goto  end ;
	}

	sprintf( *server , format , selected_server) ;
	*user = selected_user ;
	*pass = selected_pass ;
	*encoding = selected_encoding ;

	/* Successfully */
	ret = 1 ;

end:
	selected_proto = -1 ;
	
	server_list = NULL ;
	default_server = NULL ;
	
	free( selected_server) ;
	selected_server = NULL ;
	
	if( ret == 0)
	{
		free( selected_user) ;
		free( selected_pass) ;
	}
	selected_user = NULL ;
	selected_pass = NULL ;
	selected_encoding = NULL ;
	
	return  ret ;
}
