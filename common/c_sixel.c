/*
 *	$Id$
 */

#include <string.h>	/* memcpy */
#include <sys/stat.h>	/* fstat */
#include <kiklib/kik_util.h>	/* K_MIN */


#ifdef  SIXEL_1BPP

#define  realloc_pixels  realloc_pixels_1bpp
#define  correct_height  correct_height_1bpp
#define  load_sixel_from_file  load_sixel_from_file_1bpp
#define  SIXEL_RGB(r,g,b)  ((9 * (r) + 30 * (g) + (b)) * 51 >= 5120 * 20 ? 1 : 0)
#define  CARD_HEAD_SIZE  0
#define  pixel_t  u_int8_t

#else	/* SIXEL_1BPP */

#define  SIXEL_RGB(r,g,b)  ((((r)*255/100) << 16) | (((g)*255/100) << 8) | ((b)*255/100))
#ifndef  CARD_HEAD_SIZE
#ifdef  GDK_PIXBUF_VERSION
#define  CARD_HEAD_SIZE  0
#else
#define  CARD_HEAD_SIZE  8
#endif
#endif	/* CARD_HEAD_SIZE */
#define  pixel_t  u_int32_t

#endif	/* SIXEL_1BPP */

#define  PIXEL_SIZE  sizeof(pixel_t)


/* --- static variables --- */

#ifndef  SIXEL_1BPP
static pixel_t *  custom_palette ;
#endif


/* --- static functions --- */

#ifndef  __GET_PARAMS__
#define  __GET_PARAMS__
static size_t
get_params(
	int *  params ,
	size_t  max_params ,
	char **  p
	)
{
	size_t  count ;
	char *  start ;

	memset( params , 0 , sizeof(int) * max_params) ;

	for( start = *p , count = 0 ; count < max_params ; count++)
	{
		while( 1)
		{
			if( '0' <= **p && **p <= '9')
			{
				params[count] = params[count] * 10 + (**p - '0') ;
			}
			else if( **p == ';')
			{
				(*p)++ ;
				break ;
			}
			else
			{
				if( start == *p)
				{
					return  0 ;
				}
				else
				{
					(*p)-- ;

					return  count + 1 ;
				}
			}

			(*p)++ ;
		}
	}

	(*p)-- ;

	return  count ;
}
#endif	/* __GET_PARAMS__ */

static int
realloc_pixels(
	u_char **  pixels ,
	int  new_width ,
	int  new_height ,
	int  cur_width ,
	int  cur_height
	)
{
	u_char *  p ;
	int  y ;
	int  n_copy_rows ;
	size_t  new_line_len ;
	size_t  cur_line_len ;

	if( new_width == cur_width && new_height == cur_height)
	{
		return  1 ;
	}

	n_copy_rows = K_MIN(new_height,cur_height) ;
	new_line_len = new_width * PIXEL_SIZE ;
	cur_line_len = cur_width * PIXEL_SIZE ;

	if( new_width < cur_width)
	{
		if( new_height > cur_height)
		{
			/* Not supported */

		#ifdef  DEBUG
			kik_error_printf( KIK_DEBUG_TAG
				" Sixel width is shrunk (%d->%d) but"
				" height is lengthen (%d->%d)\n" ,
				cur_width , cur_height , new_width , new_height) ;
		#endif

			return  0 ;
		}
		else /* if( new_height < cur_height) */
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Sixel data: %d %d -> shrink %d %d\n" ,
				cur_width , cur_height , new_width , new_height) ;
		#endif

			for( y = 1 ; y < n_copy_rows ; y++)
			{
				memmove( *pixels + (y * new_line_len) ,
					*pixels + (y * cur_line_len) ,
					new_line_len) ;
			}

			return  1 ;
		}
	}
	else if( new_width == cur_width && new_height < cur_height)
	{
		/* do nothing */

		return  1 ;
	}

	if( new_width > (SSIZE_MAX - CARD_HEAD_SIZE) / PIXEL_SIZE / new_height)
	{
		/* integer overflow */
		return  0 ;
	}

	if( new_width == cur_width /* && new_height > cur_height */)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Sixel data: %d %d -> realloc %d %d\n" ,
			cur_width , cur_height , new_width , new_height) ;
	#endif

		/*
		 * Cast to u_char* is necessary because this function can be
		 * compiled by g++.
		 */
		if( ( p = (u_char*)realloc( *pixels - CARD_HEAD_SIZE ,
				CARD_HEAD_SIZE + new_line_len * new_height)))
		{
			p += CARD_HEAD_SIZE ;
		}
		else
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " realloc failed.\n.") ;
		#endif
			return  0 ;
		}

		memset( p + cur_line_len * cur_height , 0 ,
			new_width * (new_height - cur_height)) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Sixel data: %d %d -> calloc %d %d\n" ,
			cur_width , cur_height , new_width , new_height) ;
	#endif

		/* Cast to u_char* is necessary because this function can be compiled by g++. */
		if( ( p = (u_char*)calloc( CARD_HEAD_SIZE + new_line_len * new_height , 1)))
		{
			p += CARD_HEAD_SIZE ;
		}
		else
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " calloc failed.\n.") ;
		#endif
			return  0 ;
		}

		for( y = 0 ; y < n_copy_rows ; y++)
		{
			memcpy( p + (y * new_line_len) ,
				(*pixels) + (y * cur_line_len) ,
				cur_line_len) ;
		}

		if( *pixels)
		{
			free( (*pixels) - CARD_HEAD_SIZE) ;
		}
	}

	*pixels = p ;

	return  1 ;
}

