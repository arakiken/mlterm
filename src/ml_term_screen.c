/*
 *	$Id$
 */

#include  "ml_term_screen.h"

#include  <X11/keysym.h>	/* XK_xxx */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_util.h>	/* K_MIN */
#include  <kiklib/kik_locale.h>	/* kik_get_locale */
#include  <mkf/mkf_xct_parser.h>
#include  <mkf/mkf_xct_conv.h>
#include  <mkf/mkf_utf8_conv.h>
#include  <mkf/mkf_utf8_parser.h>

#include  "ml_str_parser.h"
#include  "ml_xic.h"
#include  "ml_picture.h"


/*
 * XXX
 *
 * char length is max 8 bytes.
 *
 * combining chars may be max 3 per char.
 *
 * I think this is enough , but I'm not sure.
 */
#define  UTF8_MAX_CHAR_SIZE  (8 * 4)

/*
 * XXX
 *
 * char prefixes are max 4 bytes.
 * additional 3 bytes + cs name len ("viscii1.1-1" is max 11 bytes) = 14 bytes for iso2022
 * extension.
 * char length is max 2 bytes.
 * (total 20 bytes)
 *
 * combining chars is max 3 per char.
 *
 * I think this is enough , but I'm not sure.
 */
#define  XCT_MAX_CHAR_SIZE  (20 * 4)

/* the same as rxvt. if this size is too few , we may drop sequences from kinput2. */
#define  KEY_BUF_SIZE  512

#define  HAS_SYSTEM_LISTENER(termscr,function) \
	((termscr)->system_listener && (termscr)->system_listener->function)

#define  HAS_SCROLL_LISTENER(termscr,function) \
	((termscr)->screen_scroll_listener && (termscr)->screen_scroll_listener->function)


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static int
convert_row_to_y(
	ml_term_screen_t *  termscr ,
	int  row
	)
{
	return  ml_line_height( termscr->font_man) * row ;
}

static int
convert_y_to_row(
	ml_term_screen_t *  termscr ,
	int *  y_rest ,
	int  y
	)
{
	int  row ;

	row = y / ml_line_height( termscr->font_man) ;

	if( y_rest)
	{
		*y_rest = y - row * ml_line_height( termscr->font_man) ;
	}

	return  row ;
}

static int
convert_x_to_char_index(
	ml_term_screen_t *  termscr ,
	ml_image_line_t *  line ,
	int *  x_rest ,
	int  x
	)
{
	if( termscr->font_present & FONT_VAR_WIDTH)
	{
		return  ml_convert_x_to_char_index( line , x_rest , x , termscr->shape) ;
	}
	else
	{
		return  ml_convert_x_to_char_index( line , x_rest , x , NULL) ;
	}
}

static int
convert_char_index_to_x(
	ml_term_screen_t *  termscr ,
	ml_image_line_t *  line ,
	int  char_index
	)
{
	if( termscr->font_present & FONT_VAR_WIDTH) 
	{
		return  ml_convert_char_index_to_x( line , char_index , termscr->shape) ;
	}
	else
	{
		return  ml_convert_char_index_to_x( line , char_index , NULL) ;
	}
}


/*
 * drawing screen functions.
 */
 
static int
draw_line(
	ml_term_screen_t *  termscr ,
	ml_image_line_t *  line ,
	int  y
	)
{
	if( ml_imgline_is_empty( line))
	{
		ml_window_clear( &termscr->window , 0 , y , termscr->window.width ,
			ml_line_height(termscr->font_man)) ;
	}
	else
	{
		int  beg_char_index ;
		int  beg_x ;
		u_int  num_of_redrawn ;
		ml_image_line_t *  orig ;

		if( termscr->shape)
		{
			if( ( orig = ml_imgline_shape( line , termscr->shape)) == NULL)
			{
				return  0 ;
			}
		}
		else
		{
			orig = NULL ;
		}
		
		beg_char_index = ml_imgline_get_beg_of_modified( line) ;
		
		/* 3rd argument is NULL since line is already shaped */
		beg_x = ml_convert_char_index_to_x( line , beg_char_index , NULL) ;

		if( termscr->font_present & FONT_VAR_WIDTH)
		{
			num_of_redrawn = line->num_of_filled_chars - beg_char_index ;
		}
		else
		{
			num_of_redrawn = ml_imgline_get_num_of_redrawn_chars( line) ;
		}

		if( line->is_cleared_to_end || termscr->font_present & FONT_VAR_WIDTH)
		{
			if( ! ml_window_draw_str_to_eol( &termscr->window , &line->chars[beg_char_index] ,
				num_of_redrawn , beg_x , y ,
				ml_line_height( termscr->font_man) ,
				ml_line_height_to_baseline( termscr->font_man)))
			{
				return  0 ;
			}
		}
		else
		{
			if( ! ml_window_draw_str( &termscr->window , &line->chars[beg_char_index] ,
				num_of_redrawn , beg_x , y ,
				ml_line_height( termscr->font_man) ,
				ml_line_height_to_baseline( termscr->font_man)))
			{
				return  0 ;
			}
		}

		if( orig)
		{
			ml_imgline_unshape( line , orig) ;
		}
	}

	return  1 ;
}

static int
draw_cursor(
	ml_term_screen_t *  termscr ,
	int  restore
	)
{
	int  x ;
	int  y ;
	ml_image_line_t *  line ;
	ml_image_line_t *  orig ;
	ml_char_t *  ch ;

	y = convert_row_to_y( termscr , ml_cursor_row( termscr->image)) ;
	
	if( ( line = ml_image_get_line( termscr->image , ml_cursor_row( termscr->image))) == NULL ||
		ml_imgline_is_empty( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " cursor line doesn't exist.\n") ;
	#endif

		return  0 ;
	}

	if( termscr->shape)
	{
		if( ( orig = ml_imgline_shape( line , termscr->shape)) == NULL)
		{
			return  0 ;
		}
	}
	else
	{
		orig = NULL ;
	}
	
	/* 3rd argument is NULL since line is already shaped */
	x = ml_convert_char_index_to_x( line , ml_cursor_char_index( termscr->image) , NULL) ;
	
	ch = &line->chars[ ml_cursor_char_index( termscr->image)] ;

	if( restore)
	{
		ml_window_draw_str( &termscr->window , ch , 1 , x , y ,
			ml_line_height( termscr->font_man) ,
			ml_line_height_to_baseline( termscr->font_man)) ;
	}
	else
	{
		if( termscr->is_focused)
		{
			ml_window_draw_cursor( &termscr->window , ch , x , y ,
				ml_line_height( termscr->font_man) ,
				ml_line_height_to_baseline( termscr->font_man)) ;
		}
		else
		{
			ml_window_draw_rect_frame( &termscr->window ,
				x + 2 , y + 2 , x + ml_char_width(ch) + 1 , y + ml_char_height(ch) + 1) ;
		}
	}

	if( orig)
	{
		ml_imgline_unshape( line , orig) ;
	}

	return  1 ;
}

static int
flush_scroll_cache(
	ml_term_screen_t *  termscr ,
	int  scroll_actual_screen
	)
{
	if( ! termscr->scroll_cache_rows)
	{
		return  0 ;
	}
	
	if( scroll_actual_screen && ml_window_is_scrollable( &termscr->window))
	{
		int  start_y ;
		int  end_y ;
		u_int  scroll_height ;

		scroll_height = ml_line_height( termscr->font_man) * abs( termscr->scroll_cache_rows) ;

		if( scroll_height < termscr->window.height)
		{
			start_y = convert_row_to_y( termscr ,
				termscr->scroll_cache_boundary_start) ;
			end_y = start_y +
				ml_line_height( termscr->font_man) *
				(termscr->scroll_cache_boundary_end -
				termscr->scroll_cache_boundary_start + 1) ;

			if( termscr->scroll_cache_rows > 0)
			{
				ml_window_scroll_upward_region( &termscr->window ,
					start_y , end_y , scroll_height) ;
			}
			else
			{
				ml_window_scroll_downward_region( &termscr->window ,
					start_y , end_y , scroll_height) ;
			}
		}
	#if  0
		else
		{
			ml_window_clear_all( &termscr->window) ;
		}
	#endif
	}
	else
	{
		/*
		 * setting modified mark to the lines within scroll region.
		 */

		int  row ;
		ml_image_line_t *  line ;

		if( termscr->scroll_cache_rows > 0)
		{
			/*
			 * scrolling upward.
			 */
			for( row = termscr->scroll_cache_boundary_start ;
				row <= termscr->scroll_cache_boundary_end - termscr->scroll_cache_rows ;
				row ++)
			{
				if( ( line = ml_image_get_line( termscr->image , row)))
				{
					ml_imgline_set_modified_all( line) ;
				}
			}
		}
		else
		{
			/*
			 * scrolling downward.
			 */
			for( row = termscr->scroll_cache_boundary_end ;
				row >= termscr->scroll_cache_boundary_start - termscr->scroll_cache_rows ;
				row --)
			{
				if( ( line = ml_image_get_line( termscr->image , row)))
				{
					ml_imgline_set_modified_all( line) ;
				}
			}
		}
	}

	termscr->scroll_cache_rows = 0 ;

	return  1 ;
}

static void
set_scroll_boundary(
	ml_term_screen_t *  termscr ,
	int  boundary_start ,
	int  boundary_end
	)
{
	if( termscr->scroll_cache_rows &&
		(termscr->scroll_cache_boundary_start != boundary_start ||
		termscr->scroll_cache_boundary_end != boundary_end))
	{
		flush_scroll_cache( termscr , 0) ;
	}

	termscr->scroll_cache_boundary_start = boundary_start ;
	termscr->scroll_cache_boundary_end = boundary_end ;
}

static int
redraw_image(
	ml_term_screen_t *  termscr
	)
{
	int  counter ;
	ml_image_line_t *  line ;
	int  y ;
	int  end_y ;
	int  beg_y ;

	flush_scroll_cache( termscr , 1) ;

	counter = 0 ;
	while(1)
	{
		if( ( line = ml_bs_get_image_line_in_screen( &termscr->bs_image , counter)) == NULL)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " nothing is redrawn.\n") ;
		#endif
		
			return  1 ;
		}
		
		if( line->is_modified)
		{
			break ;
		}

		counter ++ ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " redrawing -> line %d\n" , counter) ;
#endif

	beg_y = end_y = y = convert_row_to_y( termscr , counter) ;

	draw_line( termscr , line , y) ;
	ml_imgline_is_updated( line) ;

	counter ++ ;
	y += ml_line_height( termscr->font_man) ;
	end_y = y ;

	while( ( line = ml_bs_get_image_line_in_screen( &termscr->bs_image , counter)) != NULL)
	{
		if( line->is_modified)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " redrawing -> line %d\n" , counter) ;
		#endif

			draw_line( termscr , line , y) ;
			ml_imgline_is_updated( line) ;

			y += ml_line_height( termscr->font_man) ;
			end_y = y ;
		}
		else
		{
			y += ml_line_height( termscr->font_man) ;
		}
	
		counter ++ ;
	}

	return  1 ;
}

