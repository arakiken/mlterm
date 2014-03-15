/*
 *	$Id$
 */

#define _BSD_SOURCE	/* for strsep */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>	/* atoi */
#include <math.h>
#include <SDL.h>
#include <SDL_ttf.h>
#ifdef  USE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif
#include <kiklib/kik_config.h>


#define  REGIS_RGB(r,g,b) \
	(0xff000000 | (((r)*255/100) << 16) | (((g)*255/100) << 8) | ((b)*255/100))
#define  MAGIC_COLOR  0x0

#if  0
#define  __TEST
#endif


/* --- static variables --- */

static int  pen_x ;
static int  pen_y ;
static int  pen_x_stack[10] ;
static int  pen_y_stack[10] ;
static int  pen_stack_count ;
static SDL_Surface *  regis ;

static uint32_t  fg_color = 0xff000000 ;
static uint32_t  bg_color = 0xffffffff ;
static uint32_t  color_tbl[] =
{
	REGIS_RGB(0,0,0) ,	/* BLACK */
	REGIS_RGB(20,20,80) ,	/* BLUE */
	REGIS_RGB(80,13,13) ,	/* RED */
	REGIS_RGB(20,80,20) ,	/* GREEN */
	REGIS_RGB(80,20,80) ,	/* MAGENTA */
	REGIS_RGB(20,80,80) ,	/* CYAN */
	REGIS_RGB(80,80,20) ,	/* YELLOW */
	REGIS_RGB(53,53,53) ,	/* GRAY 50% */
	REGIS_RGB(26,26,26) ,	/* GRAY 25% */
	REGIS_RGB(33,33,60) ,	/* BLUE* */
	REGIS_RGB(60,26,26) ,	/* RED* */
	REGIS_RGB(33,60,33) ,	/* GREEN* */
	REGIS_RGB(60,33,60) ,	/* MAGENTA* */
	REGIS_RGB(33,60,60) ,	/* CYAN*/
	REGIS_RGB(60,60,33) ,	/* YELLOW* */
	REGIS_RGB(80,80,80)	/* GRAY 75% */
} ;


/* --- static functions --- */

#ifndef  HAVE_STRSEP

char *
strsep(
	char **  strp ,
	const char *  delim
	)
{

        char *  s ;
        const char *  spanp ;
        int  c ;
	int  sc ;
        char *  tok ;

	if( ( s = *strp) == NULL)
	{
		return	NULL ;
	}

	for( tok = s ; ; )
	{
		c = *s++ ;
		spanp = delim ;
		do
		{
			if( ( sc = *spanp++) == c)
			{
				if( c == 0)
				{
					s = NULL ;
				}
				else
				{
					s[-1] = 0 ;
				}

				*strp = s ;

				return  tok ;
			}
		}
		while( sc != 0) ;
	}
}

#endif

static void
help(void)
{
	fprintf( stderr , "registobmp [src: regis text file] [dst: bitmap file]\n") ;
	fprintf( stderr , " (Environment variables) REGIS_FONT=[font path]\n") ;
	fprintf( stderr , "                         REGIS_FONTSIZE=[font size]\n") ;
}

static void
resize(
	int  width ,
	int  height
	)
{
	SDL_Surface *  new_regis ;

	new_regis = SDL_CreateRGBSurface( SDL_SWSURFACE ,
			width , height , 32 , 0x00FF0000 , 0x0000FF00 , 0x000000FF , 0xFF000000) ;
	SDL_FillRect( new_regis , NULL , bg_color) ;

	if( regis)
	{
		SDL_Rect  rect ;

		rect.x = 0 ;
		rect.y = 0 ;
		rect.w = (regis->w > width) ? width : regis->w ;
		rect.h = (regis->h > height) ? height : regis->h ;

		SDL_BlitSurface( regis , &rect , new_regis , NULL) ;
	}

	regis = new_regis ;
}

inline static void
set_pixel(
	int  x ,
	int  y ,
	uint32_t  value
	)
{
	if( x < 0 || y < 0 || x >= regis->w || y >= regis->h)
	{
		return ;
	}

	((uint32_t*)regis->pixels)[y * regis->pitch / 4 + x] = value ;
}