/*
 * Correct the height which is always multiple of 6, but this doesn't
 * necessarily work.
 */
static void
correct_height(
	pixel_t *  pixels ,
	int  width ,
	int *  height	/* multiple of 6 */
	)
{
	int  x ;
	int  y ;

	pixels += (width * (*height - 1)) ;

	for( y = 0 ; y < 5 ; y++)
	{
		for( x = 0 ; x < width ; x++)
		{
			if( pixels[x])
			{
				return ;
			}
		}

		(*height) -- ;
		pixels -= width ;
	}
}

/*
 * load_sixel_from_file() returns at least 1024*1024 pixels memory even if
 * the actual image size is less than it.
 * It is the caller that should shrink (realloc) it.
 */
static u_char *
load_sixel_from_file(
	const char *  path ,
	u_int *  width_ret ,
	u_int *  height_ret
	)
{
	FILE *  fp ;
	struct stat  st ;
	char *  file_data ;
	char *  p ;
	size_t  len ;
	u_char *  pixels ;
	int  params[5] ;
	size_t  n ;	/* number of params */
	int  init_width ;
	int  pix_x ;
	int  pix_y ;
	int  cur_width ;
	int  cur_height ;
	int  width ;
	int  height ;
	int  rep ;
	int  color ;
	int  asp_x ;
	/* VT340 Default Color Map */
	static pixel_t  default_palette[] =
	{
		SIXEL_RGB(0,0,0) ,	/* BLACK */
		SIXEL_RGB(20,20,80) ,	/* BLUE */
		SIXEL_RGB(80,13,13) , /* RED */
		SIXEL_RGB(20,80,20) ,	/* GREEN */
		SIXEL_RGB(80,20,80) ,	/* MAGENTA */
		SIXEL_RGB(20,80,80) , /* CYAN */
		SIXEL_RGB(80,80,20) , /* YELLOW */
		SIXEL_RGB(53,53,53) ,	/* GRAY 50% */
		SIXEL_RGB(26,26,26) ,	/* GRAY 25% */
		SIXEL_RGB(33,33,60) , /* BLUE* */
		SIXEL_RGB(60,26,26) , /* RED* */
		SIXEL_RGB(33,60,33) , /* GREEN* */
		SIXEL_RGB(60,33,60) , /* MAGENTA* */
		SIXEL_RGB(33,60,60) , /* CYAN* */
		SIXEL_RGB(60,60,33) , /* YELLOW* */
		SIXEL_RGB(80,80,80)   /* GRAY 75% */
	} ;
	pixel_t *  palette ;

	if( ! ( fp = fopen( path , "r")))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " failed to open %s\n." , path) ;
	#endif

		return  NULL ;
	}

	fstat( fileno( fp) , &st) ;

	/*
	 * - malloc() should be used here because alloca() could return insufficient memory.
	 * - Cast to char* is necessary because this function can be compiled by g++.
	 */
	if( ! ( p = file_data = (char*)malloc( st.st_size + 1)))
	{
		fclose(fp) ;

		return  NULL ;
	}

	len = fread( p , 1 , st.st_size , fp) ;
	fclose( fp) ;
	p[len] = '\0' ;

	pixels = NULL ;
	init_width = 0 ;
	cur_width = cur_height = 0 ;
	width = 1024 ;
	height = 1024 ;

	/*  Cast to u_char* is necessary because this function can be compiled by g++. */
	if( ! realloc_pixels( &pixels , width , height , 0 , 0))
	{
		free( file_data) ;

		return  NULL ;
	}