static int
highlight_cursor(
	ml_term_screen_t *  termscr
	)
{
	if( ! termscr->is_cursor_visible || ml_is_backscroll_mode( &termscr->bs_image))
	{
		return  1 ;
	}

	flush_scroll_cache( termscr , 1) ;

	ml_highlight_cursor( termscr->image) ;

	draw_cursor( termscr , 0) ;

	/* restoring color as soon as highlighted cursor is drawn. */
	ml_unhighlight_cursor( termscr->image) ;

	ml_xic_set_spot( &termscr->window) ;

	return  1 ;
}

/*
 * this doesn't consider backscroll mode.
 */
static int
unhighlight_cursor(
	ml_term_screen_t *  termscr
	)
{
	if( ! termscr->is_cursor_visible || ml_is_backscroll_mode( &termscr->bs_image))
	{
		return  1 ;
	}
	
	flush_scroll_cache( termscr , 1) ;

	draw_cursor( termscr , 1) ;
	
	return  1 ;
}


/*
 * {enter|exit}_backscroll_mode() and bs_XXX() functions provides backscroll operations.
 */
 
static void
enter_backscroll_mode(
	ml_term_screen_t *  termscr
	)
{
	if( ml_is_backscroll_mode( &termscr->bs_image))
	{
		return ;
	}
	
	ml_set_backscroll_mode( &termscr->bs_image) ;

	if( HAS_SCROLL_LISTENER(termscr,bs_mode_entered))
	{
		(*termscr->screen_scroll_listener->bs_mode_entered)(
			termscr->screen_scroll_listener->self) ;
	}
}

static void
exit_backscroll_mode(
	ml_term_screen_t *  termscr
	)
{
	if( ! ml_is_backscroll_mode( &termscr->bs_image))
	{
		return ;
	}
	
	ml_unset_backscroll_mode( &termscr->bs_image) ;
	ml_image_set_modified_all( termscr->image) ;
	
	if( HAS_SCROLL_LISTENER(termscr,bs_mode_exited))
	{
		(*termscr->screen_scroll_listener->bs_mode_exited)(
			termscr->screen_scroll_listener->self) ;
	}
}

static void
bs_scroll_upward(
	ml_term_screen_t *  termscr
	)
{
	if( ml_bs_scroll_upward( &termscr->bs_image , 1))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_upward))
		{
			(*termscr->screen_scroll_listener->scrolled_upward)(
				termscr->screen_scroll_listener->self , 1) ;
		}
	}	
}

static void
bs_scroll_downward(
	ml_term_screen_t *  termscr
	)
{
	if( ml_bs_scroll_downward( &termscr->bs_image , 1))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_downward))
		{
			(*termscr->screen_scroll_listener->scrolled_downward)(
				termscr->screen_scroll_listener->self , 1) ;
		}
	}
}

static void
bs_half_page_upward(
	ml_term_screen_t *  termscr
	)
{
	if( ml_bs_scroll_upward( &termscr->bs_image , ml_image_get_rows( termscr->image) / 2))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_upward))
		{
			(*termscr->screen_scroll_listener->scrolled_upward)(
				termscr->screen_scroll_listener->self ,
				ml_image_get_rows( termscr->image) / 2) ;
		}
	}
}

static void
bs_half_page_downward(
	ml_term_screen_t *  termscr
	)
{
	if( ml_bs_scroll_downward( &termscr->bs_image , ml_image_get_rows( termscr->image) / 2))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_downward))
		{
			(*termscr->screen_scroll_listener->scrolled_downward)(
				termscr->screen_scroll_listener->self ,
				ml_image_get_rows( termscr->image) / 2) ;
		}
	}	
}

static void
bs_page_upward(
	ml_term_screen_t *  termscr
	)
{
	if( ml_bs_scroll_upward( &termscr->bs_image , ml_image_get_rows( termscr->image)))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_upward))
		{
			(*termscr->screen_scroll_listener->scrolled_upward)(
				termscr->screen_scroll_listener->self ,
				ml_image_get_rows( termscr->image)) ;
		}
	}
}

static void
bs_page_downward(
	ml_term_screen_t *  termscr
	)
{
	if( ml_bs_scroll_downward( &termscr->bs_image , ml_image_get_rows( termscr->image)))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_downward))
		{
			(*termscr->screen_scroll_listener->scrolled_downward)(
				termscr->screen_scroll_listener->self ,
				ml_image_get_rows( termscr->image)) ;
		}
	}	
}


static void
write_to_pty(
	ml_term_screen_t *  termscr ,
	u_char *  str ,			/* str may be NULL */
	size_t  len ,
	mkf_parser_t *  parser		/* parser may be NULL */
	)
{
	if( termscr->pty == NULL)
	{
		return ;
	}
	
	if( parser && str)
	{
		(*parser->init)( parser) ;
		(*parser->set_str)( parser , str , len) ;
	}

	(*termscr->encoding_listener->init)( termscr->encoding_listener->self , 0) ;

	if( parser)
	{
		u_char  conv_buf[512] ;
		size_t  filled_len ;

		while( ! parser->is_eos)
		{
		#ifdef  __DEBUG
			{
				int  i ;

				kik_debug_printf( KIK_DEBUG_TAG " written str:\n") ;
				for( i = 0 ; i < len ; i ++)
				{
					fprintf( stderr , "[%.2x]" , str[i]) ;
				}
				fprintf( stderr , "=>\n") ;
			}
		#endif

			if( ( filled_len = (*termscr->encoding_listener->convert)(
				termscr->encoding_listener->self , conv_buf , sizeof( conv_buf) , parser))
				== 0)
			{
				break ;
			}

		#ifdef  __DEBUG
			{
				int  i ;

				for( i = 0 ; i < filled_len ; i ++)
				{
					fprintf( stderr , "[%.2x]" , conv_buf[i]) ;
				}
				fprintf( stderr , "\n") ;
			}
		#endif

			ml_write_to_pty( termscr->pty , conv_buf , filled_len) ;
		}
	}
	else if( str)
	{
	#ifdef  __DEBUG
		{
			int  i ;

			kik_debug_printf( KIK_DEBUG_TAG " written str: ") ;
			for( i = 0 ; i < len ; i ++)
			{
				fprintf( stderr , "%.2x" , str[i]) ;
			}
			fprintf( stderr , "\n") ;
		}
	#endif

		ml_write_to_pty( termscr->pty , str , len) ;
	}
	else
	{
		return ;
	}
}


static int
set_wall_picture(
	ml_term_screen_t *  termscr
	)
{
	ml_picture_t  pic ;
	
	if( ! termscr->pic_file_path)
	{
		return  0 ;
	}
	
	if( ! ml_picture_init( &pic , &termscr->window))
	{
		goto  error ;
	}
	
	if( ! ml_picture_load( &pic , termscr->pic_file_path))
	{
		kik_msg_printf( " wall picture file %s is not found.\n" ,
			termscr->pic_file_path) ;

		ml_picture_final( &pic) ;
		
		goto  error ;
	}
	
	if( ! ml_window_set_wall_picture( &termscr->window , pic.pixmap))
	{
		ml_picture_final( &pic) ;
		
		goto  error ;
	}
	else
	{
		ml_picture_final( &pic) ;

		return  1 ;
	}

error:
	free( termscr->pic_file_path) ;
	termscr->pic_file_path = NULL ;

	ml_window_unset_wall_picture( &termscr->window) ;

	return  0 ;
}

static int
get_mod_meta_mask(
	Display *  display
	)
{
	int  mask_counter ;
	int  kc_counter ;
	XModifierKeymap *  mod_map ;
	KeyCode *  key_codes ;
	KeySym  sym ;
	int  mod_masks[] = { Mod1Mask , Mod2Mask , Mod3Mask , Mod4Mask , Mod5Mask } ;

	mod_map = XGetModifierMapping( display) ;
	key_codes = mod_map->modifiermap ;
	
	for( mask_counter = 0 ; mask_counter < 6 ; mask_counter++)
	{
		int  counter ;

		/*
		 * KeyCodes order is like this.
		 * Shift[max_keypermod] Lock[max_keypermod] Control[max_keypermod]
		 * Mod1[max_keypermod] Mod2[max_keypermod] Mod3[max_keypermod]
		 * Mod4[max_keypermod] Mod5[max_keypermod]
		 */

		/* skip shift/lock/control */
		kc_counter = (mask_counter + 3) * mod_map->max_keypermod ;
		
		for( counter = 0 ; counter < mod_map->max_keypermod ; counter++)
		{
			if( key_codes[kc_counter] == 0)
			{
				break ;
			}

			sym = XKeycodeToKeysym( display , key_codes[kc_counter] , 0) ;

			if( sym == XK_Meta_L || sym == XK_Meta_R ||
				sym == XK_Alt_L || sym == XK_Alt_R ||
				sym == XK_Super_L || sym == XK_Super_R ||
				sym == XK_Hyper_L || sym == XK_Hyper_R)
			{
				XFreeModifiermap( mod_map) ;
				
				return  mod_masks[mask_counter] ;
			}

			kc_counter ++ ;
		}
	}
	
	XFreeModifiermap( mod_map) ;

	return  0 ;
}

static int
select_iscii_lang(
	ml_term_screen_t *  termscr
	)
{
	u_int  font_size ;
	char *  font_name ;

	if( termscr->iscii_state == NULL)
	{
		return  0 ;
	}

	if( ! ml_iscii_select_lang( termscr->iscii_state , termscr->iscii_lang))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_select_lang() failed.\n") ;
	#endif
	
		return  0 ;
	}

	for( font_size = termscr->font_man->font_custom->min_font_size ;
		font_size <= termscr->font_man->font_custom->max_font_size ;
		font_size ++)
	{
		if( ( font_name = ml_iscii_get_font_name( termscr->iscii_state , font_size)) == NULL)
		{
			continue ;
		}
		
		ml_font_manager_set_local_font_name( termscr->font_man ,
			DEFAULT_FONT_ATTR(ISCII) , font_name , font_size) ;
	}

	/*
	 * XXX
	 * anti alias ISCII font is not supported.
	 */
	 	
	if( ! (termscr->font_present & FONT_AA))
	{
		ml_font_manager_reload( termscr->font_man) ;
	}
	
	return  1 ;
}

/*
 * updating  ml_logical_visual_t/ml_shape_t/ml_iscii_state_t
 */
