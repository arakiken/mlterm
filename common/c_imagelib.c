/*
 *	$Id$
 */

#include <sys/stat.h>	/* fstat */
#include <kiklib/kik_util.h>	/* K_MIN */


#ifdef  BUILTIN_IMAGELIB

#ifdef  SIXEL_1BPP
#define  SIXEL_RGB(r,g,b)  ((9 * (r) + 30 * (g) + (b)) * 51 >= 5120 * 20 ? 1 : 0)
#define  CARD_HEAD_SIZE  0
typedef u_int8_t  pixel_t ;
#else
#define  SIXEL_RGB(r,g,b)  ((((r)*255/100) << 16) | (((g)*255/100) << 8) | ((b)*255/100))
#ifndef  CARD_HEAD_SIZE
#define  CARD_HEAD_SIZE  8
#endif
typedef u_int32_t  pixel_t ;
#endif
#define  PIXEL_SIZE  sizeof(pixel_t)


/* --- static variables --- */

#ifdef  USE_GRF
static pixel_t  sixel_cmap[256] ;
static u_int  sixel_cmap_size ;
#endif


/* --- static functions --- */

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

	n_copy_rows = K_MIN(new_height,cur_height) ;

	if( new_width == cur_width && new_height == cur_height)
	{
		return  1 ;
	}

	if( new_width < cur_width)
	{
		if( new_height > cur_height)
		{
			/* Not supported */

		#ifdef  DEBUG
			kik_error_printf( KIK_DEBUG_TAG
				" Sixel width is shrinked (%d->%d) but"
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
				memmove( *pixels + (y * new_width * PIXEL_SIZE) ,
					*pixels + (y * cur_width * PIXEL_SIZE) ,
					new_width * PIXEL_SIZE) ;
			}

			return  1 ;
		}
	}
	else if( new_width == cur_width && new_height < cur_height)
	{
		/* do nothing */

		return  1 ;
	}

#ifdef  GDK_PIXBUF_VERSION
	if( new_width > SSIZE_MAX / PIXEL_SIZE / new_height)
#else
	if( new_width > (SSIZE_MAX - CARD_HEAD_SIZE) / PIXEL_SIZE / new_height)
#endif
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
	#ifdef  GDK_PIXBUF_VERSION
		if( ! ( p = (u_char*)realloc( *pixels , new_width * new_height * PIXEL_SIZE)))
	#else
		if( ( p = (u_char*)realloc( *pixels - CARD_HEAD_SIZE ,
				CARD_HEAD_SIZE + new_width * new_height * PIXEL_SIZE)))
		{
			p += CARD_HEAD_SIZE ;
		}
		else
	#endif
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " realloc failed.\n.") ;
		#endif
			return  0 ;
		}

		memset( p + cur_width * cur_height * PIXEL_SIZE , 0 ,
			new_width * (new_height - cur_height)) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Sixel data: %d %d -> calloc %d %d\n" ,
			cur_width , cur_height , new_width , new_height) ;
	#endif

		/* Cast to u_char* is necessary because this function can be compiled by g++. */
	#ifdef  GDK_PIXBUF_VERSION
		if( ! ( p = (u_char*)calloc( new_width * new_height , PIXEL_SIZE)))
	#else
		if( ( p = (u_char*)calloc( CARD_HEAD_SIZE +
				new_width * new_height * PIXEL_SIZE , 1)))
		{
			p += CARD_HEAD_SIZE ;
		}
		else
	#endif
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " calloc failed.\n.") ;
		#endif
			return  0 ;
		}

		for( y = 0 ; y < n_copy_rows ; y++)
		{
			memcpy( p + (y * new_width * PIXEL_SIZE) ,
				(*pixels) + (y * cur_width * PIXEL_SIZE) ,
				cur_width * PIXEL_SIZE) ;
		}

	#ifdef  GDK_PIXBUF_VERSION
		free( *pixels) ;
	#else
		if( *pixels)
		{
			free( (*pixels) - CARD_HEAD_SIZE) ;
		}
	#endif
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
	static pixel_t  default_color_tbl[] =
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
		SIXEL_RGB(33,60,60) , /* CYAN*/
		SIXEL_RGB(60,60,33) , /* YELLOW* */
		SIXEL_RGB(80,80,80)   /* GRAY 75% */
	} ;
#ifdef  USE_GRF
#define  color_tbl  sixel_cmap
	sixel_cmap_size = 16 ;
#else
	pixel_t  color_tbl[256] ;
#endif

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
	cur_width = cur_height = 0 ;
	width = 1024 ;
	height = 1024 ;

	/*  Cast to u_char* is necessary because this function can be compiled by g++. */
	if( ! realloc_pixels( &pixels , width , height , 0 , 0))
	{
		free( file_data) ;

		return  NULL ;
	}

	memcpy( color_tbl , default_color_tbl , sizeof(default_color_tbl)) ;
	memset( color_tbl + 16 , 0 , sizeof(color_tbl) - sizeof(default_color_tbl)) ;

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
	#if  0
		/* P1 */
		switch( *p)
		{
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
		}
	#else
		asp_x = 1 ;	/* XXX */
	#endif

		if( *(++p) != ';' || *p == '\0')
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;
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

