/*
 *	$Id$
 */

#include <stdio.h>
#include <windows.h>
#include <kiklib/kik_debug.h>
#include <kiklib/kik_sig_child.h>
#include <kiklib/kik_util.h>
#include <ml_term.h>
#include <ml_str_parser.h>
#include <mkf/mkf_sjis_conv.h>

ml_term_t *  term ;
mkf_parser_t *  parse ;
mkf_conv_t *  conv ;
HFONT  font ;

#if  0
#define  __DEBUG
#endif

#define  FONT_NAME  ")B‚l‚r ƒSƒVƒbƒN"
#define  FONT_SIZE  12
#define  COLUMNS  80
#define  LINES    30


static void
usage(void)
{
  	printf( "Usage: mlterm [-h/-telnet/-ssh/-rlogin/-raw] [host]\n") ;
  	printf( "(Without any option, mlterm start with sh.exe or cmd.exe.)\n") ;
  	fflush(NULL) ;
}

static void
sig_child(
  	void *  p ,
  	pid_t  pid
  	)
{
  	HWND  hwnd = *(HWND*)p ;

#ifdef  __DEBUG
  	kik_debug_printf( KIK_DEBUG_TAG " SIG_CHILD received.\n") ;
#endif
  
	PostMessage(hwnd, WM_DESTROY, 0, 0) ;
}

static void
pty_read_ready(
  	void *  p
  	)
{
  	HWND  hwnd = *(HWND*)p ;
  
  	PostMessage(hwnd, WM_PAINT, 0, 0) ;
}

LRESULT CALLBACK window_proc(
  	HWND hwnd,
  	UINT msg,
  	WPARAM wparam,
  	LPARAM lparam
  	)
{
        HDC  hdc ;
	CHAR buf[COLUMNS + 1];
  	int  cnt ;
  	int  row ;

  	switch(msg)
        {
        case WM_DESTROY:
          	ml_term_delete(term) ;
          	if( parse) parse->delete(parse) ;
          	if( conv) conv->delete(conv) ;
          	DeleteObject(font) ;
          
          	PostQuitMessage(0) ;

          	return 0 ;

        case WM_PAINT:
	        hdc = GetDC(hwnd) ;
          	SelectObject( hdc, font) ;
          	while( ml_term_parse_vt100_sequence( term))
                {
                  	RedrawWindow( hwnd, NULL, NULL, RDW_ERASENOW) ;
          		row = 0 ;
          		for( cnt = 0 ; cnt < LINES ; cnt++)
                	{
                          	int  col ;
                          
	                  	ml_line_t *  line = ml_term_get_line_in_screen( term, cnt) ;

        	          	if( line == NULL || ml_get_num_of_filled_chars_except_spaces(line) == 0)
                	        {
                                	col = 0 ;
                        	}
				else
                                {
                          		if( ! parse)
                                	{
                        			parse = ml_str_parser_new() ;
                                	}
                          
                          		ml_str_parser_set_str( parse, ml_char_at( line, 0),
                                                               ml_get_num_of_filled_chars_except_spaces(line)) ;
                          
                          		if( ! conv)
                                	{
                          			conv = mkf_sjis_conv_new() ;
                                	}

                          		conv->init( conv) ;
                          		col = conv->convert( conv , buf , COLUMNS, parse) ;
                                }
                          
                          	for( ; col < COLUMNS ; col++)
                                {
                                  	buf[col] = ' ' ;
                                }
                          	buf[col] = '\0' ;
                        
			#ifdef  __DEBUG
                          	kik_debug_printf( KIK_DEBUG_TAG "%s %d\n", buf, strlen(buf)) ;
			#endif
                          
        	          	TextOut(hdc,1,row,buf,strlen(buf)) ;
                          	row += FONT_SIZE ;
	                }
                }
        	ReleaseDC(hwnd,hdc) ;

	#ifdef  __DEBUG
          	kik_debug_printf( KIK_DEBUG_TAG "WM_PAINT_END\n") ;
	#endif

        	break;

        case WM_CHAR:
          	if( (char)wparam == '\r')
                {
                  	wparam = '\n' ;
                }
          	kik_warn_printf( "WRITTEN TO PTY=> %d %x\n" , ml_term_write( term, (char*)&wparam , 1, 0), wparam) ;
        }

  	return  DefWindowProc(hwnd,msg,wparam,lparam) ;
}