static int
update_encoding_proper_aux(
	ml_term_screen_t *  termscr
	)
{
	if( termscr->logvis)
	{
		(*termscr->logvis->logical)( termscr->logvis) ;
		(*termscr->logvis->delete)( termscr->logvis) ;
	}
	
	if( termscr->shape)
	{
		(*termscr->shape->delete)( termscr->shape) ;
	}

	if( (*termscr->encoding_listener->encoding)(termscr->encoding_listener->self) == ML_ISCII)
	{
		if( termscr->iscii_state == NULL)
		{
			if( ( termscr->iscii_state = ml_iscii_new()) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_new() failed.\n") ;
			#endif
				
				return  0 ;
			}

			if( ! select_iscii_lang( termscr))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " select_iscii_lang() failed.\n") ;
			#endif
			
				ml_iscii_delete( termscr->iscii_state) ;
				termscr->iscii_state = NULL ;

				return  0 ;
			}
		}
		
		if( ( termscr->logvis = ml_logvis_iscii_new(
						termscr->image , termscr->iscii_state)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_iscii_new() failed.\n") ;
		#endif

			return  0 ;
		}
		
		if( ( termscr->shape = ml_iscii_shape_new( termscr->iscii_state)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_shape_new() failed.\n") ;
		#endif
		
			return  0 ;
		}
	}
	else
	{
		if( termscr->iscii_state)
		{
			ml_iscii_delete( termscr->iscii_state) ;
			termscr->iscii_state = NULL ;
		}
		
		if( termscr->use_bidi &&
			(*termscr->encoding_listener->encoding)(termscr->encoding_listener->self) == ML_UTF8)
		{
			if( ( termscr->logvis = ml_logvis_bidi_new( termscr->image)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_bidi_new() failed.\n") ;
			#endif

				return  0 ;
			}

			if( ( termscr->shape = ml_arabic_shape_new()) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_arabic_shape_new() failed.\n") ;
			#endif

				return  0 ;
			}
		}
		else
		{
			termscr->logvis = NULL ;
			termscr->shape = NULL ;
		}
	}

	if( termscr->logvis)
	{
		(*termscr->logvis->render)( termscr->logvis) ;
		(*termscr->logvis->visual)( termscr->logvis) ;
	}

	return  1 ;
}


/*
 * callbacks of ml_window events
 */

static void
window_realized(
	ml_window_t *  win
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;
		
	termscr->mod_meta_mask = get_mod_meta_mask( termscr->window.display) ;

	if( termscr->xim_open_in_startup)
	{
		ml_xic_activate( &termscr->window , "" , "") ;
	}

	set_wall_picture( termscr) ;
}

static void
window_exposed(
	ml_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	int  counter ;
	int  beg_row ;
	int  end_row ;
	ml_term_screen_t *  termscr ;
	ml_image_line_t *  line ;
	
	termscr = (ml_term_screen_t *) win ;

	beg_row = convert_y_to_row( termscr , NULL , y) ;
	end_row = convert_y_to_row( termscr , NULL , y + height) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " exposed [row] from %d to %d [y] from %d to %d\n" ,
		beg_row , end_row , y , y + height) ;
#endif
	
	for( counter = beg_row ; counter <= end_row ; counter ++)
	{
		if( ( line = ml_bs_get_image_line_in_screen( &termscr->bs_image , counter)) == NULL)
		{
			break ;
		}

		ml_imgline_set_modified_all( line) ;
	}
	
	redraw_image( termscr) ;

	if( ml_convert_row_to_bs_row( &termscr->bs_image , beg_row) <= ml_cursor_row( termscr->image) &&
		ml_cursor_row( termscr->image) <= ml_convert_row_to_bs_row( &termscr->bs_image , end_row))
	{
		highlight_cursor( termscr) ;
	}
}

static void
window_resized(
	ml_window_t *  win
	)
{
	ml_term_screen_t *  termscr ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " term screen resized.\n") ;
#endif
	
	termscr = (ml_term_screen_t *) win ;

	ml_restore_selected_region_color( &termscr->sel) ;
	exit_backscroll_mode( termscr) ;

	unhighlight_cursor( termscr) ;

	/*
	 * !! Notice !!
	 * ml_image_resize() modifies screen image that visual to logical should be done
	 * before ml_image_resize() is called.
	 */
	if( termscr->logvis)
	{
		(*termscr->logvis->logical)( termscr->logvis) ;
	}
	
	ml_image_resize( &termscr->normal_image ,
		termscr->window.width / ml_col_width( termscr->font_man) ,
		termscr->window.height / ml_line_height( termscr->font_man)) ;

	ml_image_resize( &termscr->alt_image ,
		termscr->window.width / ml_col_width( termscr->font_man) ,
		termscr->window.height / ml_line_height( termscr->font_man)) ;

	if( termscr->logvis)
	{
		(*termscr->logvis->render)( termscr->logvis) ;
		(*termscr->logvis->visual)( termscr->logvis) ;
	}
	
	if( termscr->pty)
	{
		ml_set_pty_winsize( termscr->pty , ml_image_get_cols( termscr->image) ,
			ml_image_get_rows( termscr->image)) ;
	}

	set_wall_picture( termscr) ;

	redraw_image( termscr) ;

	highlight_cursor( termscr) ;
}

static void
window_focused(
	ml_window_t *  win
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t *) win ;

	if( termscr->is_focused)
	{
		return ;
	}
	
	unhighlight_cursor( termscr) ;

	termscr->is_focused = 1 ;
	
	ml_window_unfade( win) ;
	
	highlight_cursor( termscr) ;
}

static void
window_unfocused(
	ml_window_t *  win
	)
{
	ml_term_screen_t *  termscr ;
	
	termscr = (ml_term_screen_t *) win ;

	if( ! termscr->is_focused)
	{
		return ;
	}
	
	unhighlight_cursor( termscr) ;
	
	termscr->is_focused = 0 ;
	
	ml_window_fade( win , termscr->fade_ratio) ;

	highlight_cursor( termscr) ;
}

/*
 * the finalizer of ml_term_screen_t.
 * 
 * ml_window_final(term_screen) -> window_finalized(term_screen)
 */
static void
window_finalized(
	ml_window_t *  win
	)
{
	ml_term_screen_delete( (ml_term_screen_t*)win) ;
}

static void
window_deleted(
	ml_window_t *  win
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	if( HAS_SYSTEM_LISTENER(termscr,close_pty))
	{
		(*termscr->system_listener->close_pty)( termscr->system_listener->self ,
			ml_get_root_window( &termscr->window)) ;
	}
}

static void
config_menu(
	ml_term_screen_t *  termscr ,
	int  x ,
	int  y
	)
{
	int  global_x ;
	int  global_y ;
	Window  child ;

	XTranslateCoordinates( termscr->window.display , termscr->window.my_window ,
		DefaultRootWindow( termscr->window.display) , x , y ,
		&global_x , &global_y , &child) ;

	ml_config_menu_start( &termscr->config_menu , global_x , global_y ,
		(*termscr->encoding_listener->encoding)( termscr->encoding_listener->self) ,
		termscr->iscii_lang , ml_window_get_fg_color( &termscr->window) ,
		ml_window_get_bg_color( &termscr->window) ,
		termscr->image->tab_size , ml_get_log_size( &termscr->logs) ,
		termscr->font_man->font_size ,
		termscr->font_man->font_custom->min_font_size ,
		termscr->font_man->font_custom->max_font_size ,
		termscr->mod_meta_mode , termscr->bel_mode ,
		ml_is_char_combining() , termscr->copy_paste_via_ucs ,
		termscr->window.is_transparent , termscr->fade_ratio ,
		termscr->font_present , termscr->use_bidi ,
		ml_xic_get_xim_name( &termscr->window) , kik_get_locale()) ;
}

