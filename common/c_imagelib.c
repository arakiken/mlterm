/*
 *	$Id$
 */

#include <sys/stat.h>	/* fstat */
#include <kiklib/kik_util.h>	/* K_MIN */


#define  SIXEL_RGB(r,g,b)  ((((r)*255/100) << 16) | (((g)*255/100) << 8) | ((b)*255/100))


/* --- static functions --- */

#if  (defined(ENABLE_SIXEL) && defined(GDK_PIXBUF_VERSION)) || defined(FORCE_ENABLE_SIXEL)

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

	p = *pixels ;
	n_copy_rows = K_MIN(new_height,cur_height) ;

	if( new_width < cur_width)
	{
		for( y = 1 ; y < n_copy_rows ; y++)
		{
			memmove( p + (y * new_width * 3) , p + (y * cur_width * 3) ,
				new_width * 3) ;
		}
	}
	else if( new_width > cur_width)
	{
		/*  Cast to u_char* is necessary because this function can be compiled by g++. */
		if( ! ( p = (u_char*)realloc( p , new_width * new_height * 3)))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " realloc failed.\n.") ;
		#endif
			return  0 ;
		}

		for( y = n_copy_rows - 1 ; y >= 0 ; y--)
		{
			memmove( p + (y * new_width * 3) , p + (y * cur_width * 3) ,
				new_width * 3) ;
		}

		*pixels = p ;
	}

	return  1 ;
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
	u_int32_t  color_tbl[256] ;
	/* VT340 Default Color Map */
	static u_int32_t  default_color_tbl[] =
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
		return  NULL ;
	}

	len = fread( p , 1 , st.st_size , fp) ;
	fclose( fp) ;
	p[len] = '\0' ;

	cur_width = cur_height = 0 ;
	width = 1024 ;
	height = 1024 ;

	/*  Cast to u_char* is necessary because this function can be compiled by g++. */
	if( ! ( pixels = (u_char*)malloc( 3 * width * height)))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " malloc failed.\n.") ;
	#endif
		goto  end ;
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
		/* P1 */
		switch( *p)
		{
		case '0':
		case '1':
		case '5':
		case '6':
			asp_x = 2 ;
			break;

		case '2':
			asp_x = 5 ;
			break;

		case '3':
		case '4':
			asp_x = 3 ;
			break;

		case '7':
		case '8':
		case '9':
			asp_x = 1 ;
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
		/* P1 is omitted. */
		asp_x = 2 ;
	}

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

			n = get_params( params , 4 , &p) ;

			switch(n)
			{
			case 4:
				height = params[3] ;
			case 3:
				width = params[2] ;
			case 2:
				asp_x = params[0] / params[1] ;	/* XXX */
				break ;
			case 1:
				asp_x = params[0] ;
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
			int  a ;
			int  b ;
			int  y ;

			if( width < pix_x + rep || height < pix_y + 6)
			{
				if( ! realloc_pixels( &pixels , width * 2 , height * 2 ,
						width , height))
				{
					break ;
				}

				width *= 2 ;
				height *= 2 ;
			}

			b = *p - '?' ;
			a = 0x01 ;

			for( y = 0 ; y < 6 ; y++ )
			{
				if( (b & a) != 0)
				{
					int  x ;

					for( x = 0 ; x < rep ; x ++)
					{
						pixels[((pix_y + y) * width + pix_x + x) * 3] =
							(color_tbl[color] >> 16) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) * 3 + 1] =
							(color_tbl[color] >> 8) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) * 3 + 2] =
							(color_tbl[color]) & 0xff ;
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

	*width_ret = cur_width ;
	*height_ret = cur_height ;

	return  pixels ;
}

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

	return  gdk_pixbuf_new_from_data( pixels , GDK_COLORSPACE_RGB , FALSE , 8 ,
			width , height , width * 3 , pixbuf_destroy_notify , NULL) ;
}

#define  create_cardinals_from_sixel( path , width , height)  (NULL)

#else

#define  gdk_pixbuf_new_from_sixel(path)  (NULL)

static u_int32_t *
create_cardinals_from_sixel(
	const char *  path
	)
{
	u_int  width ;
	u_int  height ;
	u_char *  pixels ;
	u_char *  pix_p ;
	u_int32_t *  cardinal ;
	u_int32_t *  card_p ;
	int  x ;
	int  y ;

	if( ! ( pix_p = pixels = load_sixel_from_file( path , &width , &height)))
	{
		return  NULL ;
	}

	/*  Cast to u_int32_t* is necessary because this function can be compiled by g++. */
	if( width > ((SSIZE_MAX / sizeof(*cardinal)) - 2) / height ||	/* integer overflow */
	    ! ( card_p = cardinal = (u_int32_t*)malloc( (width * height + 2) * sizeof(*cardinal))))
	{
		return  NULL ;
	}

	*(card_p++) = width ;
	*(card_p++) = height ;

	for( y = 0 ; y < height ; y++)
	{
		for( x = 0 ; x < width ; x++)
		{
			*(card_p++) = 0xff000000 | pix_p[0] << 16 | pix_p[1] << 8 | pix_p[2] ;
			pix_p += 3 ;
		}
	}

	free( pixels) ;

	return  cardinal ;
}

#endif	/* GDK_PIXBUF_VERSION */

#endif  /* ENABLE_SIXEL */


#ifdef  GDK_PIXBUF_VERSION

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

	/* create (maybe shriked) copy */
	pixbuf = gdk_pixbuf_scale_simple( pixbuf , width , height , GDK_INTERP_TILES) ;

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

	g_object_unref( pixbuf) ;

	return  cardinal ;
}

#endif	/* GDK_PIXBUF_VERSION */

#ifndef  USE_WIN32GUI

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
			/* no one may notice the difference */
			if ( diff < 31)
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

#endif	/* USE_WIN32GUI */
