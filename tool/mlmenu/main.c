/*
 *	$Id$
 */

#include  <stdio.h>
#include  <unistd.h>	/* read */
#include  <stdlib.h>	/* malloc */
#include  <string.h>
#include  <X11/Xlib.h>
#include  <X11/Xutil.h>
#include  <sys/stat.h>
#include  <fcntl.h>	/* open */

#define  FONT_NAME  "-*-fixed-*-*-*--12-*-*-*-*-*-iso8859-1"

typedef struct entry
{
	char *  name ;
	char *  seq ;

} entry_t ;

static Display *  disp ;
static Window  win ;
static GC  gc1 ;
static GC  gc2 ;
static XFontStruct *  xfont ;
static entry_t  entries[124] ;
static u_int  n_ent ;
static int  cur_ent = -1 ;

static char *
get_value(
	char *  dev ,
	char *  key
	)
{
	int count ;
	char ret[1024] ;
	char c ;

	if( dev)
	{
		printf("\x1b]5381;%s:%s\x07", dev, key) ;
	}
	else
        {
		printf("\x1b]5381;%s\x07", key) ;
	}
	fflush(stdout) ;

	for( count = 0 ; count < 1024 ; count++)
	{
		if( read(STDIN_FILENO, &c, 1) == 1)
		{
			if( c != '\n')
			{
				ret[count] = c ;
			}
			else
			{
				ret[count] = '\0' ;

				if( count < 2 + strlen(key) || strcmp(ret, "#error") == 0)
				{
					return  NULL ;
				}

				/*
				 * #key=value
				 */
				return  strdup(ret + 2 + strlen(key)) ;
			}
		}
		else
		{
			return  NULL ;
		}
	}

	return  NULL ;
}

static int
init_entries(
	u_int *  cols ,
	u_int *  rows
	)
{
	int  fd ;
	struct stat  st ;
	char *  buf ;
	char *  line ;
	char *  p ;

	if( ( fd = open( "/home/ken/.mlterm/menu" , O_RDONLY , 0600)) == -1 &&
		( fd = open( "/usr/local/etc/mlterm/menu" , O_RDONLY , 0600)) == -1)
	{
		return  0 ;
	}

	fstat( fd , &st) ;

	if( ( buf = malloc( st.st_size + 1)) == NULL)
	{
		return  0 ;
	}

	read( fd , buf , st.st_size) ;
	buf[st.st_size] = '\0' ;

	close( fd) ;

	*cols = 0 ;
	while( n_ent < 124 && buf)
	{
		char *  name ;
		char *  seq ;

		line = buf ;
		
		if( ( p = strchr( line , '\n')))
		{
			*(p ++) = '\0' ;
		}

		buf = p ;
		p = line ;

		while( *p == ' ' || *p == '\t')
		{
			p ++ ;
		}
		
		if( *p == '#')
		{
			continue ;
		}

		name = p ;

		while(1)
		{
			if( *p == '\0')
			{
				seq = "" ;
				
				goto  end ;
			}
			else if( *p != ' ' && *p != '\t' && *p != '\n')
			{
				p ++ ;
			}
			else
			{
				*(p ++) = '\0' ;
				break ;
			}
		}

		while( *p == ' ' || *p == '\t')
		{
			p ++ ;
		}
		seq = p ;
		
		while( *p != ' ' && *p != '\t' && *p != '\n' && *p != '\0')
		{
			p ++ ;
		}
		*p = '\0' ;

	end:
		if( strcmp( name , "pty_list") == 0 && *seq == '\0')
		{
			char *  pty_list ;
			char *  pty ;
			int  is_active ;
			
			if ((pty_list = get_value(NULL, "pty_list")) == NULL)
			{
				return 1;
			}

			while (pty_list)
			{
				pty = pty_list;
				pty_list = strchr(pty_list, ':');
				
				if (pty_list)
				{
					*pty_list = '\0';
				}
				else
				{
					break;
				}
				
				if (*(pty_list + 1) == '1')
				{
					is_active = 1;
				}
				else
				{
					is_active = 0;
				}
				
				if( ( pty_list = strchr(pty_list + 1, ';')))
				{
					pty_list++;
				}

				if ((name = get_value(pty, "pty_name")) == NULL)
				{
					name = pty;
				}
				
				if (strncmp(name, "/dev/", 5) == 0)
				{
					name += 5;
				}

				seq = malloc(strlen(pty) + 12);
				sprintf(seq, "select_pty=%s", pty);

				entries[n_ent].name = name ;
				entries[n_ent].seq = seq ;

				if( *cols < strlen( entries[n_ent].name))
				{
					*cols = strlen( entries[n_ent].name) ;
				}

				if( ++ n_ent >= 124)
				{
					break ;
				}
			}
		}
		else
		{
			if( *name != '\0')
			{
				entries[n_ent].name = strdup( name) ;
				entries[n_ent].seq = strdup( seq) ;

				if( *cols < strlen( entries[n_ent].name))
				{
					*cols = strlen( entries[n_ent].name) ;
				}
				
				if( ++ n_ent >= 124)
				{
					break ;
				}
			}
		}
	}

	if( n_ent == 0)
	{
		return  0 ;
	}

	*rows = n_ent ;

	return  1 ;
}