static int
use_utf8_selection(
	ml_term_screen_t *  termscr
	)
{
	ml_char_encoding_t  encoding ;

	encoding = (*termscr->encoding_listener->encoding)( termscr->encoding_listener->self) ;

	if( encoding == UTF8)
	{
		return  1 ;
	}
	else if( IS_UCS_SUBSET_ENCODING(encoding) && termscr->copy_paste_via_ucs)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static int
yank_event_received(
	ml_term_screen_t *  termscr ,
	Time  time
	)
{
	if( termscr->sel.is_owner)
	{
		if( termscr->sel.sel_str == NULL || termscr->sel.sel_len == 0)
		{
			return  0 ;
		}
		
		(*termscr->ml_str_parser->init)( termscr->ml_str_parser) ;
		ml_str_parser_set_str( termscr->ml_str_parser ,
			termscr->sel.sel_str , termscr->sel.sel_len) ;
		
		write_to_pty( termscr , NULL , 0 , termscr->ml_str_parser) ;

		return  1 ;
	}
	else
	{
		if( use_utf8_selection(termscr))
		{
			return  ml_window_utf8_selection_request( &termscr->window , time) ;
		}
		else
		{
			return  ml_window_xct_selection_request( &termscr->window , time) ;
		}
	}
}

static void
key_pressed(
	ml_window_t *  win ,
	XKeyEvent *  event
	)
{
	ml_term_screen_t *  termscr ;
	size_t  size ;
	u_char  seq[KEY_BUF_SIZE] ;
	KeySym  ksym ;
	mkf_parser_t *  parser ;

	termscr = (ml_term_screen_t *) win ;

	size = ml_window_get_str( win , seq , sizeof(seq) , &parser , &ksym , event) ;

#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " received sequence =>") ;
		for( i = 0 ; i < size ; i ++)
		{
			printf( "%.2x" , seq[i]) ;
		}
		printf( "\n") ;
	}
#endif

	if( (*termscr->encoding_listener->encoding)( termscr->encoding_listener->self) == ML_ISCII)
	{
		ml_iscii_keyb_t  keyb ;

		keyb = ml_iscii_current_keyb( termscr->iscii_state) ;
		
		if( ksym == XK_Alt_R)
		{
			if( keyb == ISCIIKEYB_NONE)
			{
				ml_set_window_name( &termscr->window , "Inscript Keyb") ;
				ml_iscii_select_keyb( termscr->iscii_state , ISCIIKEYB_INSCRIPT) ;
			}
			else
			{
				ml_set_window_name( &termscr->window , "mlterm") ;
				ml_iscii_select_keyb( termscr->iscii_state , ISCIIKEYB_NONE) ;
			}

			return ;
		}
		else if( ksym == XK_F1 && keyb != ISCIIKEYB_NONE)
		{
			if( keyb == ISCIIKEYB_IITKEYB)
			{
				ml_set_window_name( &termscr->window , "Inscript Keyb") ;
				ml_iscii_select_keyb( termscr->iscii_state , ISCIIKEYB_INSCRIPT) ;
			}
			else if( keyb == ISCIIKEYB_INSCRIPT)
			{
				ml_set_window_name( &termscr->window , "Phonetic Keyb") ;
				ml_iscii_select_keyb( termscr->iscii_state , ISCIIKEYB_IITKEYB) ;
			}

			return ;
		}
	}
	
	if( ml_keymap_match( termscr->keymap , XIM_OPEN , ksym , event->state))
	{
		ml_xic_activate( &termscr->window , "" , "") ;
	}
	else if( ml_keymap_match( termscr->keymap , XIM_CLOSE , ksym , event->state))
	{
		ml_xic_deactivate( &termscr->window) ;
	}
	else if( ml_keymap_match( termscr->keymap , NEW_PTY , ksym , event->state))
	{
		if( HAS_SYSTEM_LISTENER(termscr,open_pty))
		{
			termscr->system_listener->open_pty( termscr->system_listener->self) ;
		}
	}
#ifdef  DEBUG
	else if( ml_keymap_match( termscr->keymap , EXIT_PROGRAM , ksym , event->state))
	{
		if( HAS_SYSTEM_LISTENER(termscr,exit))
		{
			termscr->system_listener->exit( termscr->system_listener->self , 1) ;
		}
	}
#endif
	else if( ml_is_backscroll_mode( &termscr->bs_image))
	{
		if( ksym == XK_Prior)
		{
			if( event->state == ShiftMask)
			{
				bs_half_page_downward( termscr) ;
			}
			else
			{
				bs_page_downward( termscr) ;
			}
		}
		else if( ksym == XK_Next)
		{
			if( event->state == ShiftMask)
			{
				bs_half_page_upward( termscr) ;
			}
			else
			{
				bs_page_upward( termscr) ;
			}
		}
		else if( ksym == XK_k || ksym == XK_Up)
		{
			bs_scroll_downward( termscr) ;
		}
		else if( ksym == XK_j || ksym == XK_Down)
		{
			bs_scroll_upward( termscr) ;
		}
		else if( ksym != XK_Shift_L && ksym != XK_Shift_R && ksym != XK_Control_L &&
			ksym != XK_Control_R && ksym != XK_Caps_Lock && ksym != XK_Shift_Lock &&
			ksym != XK_Meta_L && ksym != XK_Meta_R && ksym != XK_Alt_L &&
			ksym != XK_Alt_R && ksym != XK_Super_L && ksym != XK_Super_R &&
			ksym != XK_Hyper_L && ksym != XK_Hyper_R && ksym != XK_Escape)
		{
			/* any string except Modifiers(X11/keysymdefs.h) */
			
			ml_restore_selected_region_color( &termscr->sel) ;
			exit_backscroll_mode( termscr) ;

			redraw_image( termscr) ;

			if( size > 0)
			{
				write_to_pty( termscr , seq , size , parser) ;
			}
		}
	}
	else
	{
		ml_restore_selected_region_color( &termscr->sel) ;

		if( termscr->mod_meta_mask & event->state)
		{
			if( termscr->mod_meta_mode == MOD_META_OUTPUT_ESC)
			{
				write_to_pty( termscr , "\x1b" , 1 , NULL) ;
			}
			else if( termscr->mod_meta_mode == MOD_META_SET_MSB)
			{
				if( 0x20 <= *seq && *seq <= 0x7e)
				{
					(*seq) |= 0x80 ;
				}
			}
		}

		if( ml_keymap_match( termscr->keymap , PAGE_UP , ksym , event->state))
		{
			enter_backscroll_mode( termscr) ;
			bs_half_page_downward( termscr) ;
		}
		else if( ml_keymap_match( termscr->keymap , SCROLL_UP , ksym , event->state))
		{
			enter_backscroll_mode( termscr) ;
			bs_scroll_downward( termscr) ;
		}
		else if( ml_keymap_match( termscr->keymap , INSERT_SELECTION , ksym , event->state))
		{
			yank_event_received( termscr , CurrentTime) ;
		}
	#ifdef  __DEBUG
		else if( ksym == XK_F12)
		{
			/* this is for tests of ml_image_xxx functions */

			/* ml_image_xxx( termscr->image) ; */
			
			redraw_image( termscr) ;
		}
	#endif
		else
		{
			char *  buf ;

			if( ksym == XK_Delete && size == 1)
			{
				buf = ml_termcap_get_sequence( termscr->termcap , MLT_DELETE) ;
			}
			else if( ksym == XK_BackSpace && size == 1)
			{
				buf = ml_termcap_get_sequence( termscr->termcap , MLT_BACKSPACE) ;
			}
			else if( size > 0)
			{
				if( termscr->iscii_state)
				{
					size = ml_convert_ascii_to_iscii(
						termscr->iscii_state , seq , size , seq , size) ;
				}
				
				write_to_pty( termscr , seq , size , parser) ;

				return ;
			}
			/*
			 * following ksym is processed only if no sequence string is received(size == 0)
			 */
			else if( ksym == XK_Up)
			{
				if( termscr->is_app_cursor_keys)
				{
					buf = "\x1bOA" ;
				}
				else
				{
					buf = "\x1b[A" ;
				}
			}
			else if( ksym == XK_Down)
			{
				if( termscr->is_app_cursor_keys)
				{
					buf = "\x1bOB" ;
				}
				else
				{
					buf = "\x1b[B" ;
				}
			}
			else if( ksym == XK_Right)
			{
				if( termscr->is_app_cursor_keys)
				{
					buf = "\x1bOC" ;
				}
				else
				{
					buf = "\x1b[C" ;
				}
			}
			else if( ksym == XK_Left)
			{
				if( termscr->is_app_cursor_keys)
				{
					buf = "\x1bOD" ;
				}
				else
				{
					buf = "\x1b[D" ;
				}
			}
			else if( ksym == XK_Prior)
			{
				buf = "\x1b[5~" ;
			}
			else if( ksym == XK_Next)
			{
				buf = "\x1b[6~" ;
			}
			else if( ksym == XK_Insert)
			{
				buf = "\x1b[2~" ;
			}
			else if( ksym == XK_F1)
			{
				buf = "\x1b[11~" ;
			}
			else if( ksym == XK_F2)
			{
				buf = "\x1b[12~" ;
			}
			else if( ksym == XK_F3)
			{
				buf = "\x1b[13~" ;
			}
			else if( ksym == XK_F4)
			{
				buf = "\x1b[14~" ;
			}
			else if( ksym == XK_F5)
			{
				buf = "\x1b[15~" ;
			}
			else if( ksym == XK_F6)
			{
				buf = "\x1b[17~" ;
			}
			else if( ksym == XK_F7)
			{
				buf = "\x1b[18~" ;
			}
			else if( ksym == XK_F8)
			{
				buf = "\x1b[19~" ;
			}
			else if( ksym == XK_F9)
			{
				buf = "\x1b[20~" ;
			}
			else if( ksym == XK_F10)
			{
				buf = "\x1b[21~" ;
			}
			else if( ksym == XK_F11)
			{
				buf = "\x1b[23~" ;
			}
			else if( ksym == XK_F12)
			{
				buf = "\x1b[24~" ;
			}
			else if( ksym == XK_F13)
			{
				buf = "\x1b[25~" ;
			}
			else if( ksym == XK_F14)
			{
				buf = "\x1b[26~" ;
			}
			else if( ksym == XK_F15)
			{
				buf = "\x1b[28~" ;
			}
			else if( ksym == XK_F16)
			{
				buf = "\x1b[29~" ;
			}
			else if( ksym == XK_Help)
			{
				buf = "\x1b[28~" ;
			}
			else if( ksym == XK_Menu)
			{
				buf = "\x1b[29~" ;
			}
		#if  0
			else if( termscr->is_app_keypad && ksym == XK_KP_Home)
			{
				buf = "\x1bOw" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Up)
			{
				buf = "\x1bOx" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Down)
			{
				buf = "\x1bOw" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Right)
			{
				buf = "\x1bOv" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Left)
			{
				buf = "\x1bOt" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Prior)
			{
				buf = "\x1bOy" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Next)
			{
				buf = "\x1bOs" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_End)
			{
				buf = "\x1bOq" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Enter)
			{
				buf = "\x1bOM" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Begin)
			{
				buf = "\x1bOu" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Insert)
			{
				buf = "\x1bOp" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Begin)
			{
				buf = "\x1bOu" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Delete)
			{
				buf = "\x1bOn" ;
			}
		#endif
			else if( termscr->is_app_keypad && ksym == XK_KP_F1)
			{
				buf = "\x1bOP" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_F2)
			{
				buf = "\x1bOQ" ;
			}		
			else if( termscr->is_app_keypad && ksym == XK_KP_F3)
			{
				buf = "\x1bOR" ;
			}		
			else if( termscr->is_app_keypad && ksym == XK_KP_F4)
			{
				buf = "\x1bOS" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Multiply)
			{
				buf = "\x1bOj" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Add)
			{
				buf = "\x1bOk" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Separator)
			{
				buf = "\x1bOl" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Subtract)
			{
				buf = "\x1bOm" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Decimal)
			{
				buf = "\x1bOn" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_Divide)
			{
				buf = "\x1bOo" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_0)
			{
				buf = "\x1bOp" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_1)
			{
				buf = "\x1bOq" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_2)
			{
				buf = "\x1bOr" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_3)
			{
				buf = "\x1bOs" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_4)
			{
				buf = "\x1bOt" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_5)
			{
				buf = "\x1bOu" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_6)
			{
				buf = "\x1bOv" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_7)
			{
				buf = "\x1bOw" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_8)
			{
				buf = "\x1bOx" ;
			}
			else if( termscr->is_app_keypad && ksym == XK_KP_9)
			{
				buf = "\x1bOy" ;
			}
			else
			{
				return ;
			}

			write_to_pty( termscr , buf , strlen(buf) , NULL) ;
		}
	}
}

static void
utf8_selection_request_failed(
	ml_window_t *  win ,
	XSelectionEvent *  event
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;
	
	/* UTF8_STRING selection request failed. retrying with XCOMPOUND_TEXT */
	ml_window_xct_selection_request( &termscr->window , event->time) ;
}

static void
selection_cleared(
	ml_window_t *  win ,
	XSelectionClearEvent *  event
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;
	
	if( termscr->sel.is_owner)
	{
		ml_sel_clear( &termscr->sel) ;
	}

	ml_restore_selected_region_color( &termscr->sel) ;
}

static int
copy_paste_via_ucs(
	ml_term_screen_t *  termscr
	)
{
	ml_char_encoding_t  encoding ;

	encoding = (*termscr->encoding_listener->encoding)( termscr->encoding_listener->self) ;

	if( IS_UCS_SUBSET_ENCODING(encoding) && termscr->copy_paste_via_ucs)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static size_t
convert_selection_to_xct(
	ml_term_screen_t *  termscr ,
	u_char *  str ,
	size_t  len
	)
{
	size_t  filled_len ;
	
#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending internal str: ") ;
		for( i = 0 ; i < termscr->sel.sel_len ; i ++)
		{
			ml_char_dump( &termscr->sel.sel_str[i]) ;
		}
		fprintf( stderr , "\n -> converting to ->\n") ;
	}
#endif

	(*termscr->ml_str_parser->init)( termscr->ml_str_parser) ;
	ml_str_parser_set_str( termscr->ml_str_parser , termscr->sel.sel_str , termscr->sel.sel_len) ;
	
	(*termscr->xct_conv->init)( termscr->xct_conv) ;
	filled_len = (*termscr->xct_conv->convert)( termscr->xct_conv ,
		str , len , termscr->ml_str_parser) ;

#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending xct str: ") ;
		for( i = 0 ; i < filled_len ; i ++)
		{
			fprintf( stderr , "%.2x" , str[i]) ;
		}
		fprintf( stderr , "\n") ;
	}
#endif

	return  filled_len ;
}

static size_t
convert_selection_to_utf8(
	ml_term_screen_t *  termscr ,
	u_char *  str ,
	size_t  len
	)
{
	size_t  filled_len ;

#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending internal str: ") ;
		for( i = 0 ; i < termscr->sel.sel_len ; i ++)
		{
			ml_char_dump( &termscr->sel.sel_str[i]) ;
		}
		fprintf( stderr , "\n -> converting to ->\n") ;
	}
#endif

	(*termscr->ml_str_parser->init)( termscr->ml_str_parser) ;
	ml_str_parser_set_str( termscr->ml_str_parser , termscr->sel.sel_str , termscr->sel.sel_len) ;
	
	(*termscr->utf8_conv->init)( termscr->utf8_conv) ;
	filled_len = (*termscr->utf8_conv->convert)( termscr->utf8_conv ,
		str , len , termscr->ml_str_parser) ;
		
#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending utf8 str: ") ;
		for( i = 0 ; i < filled_len ; i ++)
		{
			fprintf( stderr , "%.2x" , str[i]) ;
		}
		fprintf( stderr , "\n") ;
	}