#if  0
	if( *(++p) != ';')
	{
		/* P2 */
		switch( *p)
		{
		case '0':
		case '2':
			...
			break;

		default:
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;
		}

		if( *(++p) != ';' || *p == '\0')
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;
		}
	}
	else
	{
		/* P2 is omitted. */
		...
	}
#endif

	/* Ignoring P2 and P3 */
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

			switch(n)
			{
			case 4:
				height = params[3] ;
			case 3:
				width = params[2] ;
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
					/* XXX HLS */
				}
				else if( params[1] == 2 )
				{
					/* RGB */
					color_tbl[color] = SIXEL_RGB(K_MIN(params[2],100),
					                             K_MIN(params[3],100),
								     K_MIN(params[4],100)) ;

				#ifdef  USE_GRF
					if( sixel_cmap_size >= color)
					{
						sixel_cmap_size = color + 1 ;
					}
				#endif

				#ifdef  __DEBUG
					kik_debug_printf( KIK_DEBUG_TAG
						" Set rgb %x for color %d.\n" ,
						color_tbl[color] , color) ;
				#endif
				}
			}
		}
		else if( *p == '$')
		{
			pix_x = 0 ;
			rep = asp_x ;
		}
		else if( *p == '-')
		{
			pix_y += 6 ;
			rep = asp_x ;
		}
		else if( *p >= '?' && *p <= '\x7E')
		{
			u_int  new_width ;
			u_int  new_height ;
			int  a ;
			int  b ;
			int  y ;

			if( ! realloc_pixels( &pixels ,
					(new_width = width < pix_x + rep ? width * 2 : width) ,
					(new_height = height < pix_y + 6 ? height * 2 : height) ,
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
					#if  defined(GDK_PIXBUF_VERSION)
						/* RGBA */
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE] =
							(color_tbl[color] >> 16) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE + 1] =
							(color_tbl[color] >> 8) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE + 2] =
							(color_tbl[color]) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE + 3] = 0xff ;
					#elif  defined(SIXEL_1BPP)
						/* 0x80 is opaque mark */
						((pixel_t*)pixels)[(pix_y + y) * width +
							pix_x + x] =
							0x80 | color_tbl[color] ;
					#else
						/* ARGB (cardinal) */
						((pixel_t*)pixels)[(pix_y + y) * width +
							pix_x + x] =
							0xff000000 | color_tbl[color] ;
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

	if( cur_width == 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Nothing is drawn.\n") ;
	#endif

		free( pixels) ;

		return  NULL ;
	}

	realloc_pixels( &pixels , cur_width , cur_height , width , height) ;
	correct_height( (pixel_t*)pixels , cur_width , &cur_height) ;

	*width_ret = cur_width ;
	*height_ret = cur_height ;

	return  pixels ;
}
#undef  color_tbl

#ifndef  SIXEL_1BPP
#ifdef  GDK_PIXBUF_VERSION

static void
pixbuf_destroy_notify(
	guchar *  pixels ,
	gpointer  data
	)
{
	free( pixels) ;
}

static GdkPixbuf *
gdk_pixbuf_new_from_sixel(
	const char *  path
	)
{
	u_char *  pixels ;
	u_int  width ;
	u_int  height ;

	if( ! ( pixels = load_sixel_from_file( path , &width , &height)))
	{
		return  NULL ;
	}

	return  gdk_pixbuf_new_from_data( pixels , GDK_COLORSPACE_RGB , TRUE , 8 ,
			width , height , width * PIXEL_SIZE , pixbuf_destroy_notify , NULL) ;
}

#define  create_cardinals_from_sixel( path , width , height)  (NULL)

/* create an CARDINAL array for_NET_WM_ICON data */
static u_int32_t *
create_cardinals_from_pixbuf(
	GdkPixbuf *  pixbuf
	)
{
	u_int  width ;
	u_int  height ;
	u_int32_t *  cardinal ;
	int  rowstride ;
	u_char *  line ;
	u_char *  pixel ;
	u_int i , j ;

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;

	if( width > ((SSIZE_MAX / sizeof(*cardinal)) - 2) / height ||	/* integer overflow */
	    ! ( cardinal = malloc( ( width * height + 2) * sizeof(*cardinal))))
	{
		return  NULL ;
	}

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	/* format of the array is {width, height, ARGB[][]} */
	cardinal[0] = width ;
	cardinal[1] = height ;
	if( gdk_pixbuf_get_has_alpha( pixbuf))
	{
		for( i = 0 ; i < height ; i++)
		{
			pixel = line ;
			line += rowstride;
			for( j = 0 ; j < width ; j++)
			{
				/* RGBA to ARGB */
				cardinal[(i*width+j)+2] = ((((((u_int32_t)(pixel[3]) << 8)
								+ pixel[0]) << 8)
								+ pixel[1]) << 8) + pixel[2] ;
				pixel += 4 ;
			}
		}
	}
	else
	{
		for( i = 0 ; i < height ; i++)
		{
			pixel = line ;
			line += rowstride;
			for( j = 0 ; j < width ; j++)
			{
				/* all pixels are completely opaque (0xFF) */
				cardinal[(i*width+j)+2] = ((((((u_int32_t)(0x0000FF) <<8)
								+ pixel[0]) << 8)
								+ pixel[1]) << 8) + pixel[2] ;
				pixel += 3 ;
			}
		}
	}

	return  cardinal ;
}