static int
init(void)
{
	int  x ;
	int  y ;
	u_int  cols ;
	u_int  rows ;
	u_int  width ;
	u_int  height ;

	if( ! init_entries( &cols , &rows))
	{
		return  0 ;
	}

	if( ( disp = XOpenDisplay(NULL)) == NULL)
	{
		return  0 ;
	}

	if( ( xfont = XLoadQueryFont( disp , FONT_NAME)) == NULL)
	{
		return  0 ;
	}

	width = xfont->max_bounds.width * cols ;
	height = (xfont->ascent + xfont->descent) * rows ;

	{
		Window  root ;
		Window  child ;
		int  root_x ;
		int  root_y ;
		u_int  mask ;

		XQueryPointer( disp , DefaultRootWindow(disp) , &root , &child , &root_x , &root_y ,
			&x , &y , &mask) ;
	}

	if( ( win = XCreateSimpleWindow( disp , DefaultRootWindow(disp) , x , y , width , height , 1 ,
		BlackPixel( disp , DefaultScreen(disp)) ,
		WhitePixel( disp , DefaultScreen(disp))) ) == NULL)
	{
		return  0 ;
	}

	{
		XSetWindowAttributes  attr ;

		attr.override_redirect = True ;
		XChangeWindowAttributes( disp , win , CWOverrideRedirect , &attr) ;
	}

	{
		XGCValues  gc_value ;
		
		gc_value.graphics_exposures = 0 ;
		gc1 = XCreateGC( disp , win , GCGraphicsExposures , &gc_value) ;
		gc2 = XCreateGC( disp , win , GCGraphicsExposures , &gc_value) ;
	}

	XSetForeground( disp , gc1 , BlackPixel( disp , DefaultScreen(disp))) ;
	XSetBackground( disp , gc1 , WhitePixel( disp , DefaultScreen(disp))) ;
	XSetFont( disp , gc1 , xfont->fid) ;

	XSetForeground( disp , gc2 , WhitePixel( disp , DefaultScreen(disp))) ;
	XSetBackground( disp , gc2 , BlackPixel( disp , DefaultScreen(disp))) ;
	XSetFont( disp , gc2 , xfont->fid) ;
	
	XSelectInput( disp , win ,
		EnterWindowMask | LeaveWindowMask | PointerMotionMask | ButtonPressMask) ;

	if( XGrabPointer( disp , DefaultRootWindow(disp) , True , ButtonPressMask , GrabModeAsync ,
		GrabModeAsync , None , None , CurrentTime) != GrabSuccess)
	{
		return  0 ;
	}

	XMapWindow( disp , win) ;
	XClearWindow( disp , win) ;

	return  1 ;
}

static int
update_screen(
	int  n ,
	int  reverse
	)
{
	int  start ;
	int  end ;
	int  count ;
	GC  gc ;
	
	if( n < 0)
	{
		start = 0 ;
		end = n_ent - 1 ;
	}
	else
	{
		start = n ;
		end = n ;
	}

	if( reverse)
	{
		gc = gc2 ;
	}
	else
	{
		gc = gc1 ;
	}

	for( count = start ; count <= end ; count ++)
	{
		XDrawImageString( disp , win , gc , 0 ,
			count * (xfont->ascent + xfont->descent) + xfont->ascent ,
			entries[count].name , strlen(entries[count].name)) ;
	}

	return  1 ;
}

static int
mouse_motion(
	int  y
	)
{
	int  entry ;

	entry = y / 10 ;
	if( entry != cur_ent)
	{
		if( cur_ent >= 0)
		{
			update_screen( cur_ent , 0) ;
		}

		update_screen( cur_ent = entry , 1) ;
	}

	return  1 ;
}

static int
mouse_pressed(void)
{
	printf( "\x1b]5379;%s\x07" , entries[cur_ent].seq) ;
	fflush( stdout) ;

	return  1 ;
}

static int
event_loop(void)
{
	XEvent  ev ;

	while( 1)
	{
		XNextEvent( disp , &ev) ;
		if( ev.xany.window != win)
		{
			break ;
		}

		if( ev.type == EnterNotify)
		{
			mouse_motion( ev.xcrossing.y - 1) ;
		}
		else if( ev.type == MotionNotify)
		{
			mouse_motion( ev.xcrossing.y - 1) ;
		}
		else if( ev.type == ButtonPress)
		{
			mouse_pressed() ;

			break ;
		}
		else if( ev.type == LeaveNotify)
		{
			update_screen( cur_ent , 0) ;
			cur_ent = -1 ;
		}
	}

	return  1 ;
}

int
main(
	int  argc ,
	char **  argv
	)
{
	if( ! init() || ! update_screen( -1 , 0) || ! event_loop())
	{
		return  1 ;
	}

	return  0 ;
}