inline static uint32_t
get_pixel(
	int  x ,
	int  y
	)
{
	if( x < 0 || y < 0 || x >= regis->w || y >= regis->h)
	{
		return  0 ;
	}

	return  ((uint32_t*)regis->pixels)[y * regis->pitch / 4 + x] ;
}

static void
draw_line(
	int  start_x ,
	int  start_y ,
	int  end_x ,
	int  end_y ,
	int  color
	)
{
	int  cx ;
	int  cy ;
	int  dx ;
	int  dy ;
	int  sx ;
	int  sy ;
	int  err ;
	int  n ;

	cx = start_x ;
	cy = start_y ;

	dx = end_x < cx ? cx - end_x : end_x - cx ;
	dy = end_y < cy ? cy - end_y : end_y - cy ;

	sx = cx < end_x ? 1 : -1 ;
	sy = cy < end_y ? 1 : -1 ;
	err = dx - dy ;

	for( n = 0 ; n < 1000 ; n++)
	{
		int  err2 ;

		set_pixel( cx , cy , color) ;
		if( cx == end_x && cy == end_y)
		{
			return ;
		}

		err2 = 2 * err ;

		if( err2 > -dy)
		{
			err = err - dy ;
			cx = cx + sx ;
		}

		if( err2 < dx)
		{
			err = err + dx ;
			cy = cy + sy ;
		}
	}
}

static void
draw_circle(
	int  x ,
	int  y ,
	int  r ,
	int  color
	)
{
	int  d ;
	int  dh ;
	int  dd ;
	int  cx ;
	int  cy ;

	d = 1 - r ;
	dh = 3 ;
	dd = 5 - 2 * r ;
	cy = r ;

	for( cx = 0 ; cx <= cy ; cx++)
	{
		if( d < 0)
		{
			d += dh ;
			dh += 2 ;
			dd += 2 ;
		}
		else
		{
			d += dd ;
			dh += 2 ;
			dd += 4 ;
			cy -- ;
		}

		set_pixel( cy + x , cx + y , color) ;
		set_pixel( cx + x , cy + y , color) ;
		set_pixel( -cx + x , cy + y , color) ;
		set_pixel( -cy + x , cx + y , color) ;
		set_pixel( -cy + x , -cx + y , color) ;
		set_pixel( -cx + x , -cy + y , color) ;
		set_pixel( cx + x , -cy + y , color) ;
		set_pixel( cy + x , -cx + y , color) ;
	}
}

static int
parse_options(
	char **  options ,	/* 10 elements */
	char **  cmd
	)
{
	if( **cmd == '(')
	{
		u_int  num_of_options ;
		int  skip_count ;
		int  inside_bracket ;
		char *  end ;

		num_of_options = 0 ;
		inside_bracket = skip_count = 0 ;
		end = ++ (*cmd) ;

		while( 1)
		{
			if( *end == '(')
			{
				skip_count ++ ;
			}
			else if( *end == ')')
			{
				if( skip_count == 0)
				{
					*end = '\0' ;
					if( options)
					{
						options[num_of_options ++] = *cmd ;
						options[num_of_options] = NULL ;
					}
					*cmd = end + 1 ;

					return  1 ;
				}
				else
				{
					skip_count -- ;
				}
			}
			else if( *end == '[')
			{
				inside_bracket = 1 ;
			}
			else if( *end == ']')
			{
				inside_bracket = 0 ;
			}
			else if( *end == ',')
			{
				if( skip_count == 0 && ! inside_bracket)
				{
					*end = '\0' ;
					if( num_of_options < 8 && options)
					{
						options[num_of_options ++] = *cmd ;
					}
					*cmd = end + 1 ;
				}
			}
			else if( *end == '\0')
			{
				if( options)
				{
					options[num_of_options ++] = *cmd ;
					options[num_of_options] = NULL ;
				}
				*cmd = NULL ;

				return  1 ;
			}

			end ++ ;
		}
	}

	return  0 ;
}