#endif

	return  filled_len ;
}

static void
xct_selection_requested(
	ml_window_t * win ,
	XSelectionRequestEvent *  event ,
	Atom  type
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	if( termscr->sel.sel_str == NULL || termscr->sel.sel_len == 0)
	{
		ml_window_send_selection( win , event , NULL , 0 , 0) ;
	}
	else
	{
		u_char *  xct_str ;
		size_t  xct_len ;
		size_t  filled_len ;

		xct_len = termscr->sel.sel_len * XCT_MAX_CHAR_SIZE ;

		if( ( xct_str = alloca( xct_len)) == NULL)
		{
			return ;
		}

		filled_len = convert_selection_to_xct( termscr , xct_str , xct_len) ;

		ml_window_send_selection( win , event , xct_str , filled_len , type) ;
	}
}

static void
utf8_selection_requested(
	ml_window_t * win ,
	XSelectionRequestEvent *  event ,
	Atom  type
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	if( termscr->sel.sel_str == NULL || termscr->sel.sel_len == 0)
	{		
		ml_window_send_selection( win , event , NULL , 0 , 0) ;
	}
	else
	{
		u_char *  utf8_str ;
		size_t  utf8_len ;
		size_t  filled_len ;
		
		utf8_len = termscr->sel.sel_len * UTF8_MAX_CHAR_SIZE ;

		if( ( utf8_str = alloca( utf8_len)) == NULL)
		{
			return ;
		}

		filled_len = convert_selection_to_utf8( termscr , utf8_str , utf8_len) ;

		ml_window_send_selection( win , event , utf8_str , filled_len , type) ;
	}
}

static void
xct_selection_notified(
	ml_window_t *  win ,
	u_char *  str ,
	size_t  len
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	if( copy_paste_via_ucs(termscr))
	{
		/* XCOMPOUND TEXT -> UCS -> PTY ENCODING */
		
		u_char  conv_buf[512] ;
		size_t  filled_len ;

		(*termscr->xct_parser->init)( termscr->xct_parser) ;
		(*termscr->xct_parser->set_str)( termscr->xct_parser , str , len) ;
		
		(*termscr->utf8_conv->init)( termscr->utf8_conv) ;

		while( ! termscr->xct_parser->is_eos)
		{
			if( ( filled_len = (*termscr->utf8_conv->convert)(
				termscr->utf8_conv , conv_buf , sizeof( conv_buf) ,
				termscr->xct_parser)) == 0)
			{
				break ;
			}

			write_to_pty( termscr , conv_buf , filled_len , termscr->utf8_parser) ;
		}
	}
	else
	{
		/* XCOMPOUND TEXT -> UCS -> PTY ENCODING */
		
		write_to_pty( termscr , str , len , termscr->xct_parser) ;
	}

	return ;
}

static void
utf8_selection_notified(
	ml_window_t *  win ,
	u_char *  str ,
	size_t  len
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	write_to_pty( termscr , str , len , termscr->utf8_parser) ;
}


static void
start_selection(
	ml_term_screen_t *  termscr ,
	int  col_r ,
	int  row_r
	)
{
	int  col_l ;
	int  row_l ;

	if( col_r == 0)
	{
		ml_image_line_t *  line ;
		
		if( ( line = ml_bs_get_image_line_in_all( &termscr->bs_image , row_r - 1))
			== NULL || ml_imgline_is_empty( line))
		{
			col_l = col_r ;
			row_l = row_r ;
		}
		else
		{
			col_l = line->num_of_filled_chars - 1 ;
			row_l = row_r - 1 ;
		}
	}
	else
	{
		col_l = col_r - 1 ;
		row_l = row_r ;
	}

	ml_start_selection( &termscr->sel , col_l , row_l , col_r , row_r) ;
}

static void
selecting_with_motion(
	ml_term_screen_t *  termscr ,
	int  x ,
	int  y ,
	Time  time
	)
{
	int  char_index ;
	int  bs_row ;
	u_int  x_rest ;
	ml_image_line_t *  line ;

	if( x < 0)
	{
		x = 0 ;
	}
	else if( x > termscr->window.width)
	{
		x = termscr->window.width ;
	}
	
	if( y < 0)
	{
		if( ml_get_num_of_logged_lines( &termscr->logs) > 0)
		{
			if( ! ml_is_backscroll_mode( &termscr->bs_image))
			{
				enter_backscroll_mode( termscr) ;
			}
			
			bs_scroll_downward( termscr) ;
		}
		
		y = 0 ;
	}
	else if( y > termscr->window.height - ml_line_height( termscr->font_man))
	{
		if( ml_is_backscroll_mode( &termscr->bs_image))
		{
			bs_scroll_upward( termscr) ;
		}
		
		y = termscr->window.height - ml_line_height( termscr->font_man) ;
	}

	bs_row = ml_convert_row_to_bs_row( &termscr->bs_image ,
		convert_y_to_row( termscr , NULL , y)) ;

	if( ( line = ml_bs_get_image_line_in_all( &termscr->bs_image , bs_row)) == NULL ||
		ml_imgline_is_empty( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " image line(%d) not found.\n" , bs_row) ;
	#endif

		return ;
	}
	
	char_index = convert_x_to_char_index( termscr , line , &x_rest , x) ;

	if( line->num_of_filled_chars - 1 == char_index && x_rest)
	{
		/*
		 * XXX hack (anyway it works)
		 * this points to an invalid char , but by its secondary effect ,
		 * if mouse is dragged in a char-less area , nothing is selected.
		 */
		char_index ++ ;
	}

	if( ! termscr->sel.is_selecting)
	{
		ml_restore_selected_region_color( &termscr->sel) ;
		
		if( ! termscr->sel.is_owner)
		{
			if( ml_window_set_selection_owner( &termscr->window , time) == 0)
			{
				return ;
			}
		}

		start_selection( termscr , char_index , bs_row) ;
	}
	else
	{
		if( ml_is_after_sel_right_base_pos( &termscr->sel , char_index , bs_row))
		{
			if( char_index > 0)
			{
				char_index -- ;
			}
		}
		else if( ml_is_before_sel_left_base_pos( &termscr->sel , char_index , bs_row))
		{
			if( char_index < line->num_of_filled_chars - 1)
			{
				char_index ++ ;
			}
		}
		
		if( ml_selected_region_is_changed( &termscr->sel , char_index , bs_row , 1))
		{
			ml_selecting( &termscr->sel , char_index , bs_row) ;
		}
	}
}

static void
button_motion(
	ml_window_t *  win ,
	XMotionEvent *  event
	)
{
	ml_term_screen_t *  termscr ;
	
	termscr = (ml_term_screen_t*) win ;

	if( termscr->is_mouse_pos_sending && ! (event->state & ShiftMask))
	{
		return ;
	}

	selecting_with_motion( termscr , event->x , event->y , event->time) ;
}

static void
button_press_continued(
	ml_window_t *  win ,
	XButtonEvent *  event
	)
{
	ml_term_screen_t *  termscr ;
	
	termscr = (ml_term_screen_t*) win ;
	
	if( termscr->sel.is_selecting &&
		(event->y < 0 || win->height - ml_line_height( termscr->font_man) < event->y))
	{
		selecting_with_motion( termscr , event->x , event->y , event->time) ;
	}
}

static void
selecting_word(
	ml_term_screen_t *  termscr ,
	int  x ,
	int  y ,
	Time  time
	)
{
	int  char_index ;
	int  row ;
	u_int  x_rest ;
	int  beg_row ;
	int  beg_char_index ;
	int  end_row ;
	int  end_char_index ;
	ml_image_line_t *  line ;

	row = ml_convert_row_to_bs_row( &termscr->bs_image , convert_y_to_row( termscr , NULL , y)) ;

	if( ( line = ml_bs_get_image_line_in_all( &termscr->bs_image , row)) == NULL ||
		ml_imgline_is_empty( line))
	{
		return ;
	}

	char_index = convert_x_to_char_index( termscr , line , &x_rest , x) ;

	if( line->num_of_filled_chars - 1 == char_index && x_rest)
	{
		/* over end of line */

		return ;
	}

	if( ml_bs_get_word_region( &termscr->bs_image , &beg_char_index , &beg_row , &end_char_index ,
		&end_row , char_index , row) == 0)
	{
		return ;
	}

	if( ! termscr->sel.is_selecting)
	{
		ml_restore_selected_region_color( &termscr->sel) ;
		
		if( ! termscr->sel.is_owner)
		{
			if( ml_window_set_selection_owner( &termscr->window , time) == 0)
			{
				return ;
			}
		}
		
		start_selection( termscr , beg_char_index , beg_row) ;
	}

	ml_selecting( &termscr->sel , end_char_index , end_row) ;
}

static void
selecting_line(
	ml_term_screen_t *  termscr ,
	int  y ,
	Time  time
	)
{
	int  row ;
	int  beg_row ;
	int  end_char_index ;
	int  end_row ;

	row = ml_convert_row_to_bs_row( &termscr->bs_image , convert_y_to_row( termscr , NULL , y)) ;

	if( ml_bs_get_line_region( &termscr->bs_image , &beg_row , &end_char_index , &end_row , row) == 0)
	{
		return ;
	}
	
	if( ! termscr->sel.is_selecting)
	{
		ml_restore_selected_region_color( &termscr->sel) ;
		
		if( ! termscr->sel.is_owner)
		{
			if( ml_window_set_selection_owner( &termscr->window , time) == 0)
			{
				return ;
			}
		}
		
		start_selection( termscr , 0 , beg_row) ;
	}

	ml_selecting( &termscr->sel , end_char_index , end_row) ;
}

static int
report_mouse_tracking(
	ml_term_screen_t *  termscr ,
	XButtonEvent *  event ,
	int  is_released
	)
{
	int  button ;
	int  key_state ;
	int  col ;
	int  row ;
	u_char  buf[7] ;

	if( is_released)
	{
		button = 3 ;
	}
	else
	{
		button = event->button - Button1 ;
	}

	/*
	 * Shift = 4
	 * Meta = 8
	 * Control = 16
	 */
	key_state = ((event->state & ShiftMask) ? 4 : 0) +
		((event->state & ControlMask) ? 16 : 0) ;
	
	row = event->y / ml_line_height( termscr->font_man) ;
	col = event->x / ml_col_width( termscr->font_man) ;

	strcpy( buf , "\x1b[M") ;

	buf[3] = 0x20 + button + key_state ;
	buf[4] = 0x20 + col + 1 ;
	buf[5] = 0x20 + row + 1 ;

	write_to_pty( termscr , buf , 6 , NULL) ;
	
	return  1 ;
}

