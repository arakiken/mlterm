/*
 *	$Id$
 */

#define _BSD_SOURCE	/* for strsep */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>	/* atoi */
#include <SDL.h>
#include <SDL_ttf.h>
#ifdef  USE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif
#include <kiklib/kik_config.h>


#define  REGIS_RGB(r,g,b) \
	(0xff000000 | (((r)*255/100) << 16) | (((g)*255/100) << 8) | ((b)*255/100))


/* --- static variables --- */

static int pen_x ;
static int pen_y ;
static int max_x ;
static int max_y ;
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

static uint32_t
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

static char *
command_screen(
	char *  cmd
	)
{
	char *  code ;

	code = strsep( &cmd , ")") + 1 ;
	if( ! cmd)
	{
		return  code ;
	}
	code ++ ;

	if( *code == 'I')
	{
		int  idx ;

		if( code[1] == '(')
		{
			/* XXX Not standard command. */

			int  red ;
			int  green ;
			int  blue ;

			if( sscanf( code + 2 , "R%dG%dB%d" , &red , &green , &blue) == 3)
			{
				bg_color = 0xff000000 | (red << 16) | (green << 8) | blue ;
			}

			if( *cmd == ')')
			{
				/* skip ')' */
				cmd ++ ;
			}
		}
		else
		{
			idx = atoi( code + 1) ;
			bg_color = color_tbl[idx] ;
		}
	}
	else if( *code == 'E')
	{
		SDL_FillRect( regis , NULL , bg_color) ;
	}
	else if( *code == 'C')
	{
		/*
		 * C0: turns the output cursor off.
		 * C1: turns the output cursor on.
		 */
	}

	return  cmd ;
}

static char *
command_text(
	char *  cmd
	)
{
	char *  text ;

	text = cmd + 2 ;

	if( cmd[1] == '\'')
	{
		static int  init_font ;
		static TTF_Font *  font ;
		SDL_Surface *  image ;
		SDL_Color color ;
		SDL_Rect  image_rect ;
		SDL_Rect  rect ;

		strsep( &text , "\'") ;
		if( ! text)
		{
			return  cmd + 1 ;
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
				return  cmd + 1 ;
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

				return  cmd + 1 ;
			}
		}

		color.r = (fg_color >> 16) & 0xff ;
		color.g = (fg_color >> 8) & 0xff ;
		color.b = fg_color & 0xff ;
	#if  SDL_MAJOR_VERSION > 1
		color.a = (fg_color >> 24) & 0xff ;
	#endif
		image = TTF_RenderUTF8_Blended( font , cmd + 2 , color) ;
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
	else if( cmd[1] == '(')
	{
		strsep( &text ,")") ;
		if( ! text)
		{
			return  cmd + 1 ;
		}
	}
	else
	{
		return  cmd + 1 ;
	}

	return  text ;
}

static char *
command_w(
	char *  cmd
	)
{
	char *  code ;

	code = strsep( &cmd , ")") + 1 ;
	if( ! cmd)
	{
		return  code ;
	}

	code ++ ;

	if( *code == 'I')
	{
		int  idx ;

		idx = atoi( code + 1) ;
		fg_color = color_tbl[idx] ;
	}

	return  code ;
}

static char *
command_position(
	char *  cmd
	)
{
	char *  xstr ;
	char *  ystr ;
	int  new_x ;
	int  new_y ;

	xstr = strsep( &cmd , ",") + 1 ;
	if( ! cmd)
	{
		return  xstr ;
	}
	xstr ++ ;

	ystr = strsep( &cmd , "]") ;
	if( ! cmd)
	{
		return  ystr + 1 ;
	}

	new_x = atoi( xstr) ;
	new_y = atoi( ystr) ;

	if( ( pen_x = new_x) > max_x)
	{
		max_x = pen_x ;
	}

	if( ( pen_y = new_y) > max_y)
	{
		max_y = pen_y ;
	}

	return  ystr ;
}

static char *
command_vector(
	char *  cmd
	)
{
	char *  xstr ;
	char *  ystr ;
	int  new_x ;
	int  new_y ;

	if( strncmp( cmd , "v[]" , 3) == 0)
	{
		return  cmd + 3 ;
	}

	xstr = strsep( &cmd , ",") + 1 ;
	if( ! cmd)
	{
		return  xstr ;
	}
	xstr ++ ;

	ystr = strsep( &cmd , "]") ;
	if( ! cmd)
	{
		return  ystr + 1 ;
	}

	new_x = atoi( xstr) ;
	new_y = atoi( ystr) ;

	draw_line( pen_x , pen_y , new_x , new_y , fg_color) ;

	if( ( pen_x = new_x) > max_x)
	{
		max_x = pen_x ;
	}

	if( ( pen_y = new_y) > max_y)
	{
		max_y = pen_y ;
	}

	return  ystr ;
}

static char *
command(
	char *  cmd
	)
{
	switch( cmd[0])
	{
	case  'S':
		return  command_screen(cmd) ;

	case  'T':
		return  command_text(cmd) ;

	case  'W':
		return  command_w(cmd) ;

	case  'P':
		return  command_position(cmd) ;

	case  'v':
		return  command_vector(cmd) ;

	default:
		return  cmd + 1 ;
	}
}


/* --- global functions --- */

int
main(int argc, char **  argv)
{
	FILE *  fp ;
	char line[100] ;
        char *  cmd ;

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