static int
parse_coordinate(
	int *  x ,
	int *  y ,
	char **  cmd
	)
{
	char *  xstr ;
	char *  ystr ;

	ystr = strsep( cmd , "]") ;
	if( ! *cmd)
	{
		*cmd = ystr ;

		return  0 ;
	}

	xstr = strsep( &ystr , ",") ;
	if( *xstr == '\0')
	{
		xstr = "+0" ;
	}

	if( ystr == NULL)
	{
		ystr = "+0" ;
	}

	if( *xstr == '+' || *xstr == '-')
	{
		if( ( *x = atoi( xstr) + pen_x) < 0)
		{
			*x = 0 ;
		}
	}
	else
	{
		*x = atoi( xstr) ;
	}

	if( *ystr == '+' || *ystr == '-')
	{
		if( ( *y = atoi( ystr) + pen_y) < 0)
		{
			*y = 0 ;
		}
	}
	else
	{
		*y = atoi( ystr) ;
	}

	return  1 ;
}

static char *
command_screen(
	char *  cmd
	)
{
	char *  options[10] ;

	if( parse_options( options , &cmd))
	{
		int  count ;

		for( count = 0 ; options[count] ; count++)
		{
			char *  option ;

			option = options[count] ;

			if( *option == 'I')
			{
				if( *(++option) == '(')
				{
					/* XXX Not standard command. */

					int  red ;
					int  green ;
					int  blue ;

					if( sscanf( option + 1 , "R%dG%dB%d" ,
						&red , &green , &blue) == 3)
					{
						bg_color = 0xff000000 | (red << 16) |
								(green << 8) | blue ;
					}
				}
				else
				{
					bg_color = color_tbl[ atoi( option + 1)] ;
				}
			}
			else if( *option == 'E')
			{
				SDL_FillRect( regis , NULL , bg_color) ;
			}
			else if( *option == 'C')
			{
				/*
				 * C0: turns the output cursor off.
				 * C1: turns the output cursor on.
				 */
			}
		}
	}

	return  cmd ;
}

static char *
command_text(
	char *  cmd
	)
{
	if( *cmd == '\'')
	{
		static int  init_font ;
		static TTF_Font *  font ;
		char *  text ;
		SDL_Surface *  image ;
		SDL_Color color ;
		SDL_Rect  image_rect ;
		SDL_Rect  rect ;

		cmd ++ ;
		text = strsep( &cmd , "\'") ;
		if( ! cmd)
		{
			return  text - 1 ;
		}

		if( ! font)
		{
			char *  file ;
			char *  size_str ;
			int  size ;
		#ifdef  USE_FONTCONFIG
			FcPattern *  mat = NULL ;
		#endif

			if( init_font)
			{
				return  cmd ;
			}

			init_font = 1 ;

			if( ! ( file = getenv( "REGIS_FONT")))
			{
			#if  defined(USE_FONTCONFIG)
				FcPattern *  pat ;
				FcResult result ;

				FcInit() ;

				pat = FcNameParse( "monospace") ;
				FcConfigSubstitute( 0 , pat , FcMatchPattern) ;
				FcDefaultSubstitute( pat) ;
				mat = FcFontMatch( 0 , pat , &result) ;
				FcPatternDestroy( pat) ;

				if( FcPatternGetString( mat , FC_FILE , 0 , &file)
				    != FcResultMatch)
				{
					FcPatternDestroy( mat) ;
					file = "arial.ttf" ;
				}
			#elif  defined(USE_WIN32API)
				file = "c:\\Windows\\Fonts\\arial.ttf" ;
			#else
				file = "arial.ttf" ;
			#endif
			}

			if( ! ( size_str = getenv( "REGIS_FONTSIZE")) ||
			    ( size = atoi( size_str)) == 0)
			{
				size = 12 ;
			}

			TTF_Init() ;
			if( ! ( font = TTF_OpenFont( file , size)))
			{
				fprintf( stderr , "Not found %s.\n" , file) ; 
			}

		#ifdef  USE_FONTCONFIG
			if( mat)
			{
				FcPatternDestroy( mat) ;
			}
		#endif

			if( ! font)
			{
				TTF_Quit() ;

				return  cmd ;
			}
		}

		color.r = (fg_color >> 16) & 0xff ;
		color.g = (fg_color >> 8) & 0xff ;
		color.b = fg_color & 0xff ;
	#if  SDL_MAJOR_VERSION > 1
		color.a = (fg_color >> 24) & 0xff ;
	#endif
		if( ( image = TTF_RenderUTF8_Blended( font , text , color)))
		{
			image_rect.x = 0 ;
			image_rect.y = 0 ;
			image_rect.w = image->w ;
			image_rect.h = image->h ;
			rect.x = pen_x ;
			rect.y = pen_y ;
			rect.w = 0 ;
			rect.h = 0 ;

			SDL_BlitSurface( image , &image_rect , regis , &rect) ;
		}
	}
	else if( *cmd == '(')
	{
		parse_options( NULL , &cmd) ;
	}

	return  cmd ;
}