#ifndef  SIXEL_1BPP
	if( custom_palette)
	{
		palette = custom_palette ;
		if( palette[256] == 0)	/* No active palette */
		{
			memcpy( palette , default_palette , sizeof(default_palette)) ;
			memset( palette + 16 , 0 ,
				sizeof(pixel_t) * 256 - sizeof(default_palette)) ;
		}
	}
	else
#endif
	{
		palette = (pixel_t*)alloca( sizeof(pixel_t) * 256) ;
		memcpy( palette , default_palette , sizeof(default_palette)) ;
		memset( palette + 16 , 0 , sizeof(pixel_t) * 256 - sizeof(default_palette)) ;
	}

restart:
	while( 1)
	{
		if( *p == '\0')
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format\n.") ;
		#endif

			goto  end ;
		}
		else if( *p == '\x90')
		{
			break ;
		}
		else if( *p == '\x1b')
		{
			if( *(++p) == 'P')
			{
				break ;
			}
		}
		else
		{
			p ++ ;
		}
	}

	if( *(++p) != ';')
	{
		/* P1 */
		switch( *p)
		{
		case 'q':
			/* The default value. (2:1 is documented though) */
			asp_x = 1 ;
		#if  0
			asp_y = 1 ;
		#endif
			goto  body ;

	#if  0
		case '0':
		case '1':
		case '5':
		case '6':
			asp_x = 1 ;
			asp_y = 2 ;
			break;

		case '2':
			asp_x = 1 ;
			asp_y = 5 ;
			break;

		case '3':
		case '4':
			asp_x = 1 ;
			asp_y = 3 ;
			break;

		case '7':
		case '8':
		case '9':
			asp_x = 1 ;
			asp_y = 1 ;
			break;

		default:
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;
	#else
		case '\0':
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;

		default:
			asp_x = 1 ;	/* XXX */
	#endif
		}

		if( p[1] == ';')
		{
			p ++ ;
		}
	}
	else
	{
		/* P1 is omitted. */
		asp_x = 1 ;	/* V:H=2:1 */
	#if  0
		asp_y = 2 ;
	#endif
	}

	if( *(++p) != ';')
	{
		/* P2 */
		switch( *p)
		{
		case 'q':
			goto  body ;
	#if  0
		case '0':
		case '2':
			...
			break;

		default:
	#else
		case '\0':
	#endif
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;
		}

		if( p[1] == ';')
		{
			p ++ ;
		}
	}
#if  0
	else
	{
		/* P2 is omitted. */
	}
#endif

	/* Ignoring P3 */
	while( *(++p) != 'q')
	{
		if( *p == '\0')
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;
		}
	}