static void
button_pressed(
	ml_window_t *  win ,
	XButtonEvent *  event ,
	int  click_num
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*)win ;
	
	ml_restore_selected_region_color( &termscr->sel) ;

	if( termscr->is_mouse_pos_sending && ! (event->state & ShiftMask))
	{
		report_mouse_tracking( termscr , event , 0) ;

		return ;
	}

	if( event->button == 2)
	{
		yank_event_received( termscr , event->time) ;
	}
	else if( click_num == 2 && event->button == 1)
	{
		/* double clicked */
		
		selecting_word( termscr , event->x , event->y , event->time) ;
	}
	else if( click_num == 3 && event->button == 1)
	{
		/* triple click */

		selecting_line( termscr , event->y , event->time) ;
	}
	else if( event->button == 3 && event->state == ControlMask)
	{
		config_menu( termscr , event->x , event->y) ;
	}
	else if ( event->button == 4) 
	{
		/* wheel mouse */
		
		enter_backscroll_mode(termscr) ;
		if( event->state == ShiftMask)
		{
			bs_scroll_downward(termscr) ;
		}
		else if( event->state == ControlMask)
		{
			bs_page_downward(termscr) ;
		} 
		else 
		{
			bs_half_page_downward(termscr) ;
		}
	}
	else if ( event->button == 5) 
	{
		/* wheel mouse */
		
		enter_backscroll_mode(termscr) ;
		if( event->state == ShiftMask)
		{
			bs_scroll_upward(termscr) ;
		}
		else if( event->state == ControlMask)
		{
			bs_page_upward(termscr) ;
		} 
		else 
		{
			bs_half_page_upward(termscr) ;
		}
	}
}

static void
button_released(
	ml_window_t *  win ,
	XButtonEvent *  event
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	if( termscr->is_mouse_pos_sending && ! (event->state & ShiftMask))
	{
		report_mouse_tracking( termscr , event , 1) ;

		return ;
	}
	
	if( termscr->sel.is_selecting)
	{
		ml_stop_selecting( &termscr->sel) ;
	}
}


/*
 * !! Notice !!
 * this is closely related to ml_{image|log}_copy_region()
 */
static void
reverse_or_restore_color(
	ml_term_screen_t *  termscr ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row ,
	int (*func)( ml_image_line_t * , int)
	)
{
	int  char_index ;
	int  row ;
	ml_image_line_t *  line ;
	u_int  size_except_end_space ;

	if( ( line = ml_bs_get_image_line_in_all( &termscr->bs_image , beg_row)) == NULL ||
		ml_imgline_is_empty( line))
	{
		return ;
	}

	size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

	row = beg_row ;
	if( beg_row == end_row)
	{
		for( char_index = beg_char_index ;
			char_index < K_MIN(end_char_index + 1,size_except_end_space) ; char_index ++)
		{
			(*func)( line , char_index) ;
		}
	}
	else if( beg_row < end_row)
	{
		for( char_index = beg_char_index ; char_index < size_except_end_space ; char_index ++)
		{
			(*func)( line , char_index) ;
		}

		for( row ++ ; row < end_row ; row ++)
		{
			if( ( line = ml_bs_get_image_line_in_all( &termscr->bs_image , row)) == NULL ||
				ml_imgline_is_empty( line))
			{
				goto  end ;
			}

			size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;
			
			for( char_index = 0 ; char_index < size_except_end_space ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
		
		if( ( line = ml_bs_get_image_line_in_all( &termscr->bs_image , row)) == NULL ||
			ml_imgline_is_empty( line))
		{
			goto  end ;
		}

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;
		
		for( char_index = 0 ;
			char_index < K_MIN(end_char_index + 1,size_except_end_space) ; char_index ++)
		{
			(*func)( line , char_index) ;
		}
	}

end:
	redraw_image( termscr) ;

	return  ;
}


/*
 * callbacks of ml_config_menu_event events.
 */

static void
font_size_changed(
	ml_term_screen_t *  termscr
	)
{
	ml_restore_selected_region_color( &termscr->sel) ;
	exit_backscroll_mode( termscr) ;

	if( HAS_SCROLL_LISTENER(termscr,line_height_changed))
	{
		(*termscr->screen_scroll_listener->line_height_changed)(
			termscr->screen_scroll_listener->self , ml_line_height( termscr->font_man)) ;
	}

	/* screen will redrawn in window_resized() */
	ml_window_resize( &termscr->window ,
		ml_col_width( termscr->font_man) * ml_image_get_cols( termscr->image) ,
		ml_line_height( termscr->font_man) * ml_image_get_rows( termscr->image) ,
		NOTIFY_TO_PARENT) ;

	ml_window_set_normal_hints( &termscr->window ,
		ml_col_width( termscr->font_man) , ml_line_height( termscr->font_man) ,
		ml_col_width( termscr->font_man) , ml_line_height( termscr->font_man)) ;
		
	ml_window_reset_font( &termscr->window) ;

	/*
	 * !! Notice !!
	 * ml_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( termscr->window.window_resized)
	{
		(*termscr->window.window_resized)( &termscr->window) ;
	}
}

static void
change_font_size(
	void *  p ,
	u_int  font_size
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	if( ! ml_change_font_size( termscr->font_man , font_size))
	{
		kik_msg_printf( "changing font size to %d failed.\n" , font_size) ;
	
		return  ;
	}

	font_size_changed( termscr) ;

	/* this is because font_man->font_set may have changed in ml_smaller_font() */
	ml_xic_font_set_changed( &termscr->window) ;

	/* redrawing all lines with new fonts. */
	ml_image_set_modified_all( termscr->image) ;
}

static void
change_char_encoding(
	void *  p ,
	ml_char_encoding_t  encoding
	)
{
	ml_term_screen_t *  termscr ;
	int  result ;
	
	termscr = p ;

	if( ( result = ml_font_manager_usascii_font_cs_changed( termscr->font_man ,
		ml_get_usascii_font_cs( encoding))) == -1)
	{
		/* failed */

		kik_msg_printf( "encoding couldn't be changed.\n") ;

		return ;
	}

	if( result)
	{
		font_size_changed( termscr) ;

		/*
		 * this is because font_man->font_set may have changed in
		 * ml_font_manager_usascii_font_cs_changed()
		 */
		ml_xic_font_set_changed( &termscr->window) ;
	}

	if( ! termscr->encoding_listener->encoding_changed( termscr->encoding_listener->self , encoding))
	{
		kik_msg_printf( "VT100 encoding and Terminal screen encoding are discrepant.\n") ;
	}
	
	ml_image_set_modified_all( termscr->image) ;
	
	update_encoding_proper_aux( termscr) ;
}

static void
change_iscii_lang(
	void *  p ,
	ml_iscii_lang_t  lang
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	termscr->iscii_lang = lang ;
	
	if( termscr->iscii_state)
	{
		if( termscr->logvis == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " termscr->logvis is NULL under ISCII encoding.\n") ;
		#endif

			return ;
		}

		(*termscr->logvis->logical)( termscr->logvis) ;
		select_iscii_lang( termscr) ;
		(*termscr->logvis->visual)( termscr->logvis) ;
		
		ml_image_set_modified_all( termscr->image) ;
	}
}

static void
change_tab_size(
	void *  p ,
	u_int  tab_size
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	ml_image_set_tab_size( termscr->image , tab_size) ;
}

static void
change_log_size(
	void *  p ,
	u_int  logsize
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	ml_restore_selected_region_color( &termscr->sel) ;
	exit_backscroll_mode( termscr) ;
	
	ml_change_log_size( &termscr->logs , logsize) ;

	if( HAS_SCROLL_LISTENER(termscr,log_size_changed))
	{
		(*termscr->screen_scroll_listener->log_size_changed)(
			termscr->screen_scroll_listener->self , logsize) ;
	}
}

static void
change_mod_meta_mode(
	void *  p ,
	ml_mod_meta_mode_t  mod_meta_mode
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	termscr->mod_meta_mode = mod_meta_mode ;
}

static void
change_bel_mode(
	void *  p ,
	ml_bel_mode_t  bel_mode
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	termscr->bel_mode = bel_mode ;
}

static void
change_char_combining_flag(
	void *  p ,
	int  is_combining_char
	)
{
	if( ! is_combining_char)
	{
		ml_unuse_char_combining() ;
	}
	else
	{
		ml_use_char_combining() ;
	}
}

static void
change_copy_paste_via_ucs_flag(
	void *  p ,
	int  flag
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	termscr->copy_paste_via_ucs = flag ;
}

static void
change_fg_color(
	void *  p ,
	ml_color_t  color
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	ml_window_set_fg_color( &termscr->window , color) ;
	ml_xic_fg_color_changed( &termscr->window) ;
}

static void
change_bg_color(
	void *  p ,
	ml_color_t  color
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	ml_window_unfade( &termscr->window) ;
	ml_window_set_bg_color( &termscr->window , color) ;
	ml_xic_bg_color_changed( &termscr->window) ;
}

static void
larger_font_size(
	void *  p
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	ml_larger_font( termscr->font_man) ;

	font_size_changed( termscr) ;

	/* this is because font_man->font_set may have changed in ml_larger_font() */
	ml_xic_font_set_changed( &termscr->window) ;

	/* redrawing all lines with new fonts. */
	ml_image_set_modified_all( termscr->image) ;
		
	return ;
}

static void
smaller_font_size(
	void *  p
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	ml_smaller_font( termscr->font_man) ;

	font_size_changed( termscr) ;
	
	/* this is because font_man->font_set may have changed in ml_smaller_font() */
	ml_xic_font_set_changed( &termscr->window) ;

	/* redrawing all lines with new fonts. */
	ml_image_set_modified_all( termscr->image) ;
	
	return ;
}

static void
change_transparent_flag(
	void *  p ,
	int  is_transparent
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( is_transparent)
	{
		ml_window_set_transparent( &termscr->window) ;
	}
	else
	{
		ml_window_unset_transparent( &termscr->window) ;
	}
	
	if( HAS_SCROLL_LISTENER(termscr,transparent_state_changed))
	{
		(*termscr->screen_scroll_listener->transparent_state_changed)(
			termscr->screen_scroll_listener->self , is_transparent) ;
	}
}

static void
change_font_present(
	void *  p ,
	ml_font_present_t  font_present
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	if( termscr->font_present == font_present)
	{
		return ;
	}

	if( ! ml_font_manager_change_font_present( termscr->font_man , font_present))
	{
		return ;
	}

	font_size_changed( termscr) ;
	
	/* redrawing all lines with new fonts. */
	ml_image_set_modified_all( termscr->image) ;
	
	termscr->font_present = font_present ;
}

static void
change_bidi_flag(
	void *  p ,
	int  use_bidi
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	termscr->use_bidi = use_bidi ;
	
	ml_image_set_modified_all( termscr->image) ;

	update_encoding_proper_aux( termscr) ;
}

static void
change_wall_picture(
	void *  p ,
	char *  file_path
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->pic_file_path)
	{
		free( termscr->pic_file_path) ;
	}

	termscr->pic_file_path = strdup( file_path) ;

	set_wall_picture( termscr) ;
}

static void
unset_wall_picture(
	void *  p
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->pic_file_path)
	{
		free( termscr->pic_file_path) ;
		termscr->pic_file_path = NULL ;
	}
	
	ml_window_unset_wall_picture( &termscr->window) ;
}

static void
change_fade_ratio(
	void *  p ,
	u_int  fade_ratio
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	termscr->fade_ratio = fade_ratio ;

	if( ! termscr->is_focused)
	{
		unhighlight_cursor( termscr) ;

		/* suppressing redrawing */
		termscr->window.window_exposed = NULL ;
		ml_window_unfade( &termscr->window) ;
		
		termscr->window.window_exposed = window_exposed ;
		ml_window_fade( &termscr->window , termscr->fade_ratio) ;
		
		highlight_cursor( termscr) ;
	}
}