static char *
command_w(
	char *  cmd
	)
{
	char *  options[10] ;

	if( parse_options( options , &cmd))
	{
		int  count ;

		for( count = 0 ; options[count] ; count++)
		{
			char *  option ;

			option = options[count] ;

			if( *option == 'I')
			{
				fg_color = color_tbl[ atoi( option + 1)] ;
			}
		}
	}

	return  cmd ;
}

static char *
command_position(
	char *  cmd
	)
{
	int  new_x ;
	int  new_y ;

	if( *(cmd ++) == '[')
	{
		if( ! parse_coordinate( &new_x , &new_y , &cmd))
		{
			return  cmd - 1 ;
		}

		pen_x = new_x ;
		pen_y = new_y ;

		return  cmd ;
	}
	else
	{
		return  cmd - 1 ;
	}
}

static char *
command_vector(
	char *  cmd
	)
{
	int  new_x ;
	int  new_y ;

	if( *cmd == '[')
	{
		cmd ++ ;
		if( ! parse_coordinate( &new_x , &new_y , &cmd))
		{
			return  cmd - 1 ;
		}

		draw_line( pen_x , pen_y , new_x , new_y , fg_color) ;

		pen_x = new_x ;
		pen_y = new_y ;
	}
	else if( *cmd == '(' && *(cmd + 2) == ')')
	{
		if( *(cmd + 1) == 'B')
		{
			if( pen_stack_count < 10)
			{
				pen_x_stack[pen_stack_count] = pen_x ;
				pen_y_stack[pen_stack_count] = pen_y ;
			}

			pen_stack_count ++ ;
		}
		else if( *(cmd + 1) == 'S')
		{
			if( pen_stack_count < 10)
			{
				pen_x_stack[pen_stack_count] = -1 ;
			}

			pen_stack_count ++ ;
		}
		else if( *(cmd + 1) == 'E')
		{
			pen_stack_count -- ;

			if( pen_stack_count < 10 && pen_x_stack[pen_stack_count] >= 0)
			{
				draw_line( pen_x , pen_y ,
					pen_x_stack[pen_stack_count] ,
					pen_y_stack[pen_stack_count] ,
					fg_color) ;
				pen_x = pen_x_stack[pen_stack_count] ;
				pen_y = pen_y_stack[pen_stack_count] ;
			}
		}

		cmd += 3 ;
	}

	return  cmd ;
}

static char *
command_circle(
	char *  cmd
	)
{
	if( *cmd == '[')
	{
		int  x ;
		int  y ;
		int  r ;

		cmd ++ ;
		if( ! parse_coordinate( &x , &y , &cmd))
		{
			return  cmd - 1 ;
		}

		if( x == pen_x)
		{
			r = abs( y - pen_y) ;
		}
		else if( y == pen_y)
		{
			r = abs( x - pen_x) ;
		}
		else
		{
			r = sqrt( pow( abs(x - pen_x) , 2) + pow( abs(y - pen_y) , 2)) ;
		}

		draw_circle( pen_x , pen_y , r , fg_color) ;
	}

	return  cmd ;
}

static void
fill(
	int  x ,
	int  y
	)
{
	if( y >= 1 && get_pixel( x , y - 1) != MAGIC_COLOR)
	{
		set_pixel( x , y - 1 , MAGIC_COLOR) ;
		fill( x , y - 1) ;
	}

	if( y < regis->h - 1 && get_pixel( x , y + 1) != MAGIC_COLOR)
	{
		set_pixel( x , y + 1 , MAGIC_COLOR) ;
		fill( x , y + 1) ;
	}

	if( x >= 1 && get_pixel( x - 1 , y) != MAGIC_COLOR)
	{
		set_pixel( x - 1 , y , MAGIC_COLOR) ;
		fill( x - 1 , y) ;
	}

	if( x < regis->w + 1 && get_pixel( x + 1 , y) != MAGIC_COLOR)
	{
		set_pixel( x + 1 , y , MAGIC_COLOR) ;
		fill( x + 1 , y) ;
	}
}