static GdkPixbuf *
gdk_pixbuf_new_from(
	const char *  path
	)
{
	GdkPixbuf *  pixbuf ;

	if( ! strstr( path , ".six") || ! ( pixbuf = gdk_pixbuf_new_from_sixel( path)))
	{
	#if GDK_PIXBUF_MAJOR >= 2

	#if GDK_PIXBUF_MINOR >= 14
		if( strstr( path , "://"))
		{
			GFile *  file ;
			GInputStream *  in ;

			if( ( in = (GInputStream*)g_file_read(
					( file = g_vfs_get_file_for_uri(
							g_vfs_get_default() , path)) ,
					NULL , NULL)))
			{
				pixbuf = gdk_pixbuf_new_from_stream( in , NULL , NULL) ;
				g_object_unref( in) ;
			}
			else
			{
			#ifndef  G_PLATFORM_WIN32
				char *  cmd ;
			#endif

				pixbuf = NULL ;

				/* g_unix_input_stream_new doesn't exists on win32. */
			#ifndef  G_PLATFORM_WIN32
				if( ( cmd = alloca( 11 + strlen( path) + 1)))
				{
					FILE *  fp ;

					sprintf( cmd , "curl -k -s %s" , path) ;
					if( ( fp = popen( cmd , "r")))
					{
						in = g_unix_input_stream_new(
							fileno(fp) , FALSE) ;
						pixbuf = gdk_pixbuf_new_from_stream(
								in , NULL , NULL) ;
						pclose( fp) ;
					}
				}
			#endif
			}

			g_object_unref( file) ;
		}
		else
	#endif	/* GDK_PIXBUF_MINOR */
		{
			pixbuf = gdk_pixbuf_new_from_file( path , NULL) ;
		}

	#else	/* GDK_PIXBUF_MAJOR */

		pixbuf = gdk_pixbuf_new_from_file( path) ;

	#endif	/* GDK_PIXBUF_MAJOR */
	}

	return  pixbuf ;
}

#else	/* GDK_PIXBUF_VERSION */

#define  gdk_pixbuf_new_from_sixel(path)  (NULL)

static u_int32_t *
create_cardinals_from_sixel(
	const char *  path
	)
{
	u_int  width ;
	u_int  height ;
	u_int32_t *  cardinal ;

	if( ! ( cardinal = (u_int32_t*)load_sixel_from_file( path , &width , &height)))
	{
		return  NULL ;
	}

	cardinal -= 2 ;

	cardinal[0] = width ;
	cardinal[1] = height ;

	return  cardinal ;
}

#endif	/* GDK_PIXBUF_VERSION */
#endif  /* SIXEL_1BPP */

#undef  SIXEL_RGB
#undef  PIXEL_SIZE
#undef  CARD_HEAD_SIZE

#endif  /* BUILTIN_IMAGELIB */


#ifdef  USE_X11

/* seek the closest color */
static int
closest_color_index(
	XColor *  color_list ,
	int  len ,
	int  red ,
	int  green ,
	int  blue
	)
{
	int  closest = 0 ;
	int  i ;
	u_long  min = 0xffffff ;
	u_long  diff ;
	int  diff_r , diff_g , diff_b ;

	for( i = 0 ; i < len ; i++)
	{
		/* lazy color-space conversion*/
		diff_r = red - (color_list[i].red >> 8) ;
		diff_g = green - (color_list[i].green >> 8) ;
		diff_b = blue - (color_list[i].blue >> 8) ;
		diff = diff_r * diff_r *9 + diff_g * diff_g * 30 + diff_b * diff_b ;
		if ( diff < min)
		{
			min = diff ;
			closest = i ;
			/* no one may notice the difference (4[2^3/2]*4*9+4*4*30+4*4) */
			if ( diff < 640)
			{
				break ;
			}
		}
	}

	return  closest ;
}

/**Return position of the least significant bit
 *
 *\param val value to count
 *
 */
static int
lsb(
	u_int  val
	)
{
	int nth = 0 ;

	if( val == 0)
	{
		return  0 ;
	}

	while((val & 1) == 0)
	{
		val = val >> 1 ;
		nth ++ ;
	}

	return  nth ;
}

/**Return  position of the most significant bit
 *
 *\param val value to count
 *
 */
static int
msb(
	u_int val
	)
{
	int nth ;

	if( val == 0)
	{
		return  0 ;
	}

	nth = lsb( val) + 1 ;

	while(val & (1 << nth))
	{
		nth++ ;
	}

	return  nth ;
}

#endif	/* USE_X11 */