static void
change_xim(
	void *  p ,
	char *  xim ,
	char *  locale
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	ml_xic_deactivate( &termscr->window) ;

	ml_xic_activate( &termscr->window , xim , locale) ;
}

static void
full_reset(
	void *  p
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	(*termscr->encoding_listener->init)( termscr->encoding_listener->self , 1) ;
}


/*
 * callbacks of ml_sel_event_listener_t events.
 */
 
static void
reverse_color(
	void *  p ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	reverse_or_restore_color( (ml_term_screen_t*)p , beg_char_index , beg_row ,
		end_char_index , end_row , ml_imgline_reverse_color) ;
}

static void
restore_color(
	void *  p ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	reverse_or_restore_color( (ml_term_screen_t*)p , beg_char_index , beg_row ,
		end_char_index , end_row , ml_imgline_restore_color) ;
}

static int
select_in_window(
	void *  p ,
	ml_char_t **  chars ,
	u_int *  len ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	ml_term_screen_t *  termscr ;
	u_int  size ;
	ml_char_t *  buf ;

	termscr = p ;

	if( ( size = ml_bs_get_region_size( &termscr->bs_image , beg_char_index , beg_row ,
		end_char_index , end_row)) == 0)
	{
		return  0 ;
	}

	if( ( buf = ml_str_alloca( size)) == NULL)
	{
		return  0 ;
	}

	*len = ml_bs_copy_region( &termscr->bs_image , buf , size , beg_char_index ,
		beg_row , end_char_index , end_row) ;

#ifdef  DEBUG
	if( size != *len)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" ml_bs_get_region_size() == %d and ml_bs_copy_region() == %d"
			" are not the same size !\n" ,
			size , *len) ;
	}
#endif

	if( (*chars = ml_str_new( size)) == NULL)
	{
		return  0 ;
	}

	ml_str_copy( *chars , buf , size) ;

	ml_str_final( buf , size) ;
	
	return  1 ;
}


/*
 * callbacks of ml_bs_event_listener_t events.
 */
 
static int
window_scroll_upward(
	void *  p ,
	u_int  size
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( ! ml_window_is_scrollable( &termscr->window))
	{
		return  0 ;
	}

	set_scroll_boundary( termscr , 0 , ml_image_get_rows( termscr->image) - 1) ;
	
	termscr->scroll_cache_rows += size ;

	return  1 ;
}

static int
window_scroll_downward(
	void *  p ,
	u_int  size
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	if( ! ml_window_is_scrollable( &termscr->window))
	{
		return  0 ;
	}

	set_scroll_boundary( termscr , 0 , ml_image_get_rows( termscr->image) - 1) ;
	
	termscr->scroll_cache_rows -= size ;

	return  1 ;
}


/*
 * callbacks of ml_image_scroll events
 */
 
static int
window_scroll_upward_region(
	void *  p ,
	int  beg_row ,
	int  end_row ,
	u_int  size
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	if( ! ml_window_is_scrollable( &termscr->window))
	{
		return  0 ;
	}
	
	ml_restore_selected_region_color( &termscr->sel) ;

	set_scroll_boundary( termscr , beg_row , end_row) ;
	
	termscr->scroll_cache_rows += size ;

	return  1 ;
}

static int
window_scroll_downward_region(
	void *  p ,
	int  beg_row ,
	int  end_row ,
	u_int  size
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( ! ml_window_is_scrollable( &termscr->window))
	{
		return  0 ;
	}
	
	ml_restore_selected_region_color( &termscr->sel) ;

	set_scroll_boundary( termscr , beg_row , end_row) ;
	
	termscr->scroll_cache_rows -= size ;

	return  1 ;
}

/*
 * ml_sb_term_screen will override this function.
 */
static void
receive_scrolled_out_line(
	void *  p ,
	ml_image_line_t *  line
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->logvis)
	{
		(*termscr->logvis->visual_line)( termscr->logvis , line) ;
	}
	
	ml_log_add( &termscr->logs , line) ;
}


/*
 * callbacks of ml_xim events.
 */
 
/*
 * this doesn't consider backscroll mode.
 */
static int
get_spot(
	void *  p ,
	int *  x ,
	int *  y
	)
{
	ml_term_screen_t *  termscr ;
	ml_image_line_t *  line ;

	termscr = p ;
	
	if( ( line = ml_image_get_line( termscr->image , ml_cursor_row( termscr->image))) == NULL ||
		ml_imgline_is_empty( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " cursor line doesn't exist ?.\n") ;
	#endif
	
		return  0 ;
	}
	
	*y = convert_row_to_y( termscr , ml_cursor_row( termscr->image)) +
		ml_line_height( termscr->font_man) ;
	
	*x = convert_char_index_to_x( termscr , line , ml_cursor_char_index( termscr->image)) ;

	return  1 ;
}

static XFontSet
get_fontset(
	void *  p
	)
{
	ml_term_screen_t *  termscr ;
	
	termscr = p ;

	return  ml_get_fontset( termscr->font_man) ;
}


/* --- global functions --- */