int PASCAL WinMain(
  	HINSTANCE hinst,
  	HINSTANCE hprev,
  	char *  cmdline,
  	int cmdshow
	)
{
  	char *  p ;
  	char *  protocol ;
  	char *  host ;
  	HWND  hwnd ;
  	WNDCLASS  wc ;
  	MSG  msg ;
  	int  width ;
  	int  height ;
  	ml_pty_event_listener_t  pty_listener ;
  	char  wid[50] ;
        char  lines[12] ;
        char  columns[15] ;

  	protocol = NULL ;
  	host = NULL ;
  
	p = cmdline ;

#if  0
        kik_debug_printf( "cmd_line => %s\n", cmdline) ;
#endif
  	while( 1)
        {
  		while( *p != '\0' && *p != ' ')
        	{
        		p++ ;
        	}

          	if( *p == '\0')
                {
                  	break ;
                }
  		*(p++) = '\0' ;

          	if( strcmp( cmdline, "-telnet") == 0 || strcmp( cmdline, "-ssh") == 0 ||
                    	strcmp( cmdline, "-rlogin") == 0 || strcmp( cmdline, "-raw") == 0)
                {
                  	protocol = cmdline ;
                }
          	else if( strcmp( cmdline, "-h") == 0)
                {
                  	usage() ;

                 	return  0 ;
                }
          
  		while( *p == ' ')
        	{
          		p++ ;
        	}

          	if( *p == '\0')
                {
                  	break ;
                }
        	cmdline = p ;
        }

  	if( *cmdline != '\0')
        {
  		host = cmdline ;
        }

#if  0
  	kik_debug_printf( "proto:%s host:%s\n", protocol, host) ;
#endif
  
  	/* Prepare window class */
  	ZeroMemory(&wc,sizeof(WNDCLASS)) ;
  	wc.lpfnWndProc = window_proc ;
  	wc.hInstance = hinst ;
  	wc.hIcon = NULL ;
  	wc.hCursor = LoadCursor(NULL,IDC_ARROW) ;
  	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH) ;
  	wc.lpszClassName = "Test" ;

  	if( ! RegisterClass(&wc))
        {
          	MessageBox(NULL,"Failed to register class.",NULL,MB_ICONSTOP) ;

          	return  1 ;
        }

  	font = CreateFont( FONT_SIZE, FONT_SIZE / 2,
                           0, /* text angle */
                           0, /* char angle */
                           FW_REGULAR, /* weight */
                           FALSE, /* italic */
                           FALSE, /* underline */
                           FALSE, /* eraseline */
                           SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           PROOF_QUALITY, FIXED_PITCH | FF_MODERN, FONT_NAME) ;

  	width = COLUMNS * FONT_SIZE / 2 + 2 ;
        height = LINES * FONT_SIZE + 30 ;

        hwnd = CreateWindow("Test", "mlterm", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        		width, height, NULL, NULL, hinst, NULL) ;

  	if( ! hwnd)
        {
          	MessageBox(NULL,"Failed to create window.",NULL,MB_ICONSTOP) ;

          	return  1 ;
        }

  	ShowWindow(hwnd,cmdshow) ;

  	term = ml_term_new( COLUMNS, LINES, 8, 0, ML_SJIS, 1, NO_UNICODE_FONT_POLICY, 1, 0, 0, 0, 0, 0,
                            BSM_NONE, VERT_LTR, ISCIILANG_UNKNOWN) ;

  	snprintf( wid, 50, "WINDOWID=%d", hwnd) ;
        snprintf( lines, 12, "LINES=%d", LINES) ;
        snprintf( columns, 15, "COLUMNS=%d", COLUMNS) ;

  	if( protocol)
        {
        	char *  argv[] = { protocol, host, NULL } ;
          	char *  env[] = { wid, NULL } ;
          	ml_term_open_pty( term, "plink.exe", argv, env, NULL) ;
        }
  	else
  	{
          	HANDLE  file ;

          	file = CreateFile( "C:\\msys\\1.0\\bin\\sh.exe", GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
                		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL) ;
          	if( file == INVALID_HANDLE_VALUE)
                {
                  	ml_term_open_pty( term, "C:\\windows\\system32\\cmd.exe",NULL,NULL,NULL) ;
                }
          	else
                {
        		char *  env[] = { lines, columns, wid, NULL } ;
        		char *  argv[] = { "--login", "-i" , NULL } ;
                  
  			ml_term_open_pty( term, "C:\\msys\\1.0\\bin\\sh.exe", argv, env, NULL) ;
                  	CloseHandle(file) ;
                }
        }

  	pty_listener.self = &hwnd ;
  	pty_listener.closed = NULL ;
  	pty_listener.read_ready = pty_read_ready ;

  	ml_term_attach( term, NULL, NULL, NULL, &pty_listener) ;

  	kik_sig_child_init() ;
  	kik_add_sig_child_listener( &hwnd, sig_child) ;

  	while(GetMessage(&msg,NULL,0,0))
        {
          	TranslateMessage(&msg) ;
          	DispatchMessage(&msg) ;
        }

  	kik_sig_child_final() ;

  	return  0 ;
}