body:
	rep = asp_x ;
	pix_x = pix_y = 0 ;
	color = 0 ;

	while( *(++p) != '\0')
	{
		if( *p == '"')	/* " Pan ; Pad ; Ph ; Pv */
		{
			if( *(++p) == '\0')
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
			#endif
				break ;
			}

			if( ( n = get_params( params , 4 , &p)) == 1)
			{
				params[1] = 1 ;
				n = 2 ;
			}

			/* XXX ignored */
		#if  0
			switch(n)
			{
			case 4:
				height = params[3] ;
			case 3:
				width = params[2] ;
				/* XXX realloc_pixels() is necessary here. */
			case 2:
				/* V:H=params[0]:params[1] */
			#if  0
				asp_x = params[1] ;
				asp_y = params[0] ;
			#else
				rep /= asp_x ;
				if( ( asp_x = params[1] / params[0]) == 0)
				{
					asp_x = 1 ;	/* XXX */
				}
				rep *= asp_x ;
			#endif
			}

			if( asp_x <= 0)
			{
				asp_x = 1 ;
			}
		#endif
		}
		else if( *p == '!')	/* ! Pn Ch */
		{
			if( *(++p) == '\0')
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
			#endif

				break ;
			}

			if( get_params( params , 1 , &p) > 0)
			{
				if( ( rep = params[0]) < 1)
				{
					rep = 1 ;
				}

				rep *= asp_x ;
			}
		}
		else if( *p == '#')	/* # Pc ; Pu; Px; Py; Pz */
		{
			if( *(++p) == '\0')
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
			#endif
				break ;
			}

			n = get_params( params , 5 , &p) ;

			if( n > 0)
			{
				if( ( color = params[0]) < 0)
				{
					color = 0 ;
				}
				else if( color > 255)
				{
					color = 255 ;
				}
			}

			if( n > 4)
			{
				if( params[1] == 1)
				{
					/* HLS */
					int  h ;
					u_int32_t  l ;
					u_int32_t  s ;
					u_int8_t  rgb[3] ;

					h = K_MIN(params[2],360) ;
					l = K_MIN(params[3],100) ;
					s = K_MIN(params[4],100) ;

					if( s == 0)
					{
						rgb[0] = rgb[1] = rgb[2] = l * 255 / 100 ;
					}
					else
					{
						u_int32_t  m1 ;
						u_int32_t  m2 ;
						int  count ;

						if( l < 50)
						{
							m2 = l * (100 + s) ;
						}
						else
						{
							m2 = (l + s) * 100 - l * s ;
						}
						m1 = l * 200 - m2 ;

						for( count = 0 ; count < 3 ; count++)
						{
							u_int32_t  pc ;

							if( h < 60)
							{
								pc = m1 + (m2 - m1) * h / 60 ;
							}
							else if( h < 180)
							{
								pc = m2 ;
							}
							else if( h < 240)
							{
								pc = m1 + (m2 - m1) *
								          (240 - h) / 60 ;
							}
							else
							{
								pc = m1 ;
							}
							rgb[count] = pc * 255 / 10000 ;

							if( ( h -= 120) < 0)
							{
								h += 360 ;
							}
						}
					}

					palette[color] = (rgb[0] << 16) | (rgb[1] << 8) | rgb[2] ;
				}
				else if( params[1] == 2)
				{
					/* RGB */
					palette[color] = SIXEL_RGB(K_MIN(params[2],100),
					                             K_MIN(params[3],100),
								     K_MIN(params[4],100)) ;
				}
				else
				{
					continue ;
				}


			#ifndef  SIXEL_1BPP
				if( palette == custom_palette && palette[256] <= color)
				{
					/*
					 * Set max active palette number for NetBSD/OpenBSD.
					 * (See load_file() in fb/x_imagelib.c)
					 */
					palette[256] = color + 1 ;
				}
			#endif

			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" Set rgb %x for color %d.\n" ,
					palette[color] , color) ;
			#endif
			}
		}
		else if( *p == '$' || *p == '-')
		{
			pix_x = 0 ;
			rep = asp_x ;

			if( ! init_width && width > cur_width && cur_width > 0)
			{
				int  y ;

			#ifdef  DEBUG
				kik_debug_printf( "Sixel width is shrunk (%d -> %d)\n" ,
					width , cur_width) ;
			#endif

				for( y = 1 ; y < cur_height ; y++)
				{
					memmove( pixels + y * cur_width * PIXEL_SIZE ,
						pixels + y * width * PIXEL_SIZE ,
						cur_width * PIXEL_SIZE) ;
				}
				memset( pixels + y * cur_width * PIXEL_SIZE , 0 ,
					(cur_height * (width - cur_width) * PIXEL_SIZE)) ;

				width = cur_width ;
				init_width = 1 ;
			}

			if( *p == '-')
			{
				pix_y += 6 ;
			}
		}
		else if( *p >= '?' && *p <= '\x7E')
		{
			u_int  new_width ;
			u_int  new_height ;
			int  a ;
			int  b ;
			int  y ;

			if( ! realloc_pixels( &pixels ,
					(new_width = width < pix_x + rep ? width + 512 : width) ,
					(new_height = height < pix_y + 6 ? height + 512 : height) ,
					width , height))
			{
				break ;
			}

			width = new_width ;
			height = new_height ;

			b = *p - '?' ;
			a = 0x01 ;

			for( y = 0 ; y < 6 ; y++ )
			{
				if( (b & a) != 0)
				{
					int  x ;

					for( x = 0 ; x < rep ; x ++)
					{
					#if  defined(GDK_PIXBUF_VERSION) || defined(USE_QUARTZ)
						/* RGBA */
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE] =
							(palette[color] >> 16) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE + 1] =
							(palette[color] >> 8) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE + 2] =
							(palette[color]) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE + 3] = 0xff ;
					#elif  defined(SIXEL_1BPP)
						/* 0x80 is opaque mark */
						((pixel_t*)pixels)[(pix_y + y) * width +
							pix_x + x] =
							0x80 | palette[color] ;
					#else
						/* ARGB (cardinal) */
						((pixel_t*)pixels)[(pix_y + y) * width +
							pix_x + x] =
							0xff000000 | palette[color] ;
					#endif
					}
				}

				a <<= 1 ;
			}

			pix_x += rep ;

			if( cur_width < pix_x)
			{
				cur_width = pix_x ;
			}

			if( cur_height < pix_y + 6)
			{
				cur_height = pix_y + 6 ;
			}

			rep = asp_x ;
		}
		else if( *p == '\x1b')
		{
			if( *(++p) == '\\')
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " EOF.\n.") ;
			#endif

				if( *(p + 1) != '\0')
				{
					goto  restart ;
				}

				break ;
			}
			else if( *p == '\0')
			{
				break ;
			}
		}
		else if( *p == '\x9c')
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " EOF.\n.") ;
		#endif

			if( *(p + 1) != '\0')
			{
				goto  restart ;
			}

			break ;
		}
	}

end:
	free( file_data) ;

#ifndef  SIXEL_1BPP
	custom_palette = NULL ;
#endif

	if( cur_width == 0 ||
	    ! realloc_pixels( &pixels , cur_width , cur_height , width , height))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Nothing is drawn.\n") ;
	#endif

		free( pixels - CARD_HEAD_SIZE) ;

		return  NULL ;
	}

	correct_height( (pixel_t*)pixels , cur_width , &cur_height) ;

	*width_ret = cur_width ;
	*height_ret = cur_height ;

	return  pixels ;
}

#ifndef  SIXEL_1BPP

pixel_t *
x_set_custom_sixel_palette(
	pixel_t *  palette	/* NULL -> Create new palette */
	)
{
	if( ! palette)
	{
		palette = (pixel_t*)calloc( sizeof(pixel_t) , 257) ;
	}

	return  (custom_palette = palette) ;
}

#endif

#undef  realloc_pixels
#undef  correct_height
#undef  load_sixel_from_file
#undef  SIXEL_RGB
#undef  CARD_HEAD_SIZE
#undef  pixel_t
#undef  PIXEL_SIZE