ml_term_screen_t *
ml_term_screen_new(
	u_int  cols ,
	u_int  rows ,
	ml_font_manager_t *  font_man ,
	ml_color_table_t  color_table ,
	u_int  fade_ratio ,
	ml_keymap_t *  keymap ,
	ml_termcap_t *  termcap ,
	u_int  num_of_log_lines ,
	u_int  tab_size ,
	int  use_xim ,
	int  xim_open_in_startup ,
	ml_mod_meta_mode_t  mod_meta_mode ,
	ml_bel_mode_t  bel_mode ,
	int  copy_paste_via_ucs ,
	char *  pic_file_path ,
	int  use_transbg ,
	ml_font_present_t  font_present ,
	int  use_bidi ,
	int  big5_buggy ,
	char *  conf_menu_path ,
	ml_iscii_lang_t  iscii_lang
	)
{
	ml_term_screen_t *  termscr ;
	u_int  width ;
	u_int  height ;
	ml_char_t  sp_ch ;
	ml_char_t  nl_ch ;

	if( ( termscr = malloc( sizeof( ml_term_screen_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
	
		return  NULL ;
	}

	/* allocated dynamically */
	termscr->utf8_parser = NULL ;
	termscr->xct_parser = NULL ;
	termscr->ml_str_parser = NULL ;
	termscr->utf8_conv = NULL ;
	termscr->xct_conv = NULL ;
	
	termscr->pty = NULL ;
	
	termscr->font_man = font_man ;

	if( font_present)
	{
		if( ! ml_font_manager_change_font_present( termscr->font_man , font_present))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_font_manager_change_font_present failed.\n") ;
		#endif
		
			font_present = 0 ;
		}
	}
	
	termscr->font_present = font_present ;

	width = cols * ml_col_width( font_man) ;
	height = rows * ml_line_height( font_man) ;

	termscr->fade_ratio = fade_ratio ;
	termscr->is_focused = 0 ;

	if( ml_window_init( &termscr->window , color_table , width , height ,
		ml_col_width( termscr->font_man) , ml_line_height( termscr->font_man) ,
		ml_col_width( termscr->font_man) , ml_line_height( termscr->font_man) , 2) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_window_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( use_xim)
	{
		termscr->xim_listener.self = termscr ;
		termscr->xim_listener.get_spot = get_spot ;
		termscr->xim_listener.get_fontset = get_fontset ;

		if( ml_use_xim( &termscr->window , &termscr->xim_listener) == 0)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_use_xim() failed.\n") ;
		#endif
		}

		termscr->xim_open_in_startup = xim_open_in_startup ;
	}

	ml_window_set_cursor( &termscr->window , XC_xterm) ;

	/*
	 * event call backs.
	 */

	ml_window_init_event_mask( &termscr->window ,
		ButtonPressMask | ButtonMotionMask | ButtonReleaseMask | KeyPressMask) ;

	termscr->window.window_realized = window_realized ;
	termscr->window.window_finalized = window_finalized ;
	termscr->window.window_exposed = window_exposed ;
	termscr->window.window_focused = window_focused ;
	termscr->window.window_unfocused = window_unfocused ;
	termscr->window.key_pressed = key_pressed ;
	termscr->window.window_resized = window_resized ;
	termscr->window.button_motion = button_motion ;
	termscr->window.button_released = button_released ;
	termscr->window.button_pressed = button_pressed ;
	termscr->window.button_press_continued = button_press_continued ;
	termscr->window.selection_cleared = selection_cleared ;
	termscr->window.xct_selection_requested = xct_selection_requested ;
	termscr->window.utf8_selection_requested = utf8_selection_requested ;
	termscr->window.xct_selection_notified = xct_selection_notified ;
	termscr->window.utf8_selection_notified = utf8_selection_notified ;
	termscr->window.xct_selection_request_failed = NULL ;
	termscr->window.utf8_selection_request_failed = utf8_selection_request_failed ;
	termscr->window.window_deleted = window_deleted ;

	if( use_transbg)
	{
		ml_window_set_transparent( &termscr->window) ;
	}

	if( pic_file_path)
	{
		termscr->pic_file_path = strdup( pic_file_path) ;
	}
	else
	{
		termscr->pic_file_path = NULL ;
	}

	ml_char_init( &sp_ch) ;
	ml_char_init( &nl_ch) ;
	
	ml_char_set( &sp_ch , " " , 1 , ml_get_usascii_font( termscr->font_man) ,
		0 , MLC_FG_COLOR , MLC_BG_COLOR) ;
	ml_char_set( &nl_ch , "\n" , 1 , ml_get_usascii_font( termscr->font_man) ,
		0 , MLC_FG_COLOR , MLC_BG_COLOR) ;

	termscr->image_scroll_listener.self = termscr ;
	/* this may be overwritten */
	termscr->image_scroll_listener.receive_upward_scrolled_out_line = receive_scrolled_out_line ;
	termscr->image_scroll_listener.receive_downward_scrolled_out_line = NULL ;
	termscr->image_scroll_listener.window_scroll_upward_region = window_scroll_upward_region ;
	termscr->image_scroll_listener.window_scroll_downward_region = window_scroll_downward_region ;
	
	if( ! ml_image_init( &termscr->normal_image , &termscr->image_scroll_listener ,
		cols , rows , sp_ch , tab_size , 1))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_image_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ! ml_image_init( &termscr->alt_image , &termscr->image_scroll_listener ,
		cols , rows , sp_ch , tab_size , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_image_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	termscr->image = &termscr->normal_image ;

	termscr->use_bidi = use_bidi ;

	termscr->sel_listener.self = termscr ;
	termscr->sel_listener.select_in_window = select_in_window ;
	termscr->sel_listener.reverse_color = reverse_color ;
	termscr->sel_listener.restore_color = restore_color ;

	if( ! ml_sel_init( &termscr->sel , &termscr->sel_listener))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_sel_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ! ml_log_init( &termscr->logs , cols , num_of_log_lines))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_log_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	termscr->bs_listener.self = termscr ;
	termscr->bs_listener.window_scroll_upward = window_scroll_upward ;
	termscr->bs_listener.window_scroll_downward = window_scroll_downward ;

	if( ! ml_bs_init( &termscr->bs_image , termscr->image , &termscr->logs ,
		&termscr->bs_listener , &nl_ch))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_bs_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	ml_char_final( &sp_ch) ;
	ml_char_final( &nl_ch) ;

	termscr->config_menu_listener.self = termscr ;
	termscr->config_menu_listener.change_char_encoding = change_char_encoding ;
	termscr->config_menu_listener.change_iscii_lang = change_iscii_lang ;
	termscr->config_menu_listener.change_fg_color = change_fg_color ;
	termscr->config_menu_listener.change_bg_color = change_bg_color ;
	termscr->config_menu_listener.change_tab_size = change_tab_size ;
	termscr->config_menu_listener.change_log_size = change_log_size ;
	termscr->config_menu_listener.change_font_size = change_font_size ;
	termscr->config_menu_listener.change_mod_meta_mode = change_mod_meta_mode ;
	termscr->config_menu_listener.change_bel_mode = change_bel_mode ;
	termscr->config_menu_listener.change_char_combining_flag = change_char_combining_flag ;
	termscr->config_menu_listener.change_copy_paste_via_ucs_flag = change_copy_paste_via_ucs_flag ;
	termscr->config_menu_listener.change_transparent_flag = change_transparent_flag ;
	termscr->config_menu_listener.change_fade_ratio = change_fade_ratio ;
	termscr->config_menu_listener.change_font_present = change_font_present ;
	termscr->config_menu_listener.change_bidi_flag = change_bidi_flag ;
	termscr->config_menu_listener.change_xim = change_xim ;
	termscr->config_menu_listener.larger_font_size = larger_font_size ;
	termscr->config_menu_listener.smaller_font_size = smaller_font_size ;
	termscr->config_menu_listener.change_wall_picture = change_wall_picture ;
	termscr->config_menu_listener.unset_wall_picture = unset_wall_picture ;
	termscr->config_menu_listener.full_reset = full_reset ;

	if( ! ml_config_menu_init( &termscr->config_menu , conf_menu_path ,
		&termscr->config_menu_listener))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_config_menu_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	termscr->keymap = keymap ;
	termscr->termcap = termcap ;
	
	termscr->mod_meta_mask = 0 ;
	termscr->mod_meta_mode = mod_meta_mode ;

	termscr->bel_mode = bel_mode ;

	termscr->logvis = NULL ;
	termscr->iscii_lang = iscii_lang ;
	termscr->iscii_state = NULL ;
	termscr->shape = NULL ;

	/*
	 * for receiving selection.
	 */

	if( ( termscr->utf8_parser = mkf_utf8_parser_new()) == NULL)
	{
		goto  error ;
	}
	
	if( ( termscr->xct_parser = mkf_xct_parser_new()) == NULL)
	{
		goto  error ;
	}

	/*
	 * for sending selection
	 */
	 
	if( ( termscr->ml_str_parser = ml_str_parser_new()) == NULL)
	{
		goto  error ;
	}

	if( ( termscr->utf8_conv = mkf_utf8_conv_new()) == NULL)
	{
		goto  error ;
	}

	if( big5_buggy)
	{
		if( ( termscr->xct_conv = mkf_xct_big5_buggy_conv_new()) == NULL)
		{
			goto  error ;
		}
	}
	else if( ( termscr->xct_conv = mkf_xct_conv_new()) == NULL)
	{
		goto  error ;
	}

	termscr->copy_paste_via_ucs = copy_paste_via_ucs ;

	termscr->encoding_listener = NULL ;
	termscr->system_listener = NULL ;
	termscr->screen_scroll_listener = NULL ;

	termscr->scroll_cache_rows = 0 ;
	termscr->scroll_cache_boundary_start = 0 ;
	termscr->scroll_cache_boundary_end = 0 ;

	termscr->is_reverse = 0 ;
	termscr->is_cursor_visible = 1 ;
	termscr->is_app_keypad = 0 ;
	termscr->is_app_cursor_keys = 0 ;
	termscr->is_mouse_pos_sending = 0 ;

	return  termscr ;

error:
	if( termscr->utf8_parser)
	{
		(*termscr->utf8_parser->delete)( termscr->utf8_parser) ;
	}
	
	if( termscr->xct_parser)
	{
		(*termscr->xct_parser->delete)( termscr->xct_parser) ;
	}
	
	if( termscr->ml_str_parser)
	{
		(*termscr->ml_str_parser->delete)( termscr->ml_str_parser) ;
	}
	
	if( termscr->utf8_conv)
	{
		(*termscr->utf8_conv->delete)( termscr->utf8_conv) ;
	}
	
	if( termscr->xct_conv)
	{
		(*termscr->xct_conv->delete)( termscr->xct_conv) ;
	}
	
	if( termscr)
	{
		free( termscr) ;
	}

	return  NULL ;
}

int
ml_term_screen_delete(
	ml_term_screen_t *  termscr
	)
{
	/*
	 * this should be done before ml_image_final() since termscr->logvis refers
	 * to ml_image_t and may have some data structure for it.
	 */
	if( termscr->logvis)
	{
		(*termscr->logvis->delete)( termscr->logvis) ;
	}
	
	ml_image_final( &termscr->normal_image) ;
	ml_image_final( &termscr->alt_image) ;

	ml_log_final( &termscr->logs) ;
	ml_sel_final( &termscr->sel) ;
	ml_bs_final( &termscr->bs_image) ;
	ml_config_menu_final( &termscr->config_menu) ;

	if( termscr->shape)
	{
		(*termscr->shape->delete)( termscr->shape) ;
	}

	if( termscr->iscii_state)
	{
		ml_iscii_delete( termscr->iscii_state) ;
	}

	if( termscr->pic_file_path)
	{
		free( termscr->pic_file_path) ;
	}
	
	if( termscr->utf8_parser)
	{
		(*termscr->utf8_parser->delete)( termscr->utf8_parser) ;
	}
	
	if( termscr->xct_parser)
	{
		(*termscr->xct_parser->delete)( termscr->xct_parser) ;
	}
	
	if( termscr->ml_str_parser)
	{
		(*termscr->ml_str_parser->delete)( termscr->ml_str_parser) ;
	}
	
	if( termscr->utf8_conv)
	{
		(*termscr->utf8_conv->delete)( termscr->utf8_conv) ;
	}
	
	if( termscr->xct_conv)
	{
		(*termscr->xct_conv->delete)( termscr->xct_conv) ;
	}
	
	free( termscr) ;

	return  1 ;
}

int
ml_term_screen_set_pty(
	ml_term_screen_t *  termscr ,
	ml_pty_t *  pty
	)
{
	if( termscr->pty)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " pty is already set.\n") ;
	#endif
	
		return  0 ;
	}

	termscr->pty = pty ;

	return  1 ;
}

int
ml_set_system_listener(
	ml_term_screen_t *  termscr ,
	ml_system_event_listener_t *  system_listener
	)
{
	if( termscr->system_listener)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " system listener is already set.\n") ;
	#endif
	
		return  0 ;
	}

	termscr->system_listener = system_listener ;

	return  1 ;
}

int
ml_set_encoding_listener(
	ml_term_screen_t *  termscr ,
	ml_pty_encoding_event_listener_t *  encoding_listener
	)
{
	if( termscr->encoding_listener)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " encoding listener is already set.\n") ;
	#endif

		return  0 ;
	}

	/*
	 * all members must be set.
	 */
	if( ! encoding_listener->encoding_changed ||
		! encoding_listener->encoding ||
		! encoding_listener->convert ||
		! encoding_listener->init)
	{
		return  0 ;
	}

	termscr->encoding_listener = encoding_listener ;
	
	update_encoding_proper_aux( termscr) ;
	
	return  1 ;
}

int
ml_set_screen_scroll_listener(
	ml_term_screen_t *  termscr ,
	ml_screen_scroll_event_listener_t *  screen_scroll_listener
	)
{
	if( termscr->screen_scroll_listener)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " screen scroll listener is already set.\n") ;
	#endif
	
		return  0 ;
	}

	termscr->screen_scroll_listener = screen_scroll_listener ;

	return  1 ;
}


/*
 * for scrollbar scroll.
 */
 
int
ml_term_screen_scroll_upward(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	ml_restore_selected_region_color( &termscr->sel) ;

	if( ! ml_is_backscroll_mode( &termscr->bs_image))
	{
		enter_backscroll_mode( termscr) ;
	}

	ml_bs_scroll_upward( &termscr->bs_image , size) ;
	redraw_image( termscr) ;

	return  1 ;
}

int
ml_term_screen_scroll_downward(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	ml_restore_selected_region_color( &termscr->sel) ;

	if( ! ml_is_backscroll_mode( &termscr->bs_image))
	{
		enter_backscroll_mode( termscr) ;
	}

	ml_bs_scroll_downward( &termscr->bs_image , size) ;
	redraw_image( termscr) ;

	return  1 ;
}

int
ml_term_screen_scroll_to(
	ml_term_screen_t *  termscr ,
	int  row
	)
{
	ml_restore_selected_region_color( &termscr->sel) ;

	if( ! ml_is_backscroll_mode( &termscr->bs_image))
	{
		enter_backscroll_mode( termscr) ;
	}
	
	ml_bs_scroll_to( &termscr->bs_image , row) ;
	redraw_image( termscr) ;

	return  1 ;
}


/*
 * for ml_term_manager
 */

u_int
ml_term_screen_get_cols(
	ml_term_screen_t *  termscr
	)
{
	return  ml_image_get_cols( termscr->image) ;
}

u_int
ml_term_screen_get_rows(
	ml_term_screen_t *  termscr
	)
{
	return  ml_image_get_rows( termscr->image) ;
}


/*
 * these start/stop functions ensure that VT100 commands should treat screen buffer
 * as logical order , not visual order.
 */
 
int
ml_term_screen_start_vt100_cmd(
	ml_term_screen_t *  termscr
	)
{
	ml_restore_selected_region_color( &termscr->sel) ;
	exit_backscroll_mode( termscr) ;

	unhighlight_cursor( termscr) ;

	if( termscr->logvis)
	{
		(*termscr->logvis->logical)( termscr->logvis) ;
	}

	return  1 ;
}

int
ml_term_screen_stop_vt100_cmd(
	ml_term_screen_t *  termscr
	)
{
	if( termscr->logvis)
	{
		(*termscr->logvis->render)( termscr->logvis) ;
		(*termscr->logvis->visual)( termscr->logvis) ;
	}
	
	redraw_image( termscr) ;
	highlight_cursor( termscr) ;
	
	return  1 ;
}

ml_font_t *
ml_term_screen_get_font(
	ml_term_screen_t *  termscr ,
	ml_font_attr_t  attr
	)
{
	ml_font_t *  font ;
	
	if( ( font = ml_get_font( termscr->font_man , attr)) == NULL)
	{
		return  NULL ;
	}

#ifdef  DEBUG
	if( ml_line_height( termscr->font_man) < font->height)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" font(cs %x) height %d is larger than the basic line height %d.\n" ,
			ml_font_cs(font) , font->height , ml_line_height( termscr->font_man)) ;
	}
	else if( ml_line_height( termscr->font_man) > font->height)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" font(cs %x) height %d is smaller than the basic line height %d.\n" ,
			ml_font_cs(font) , font->height , ml_line_height( termscr->font_man)) ;
	}
#endif

	return  font ;
}