static char *  command( char *  cmd) ;

static char *
command_fill(
	char *  cmd
	)
{
	char *  options[10] ;

	if( parse_options( options , &cmd))
	{
		int  orig_fg_color ;
		int  x ;
		int  y ;

		orig_fg_color = fg_color ;
		fg_color = MAGIC_COLOR ;
		while( *(options[0] = command( options[0]))) ;

		fg_color = orig_fg_color ;

		for( y = 0 ; y < regis->h ; y++)
		{
			for( x = 0 ; x < regis->w ; x++)
			{
				if( get_pixel( x , y) == MAGIC_COLOR &&
				    y < 799 && get_pixel( x , y + 1) != MAGIC_COLOR)
				{
					fill( x , y + 1) ;

					for( ; y < regis->h ; y++)
					{
						for( x = 0 ; x < regis->w ; x++)
						{
							if( get_pixel( x , y) == MAGIC_COLOR)
							{
								set_pixel( x , y , fg_color) ;
							}
						}
					}

					break ;
				}
			}
		}
	}

	return  cmd ;
}

static char *
command(
	char *  cmd
	)
{
	static char *  (*command)( char *) ;

	switch( *cmd)
	{
		char *  orig_cmd ;

	default:
		orig_cmd = cmd ;
		if( ! command || ( cmd = (*command)( orig_cmd)) == orig_cmd)
		{
			return  orig_cmd + 1 ;
		}
		else
		{
			return  cmd ;
		}

	case  'S':
		command = command_screen ;
		break ;

	case  'T':
		command = command_text ;
		break ;

	case  'W':
		command = command_w ;
		break ;

	case  'P':
		command = command_position ;
		break ;

	case  'v':
	case  'V':
		command = command_vector ;
		break ;

	case  'C':
		command = command_circle ;
		break ;

	case  'F':
		command = command_fill ;
		break ;
	}

	return  (*command)( cmd + 1) ;
}


#ifdef  __TEST

static void
test_parse_options(void)
{
	char  cmd[] = "S(V(W(I(R,P))),S1)" ;
	char *  p ;
	char *  options[10] ;
	int  count ;

	p = cmd + 1 ;
	parse_options( options , &p) ;

	for( count = 0 ; options[count] ; count++)
	{
		fprintf(stderr,"%s %s\n",options[count],p) ;
	}
}

static void
test_parse_coordinate(void)
{
	char  cmd[] = "[100,200][300,400][+200,-300]" ;
	char *  p ;
	int  x ;
	int  y ;

	pen_x = 100 ;
	pen_y = 200 ;
	p = cmd ;
	while( *(p ++) == '[')
	{
		parse_coordinate( &x , &y , &p) ;

		fprintf( stderr , "%d %d\n" , x , y) ;
	}
	pen_x = 0 ;
	pen_y = 0 ;
}

#endif


/* --- global functions --- */

int
main(int argc, char **  argv)
{
	FILE *  fp ;
	char line[256] ;
	char *  cmd ;

#ifdef  __TEST
	test_parse_options() ;
	test_parse_coordinate() ;
#endif

	if( argc != 3)
	{
		help() ;

		return  -1 ;
	}

	if( ! ( fp = fopen(argv[1], "r")))
	{
		return  -1 ;
	}

#if  0
	SDL_Init(SDL_INIT_EVERYTHING) ;
#endif

	resize( 800 , 480) ;

	if( ( cmd = fgets( line , sizeof(line) , fp)))
	{
		char *  p ;

		if( *cmd == '\x1b' && ( p = strchr( cmd , 'p')))
		{
			cmd = p + 1 ;
		}

		do
		{
			while( *(cmd = command(cmd))) ;
		}
		while( (cmd = fgets( line , sizeof(line), fp))) ;
	}

	fclose(fp) ;

	SDL_SaveBMP( regis, argv[2]) ;

	return  0 ;
}
