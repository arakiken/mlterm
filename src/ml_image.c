/*
 *	$Id$
 */

#include  "ml_image.h"

#include  <stdio.h>		/* fprintf */
#include  <string.h>		/* memmove/memset */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */

#include  "ml_image_intern.h"
#include  "ml_image_scroll.h"
#include  "ml_bidi.h"


#if  0
#define  __DEBUG
#endif

#if  0
#define  CURSOR_DEBUG
#endif

#define  IS_BIDI_VISUAL(image) ((image)->lines[0].is_bidi)

#define  LINE_TAB_STOPS_SIZE(image) ((image)->num_of_cols / 8 + 1)
#define  DEFAULT_TAB_SIZE  8


/* --- static functions --- */

/*
 * return: actually broken rows.
 */
static u_int
break_row_boundary(
	ml_image_t *  image ,
	u_int  size
	)
{
	int  counter ;

	if( image->num_of_filled_rows + size > image->num_of_rows)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " it failed to break from line %d by size %d." ,
			END_ROW(image) , size) ;
	#endif

		size = image->num_of_rows - image->num_of_filled_rows ;

	#ifdef  DEBUG
		fprintf( stderr , " ... size is modified -> %d\n" , size) ;
	#endif
	}

	if( size == 0)
	{
		/* nothing is done */
		
		return  0 ;
	}

	for( counter = image->num_of_filled_rows ; counter < image->num_of_filled_rows + size ; counter ++)
	{
		ml_imgline_clear( &IMAGE_LINE(image,counter) , 0 , &image->sp_ch) ;
	}

	image->num_of_filled_rows += size ;

	/*
	 * change char index is not updated , because space has no glyph.
	 */
	 
	return  size ;
}

static void
reset_wraparound_checker(
	ml_image_t *  image
	)
{
	image->wraparound_ready_line = NULL ;
	ml_char_init( &image->prev_recv_ch) ;

	return ;
}

/*
 * if len is over IMAGE_LINE(image,end_row).num_of_filled_chars , *char_index points over it.
 */
static int
get_pos(
	ml_image_t *  image ,
	int *  row ,
	int *  char_index ,
	int  start_row ,
	int  start_char_index ,
	int  end_row ,
	u_int  len
	)
{
	*char_index = start_char_index + len ;

	for( *row = start_row ; *row < end_row ; (*row) ++)
	{
		if( *char_index < IMAGE_LINE(image,*row).num_of_filled_chars)
		{
			reset_wraparound_checker( image) ;
			
			return  1 ;
		}

		*char_index -= IMAGE_LINE(image,*row).num_of_filled_chars ;
	}

	if( IMAGE_LINE(image,end_row).num_of_filled_cols == image->num_of_cols &&
		*char_index > ml_imgline_end_char_index( &IMAGE_LINE(image,end_row)))
	{
		if( image->wraparound_ready_line == &IMAGE_LINE(image,end_row))
		{
			IMAGE_LINE(image,*row).is_continued_to_next = 1 ;
			
			if( *row == image->num_of_rows - 1)
			{
				ml_imgscrl_scroll_upward( image , 1) ;

				if( break_row_boundary( image , 1) != 1)
				{
				#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG
						" it failed to break_row_boundary.\n") ;
				#endif

					return  0 ;
				}
			}
			else
			{
				if( ++ (*row) > END_ROW(image))
				{
					if( break_row_boundary( image , 1) != 1)
					{
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" it failed to break_row_boundary.\n") ;
					#endif

						return  0 ;
					}
				}
			}

		#ifdef  __DEBUG
			if( ml_char_font( &image->prev_recv_ch) == NULL)
			{
				kik_warn_printf( KIK_DEBUG_TAG " image->prev_recv_ch is NULL.\n") ;
			}
		#endif

			ml_imgline_overwrite_chars( &IMAGE_LINE(image,*row) , 0 ,
				&image->prev_recv_ch , 1 , ml_char_cols( &image->prev_recv_ch) ,
				&image->sp_ch) ;

			*char_index = 1 ;

			reset_wraparound_checker( image) ;
		}
		else
		{
			*char_index = ml_imgline_end_char_index( &IMAGE_LINE(image,end_row)) ;
			
			image->wraparound_ready_line = &IMAGE_LINE(image,end_row) ;
		}
	}
	else
	{
		reset_wraparound_checker( image) ;
	}

	return  1 ;
}

static int
render_chars(
	ml_image_t *  image ,
	int  start_row ,
	ml_char_t *  chars ,
	u_int  num_of_chars
	)
{
	int  bol ;		/* index of the beginning of line */
	u_int  row ;
	u_int  line_len ;
	u_int  line_cols ;
	int  counter ;
	u_int  scroll_size ;

	ml_line_hints_reset( &image->line_hints) ;
	counter = 0 ;
	bol = 0 ;
	row = start_row ;
	scroll_size = 0 ;
	line_cols = 0 ;	
	while( 1)
	{
		if( line_cols + ml_char_cols( &chars[counter]) > image->num_of_cols)
		{
			/* wraparound line */

			/* excluding this char */
			line_len = counter - bol ;
			
			if( line_len > image->num_of_cols)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG 
					" line len %d is over ml_image_t::num_of_cols %d" ,
					line_len , image->num_of_cols) ;
			#endif

				line_len = image->num_of_cols ;

			#ifdef  DEBUG
				fprintf( stderr , " ... modified -> %d\n" , line_len) ;
			#endif
			}
			
			ml_line_hints_add( &image->line_hints , bol , line_len , line_cols) ;

			if( row >= image->num_of_rows)
			{
				scroll_size ++ ;
			}

			row ++ ;
			
			bol = counter ;
			line_cols = 0 ;
		}
		
		line_cols += ml_char_cols( &chars[counter]) ;
		
		if( ++ counter >= num_of_chars)
		{
			if( counter > bol)
			{
				line_len = counter - bol ;
				
				if( line_len > image->num_of_cols)
				{
				#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG 
						" line len %d is over ml_image_t::num_of_chars %d" ,
						line_len , image->num_of_cols) ;
				#endif

					line_len = image->num_of_cols ;

				#ifdef  DEBUG
					fprintf( stderr , "... modified -> %d\n" , line_len) ;
				#endif
				}
				
				/* last line is not completely filled */

				ml_line_hints_add( &image->line_hints , bol , line_len , line_cols) ;

				if( row >= image->num_of_rows)
				{
					scroll_size ++ ;
				}
			}

			if( scroll_size)
			{
				ml_imgscrl_scroll_upward( image , scroll_size) ;
			}

			return  1 ;
		}
	}
}

static int
overwrite_lines(
	ml_image_t *  image ,
	ml_char_t *  chars ,
	u_int  num_of_chars
	)
{
	int  counter ;
	int  current_row ;
	int  beg_char_index ;
	u_int  num_of_lines ;
	
	if( ! render_chars( image , image->cursor.row , chars , num_of_chars))
	{
		return  0 ;
	}

	num_of_lines = ml_get_num_of_lines_by_hints( &image->line_hints) ;

#ifdef  DEBUG
	if( num_of_lines > image->num_of_rows)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" ml_get_num_of_lines_by_hints() returns too large value.\n") ;
		
		num_of_lines = image->num_of_rows ;
	}
#endif

	counter = 0 ;
	current_row = image->cursor.row ;

	/* all changes should happen after cursor */
	beg_char_index = image->cursor.char_index ;

	while( 1)
	{
		ml_image_line_t *  line ;
		int  beg_of_line ;
		u_int  len ;
		u_int  cols ;

		if( ! ml_line_hints_at( &image->line_hints , &beg_of_line , &len , &cols , counter))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_line_hints_at(%d) failed.\n" , counter) ;
		#endif
			
			continue ;
		}

		if( beg_of_line >= num_of_chars)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " beg_of_line %d is over num_of_chars %d.\n" ,
				beg_of_line , num_of_chars) ;
		#endif
			
			return  0 ;
		}

		line = &IMAGE_LINE(image,current_row) ;
		
		if( ++ counter == num_of_lines)
		{
			ml_imgline_overwrite_chars( line , beg_char_index ,
				&chars[beg_of_line] , len , cols , &image->sp_ch) ;
			line->is_continued_to_next = 0 ;

			break ;
		}
		else
		{
			ml_imgline_overwrite_all( line , beg_char_index ,
				&chars[beg_of_line] , len , cols) ;
			line->is_continued_to_next = 1 ;
			
			current_row ++ ;
		}

		beg_char_index = 0 ;
	}

	if( current_row >= image->num_of_filled_rows)
	{
		image->num_of_filled_rows = current_row + 1 ;
	}

	return  1 ;
}

static int
cursor_goto_intern(
	ml_image_t *  image ,
	int  col_or_indx ,
	int  row ,
	int  flag ,		/* BREAK_BOUNDARY */
	int  is_col
	)
{
	u_int  brk_size ;
	u_int  actual ;
	int  char_index ;
	int  cols_rest ;

	if( row > END_ROW(image))
	{
		if( ! ( flag & BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot goto row %d(total lines are %d)\n" ,
				row , image->num_of_filled_rows) ;
		#endif

			return  0 ;
		}

		brk_size = row - END_ROW(image) ;
		
		if( ( actual = break_row_boundary( image , brk_size)) < brk_size)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot goto row %d(total lines are %d)\n" ,
				row , image->num_of_filled_rows) ;
		#endif

			row -= (brk_size - actual) ;
		}
	}

	if( is_col)
	{
		char_index = ml_convert_col_to_char_index( &IMAGE_LINE(image,row) , &cols_rest ,
					col_or_indx , BREAK_BOUNDARY) ;
	}
	else
	{
		char_index = col_or_indx ;
		cols_rest = 0 ;
	}
	
	if( char_index > ml_imgline_end_char_index( &IMAGE_LINE(image,row)))
	{
		if( ! ( flag & BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf(
				KIK_DEBUG_TAG " cursor cannot goto char_index %d(line length is %d)\n" ,
				char_index , IMAGE_LINE(image,row).num_of_filled_chars) ;
		#endif
		
			return  0 ;
		}

		brk_size = char_index + cols_rest + 1 - IMAGE_LINE(image,row).num_of_filled_chars ;
		
		if( ( actual = ml_imgline_break_boundary( &IMAGE_LINE(image,row) , brk_size , &image->sp_ch))
			< brk_size)
		{
		#ifdef  DEBUG
			kik_warn_printf(
				KIK_DEBUG_TAG " cursor cannot goto char index %d(line length is %d)\n" ,
				char_index , IMAGE_LINE(image,row).num_of_filled_chars) ;
		#endif

			char_index -= (brk_size - actual) ;
		}
	}

	image->cursor.char_index = char_index ;
	image->cursor.row = row ;
	
	image->cursor.col =
		ml_convert_char_index_to_col( &CURSOR_LINE(image) , image->cursor.char_index , 0)
		+ cols_rest ;

	return  1 ;
}

static int
cursor_goto_by_char_index(
	ml_image_t *  image ,
	int  char_index ,
	int  row ,
	int  flag		/* BREAK_BOUNDARY */
	)
{
	return  cursor_goto_intern( image , char_index , row , flag , 0) ;
}

static int
cursor_goto_by_col(
	ml_image_t *  image ,
	int  col ,
	int  row ,
	int  flag		/* BREAK_BOUNDARY */
	)
{
	if( col >= image->num_of_cols)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " col(%d) is larger than num_of_cols(%d)" ,
			col , image->num_of_cols) ;
	#endif
	
		col = image->num_of_cols - 1 ;
		
	#ifdef  DEBUG
		fprintf( stderr , " ... col modified -> %d\n" , col) ;
	#endif
	}
	
	return  cursor_goto_intern( image , col , row , flag , 1) ;
}

/*
 * inserting chars within a line.
 */
static int
insert_chars(
	ml_image_t *  image ,
	ml_char_t *  ins_chars ,
	u_int  num_of_ins_chars ,
	int  do_move_cursor
	)
{
	int  cursor_index ;
	int  new_char_index ;
	int  new_row ;
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  filled_len ;
	u_int  filled_cols ;
	int  cols_rest ;
	int  cols_after ;
	int  counter ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	buf_len = image->num_of_cols ;

	if( ( buffer = ml_str_alloca( buf_len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	filled_len = 0 ;

	/*
	 * before cursor(excluding cursor)
	 */
	 
	if( image->cursor.char_index > 0)
	{
		ml_str_copy( &buffer[filled_len] , CURSOR_LINE(image).chars , image->cursor.char_index) ;
		filled_len += image->cursor.char_index ;
	}

	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	if( cols_rest)
	{
	#ifdef  DEBUG
		if( ml_char_cols( &CURSOR_CHAR(image)) <= cols_rest)
		{
			kik_warn_printf( KIK_DEBUG_TAG " illegal cols_rest.\n") ;
		}
	#endif
	
		/*
		 * padding spaces for former half of cursor.
		 */
		for( counter = 0 ; counter < cols_rest ; counter ++)
		{
			ml_char_copy( &buffer[filled_len ++] , &image->sp_ch) ;
		}
		
		cols_after = ml_char_cols( &CURSOR_CHAR(image)) - cols_rest ;
	}
	else
	{
		cols_after = 0 ;
	}

	if( ! do_move_cursor)
	{
		cursor_index = filled_len ;
	}

	/*
	 * chars are appended one by one below since the line may be full.
	 */
	 
	/*
	 * inserted chars
	 */
	
	filled_cols = ml_str_cols( buffer , filled_len) ;
	
	for( counter = 0 ; counter < num_of_ins_chars ; counter ++)
	{
		if( filled_cols + ml_char_cols( &ins_chars[counter]) > image->num_of_cols)
		{
			break ;
		}

		ml_char_copy( &buffer[filled_len ++] , &ins_chars[counter]) ;
		filled_cols += ml_char_cols( &ins_chars[counter]) ;
	}

	if( do_move_cursor)
	{
		cursor_index = filled_len ;
	}
	
	if( filled_cols < image->num_of_cols)
	{
		/*
		 * cursor char
		 */
		 
		if( cols_after)
		{
			/*
			 * padding spaces for latter half of cursor.
			 */
			for( counter = 0 ; counter < cols_after ; counter ++)
			{
				if( filled_cols + ml_char_cols( &image->sp_ch) > image->num_of_cols)
				{
					goto  line_full ;
				}

				ml_char_copy( &buffer[filled_len++] , &image->sp_ch) ;
				filled_cols += ml_char_cols( &image->sp_ch) ;
			}
		}
		else
		{
			if( filled_cols + ml_char_cols( &CURSOR_CHAR(image)) > image->num_of_cols)
			{
				goto  line_full ;
			}

			ml_char_copy( &buffer[filled_len++] , &CURSOR_CHAR(image)) ;
			filled_cols += ml_char_cols( &CURSOR_CHAR(image)) ;
		}

		/*
		 * after cursor(excluding cursor)
		 */
		 
		for( counter = image->cursor.char_index + 1 ;
			counter < CURSOR_LINE(image).num_of_filled_chars ;
			counter ++)
		{
			if( filled_cols + ml_char_cols( &CURSOR_LINE(image).chars[counter]) >
				image->num_of_cols)
			{
				break ;
			}

			ml_char_copy( &buffer[filled_len ++] , &CURSOR_LINE(image).chars[counter]) ;
			filled_cols += ml_char_cols( &CURSOR_LINE(image).chars[counter]) ;
		}
	}

line_full:
	
	/*
	 * overwriting.
	 */

	ml_imgline_overwrite_all( &CURSOR_LINE(image) , image->cursor.char_index , buffer ,
		filled_len , filled_cols) ;

	ml_str_final( buffer , buf_len) ;

	/* this is necessary before get_pos() since insert_chars() should affect only this line */
	reset_wraparound_checker( image) ;

	/* wraparound_ready_line member is set in this function */
	get_pos( image , &new_row , &new_char_index , image->cursor.row , 0 ,
		image->cursor.row , cursor_index) ;

	/* for wraparound line checking */
	if( image->wraparound_ready_line != NULL)
	{
		if( num_of_ins_chars == 1)
		{
			ml_char_copy( &image->prev_recv_ch , ins_chars) ;
		}
		else
		{
			reset_wraparound_checker( image) ;
		}
	}
	
	cursor_goto_by_char_index( image , new_char_index , new_row , BREAK_BOUNDARY) ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	return  1 ;
}


/* --- global functions --- */

int
ml_image_init(
	ml_image_t *  image ,
	ml_image_scroll_event_listener_t *  scroll_listener ,
	u_int  num_of_cols ,
	u_int  num_of_rows ,
	ml_char_t   sp_ch ,
	u_int  tab_size ,
	int  is_logging
	)
{
	int  counter ;

	if( num_of_cols == 0 || num_of_rows == 0)
	{
		return  0 ;
	}
	
	image->num_of_rows = num_of_rows ;
	image->num_of_cols = num_of_cols ;

	image->beg_line = 0 ;

	if( ( image->lines = malloc( sizeof( ml_image_line_t) * image->num_of_rows)) == NULL)
	{
		return  0 ;
	}

	for( counter = 0 ; counter < image->num_of_rows ; counter ++)
	{
		if( ! ml_imgline_init( &image->lines[counter] , image->num_of_cols))
		{
			return  0 ;
		}
	}

	image->cursor.row = 0 ;
	image->cursor.char_index = 0 ;
	image->cursor.col = 0 ;
	image->cursor.fg_color = MLC_BG_COLOR ;
	image->cursor.bg_color = MLC_FG_COLOR ;
	image->cursor.orig_fg = MLC_BG_COLOR ;
	image->cursor.orig_bg = MLC_FG_COLOR ;
	image->cursor.is_highlighted = 0 ;

	image->cursor_is_saved = 0 ;

	ml_char_init( &image->sp_ch) ;
	ml_char_copy( &image->sp_ch , &sp_ch) ;

	/* initializing the first line. */
	ml_imgline_clear( &CURSOR_LINE(image) , 0 , &image->sp_ch) ;
	image->num_of_filled_rows = 1 ;

	image->is_logging = is_logging ;

	reset_wraparound_checker( image) ;
	
	image->scroll_region_beg = 0 ;
	image->scroll_region_end = image->num_of_rows - 1 ;
	image->scroll_listener = scroll_listener ;

	if( ( image->tab_stops = malloc( image->num_of_rows * LINE_TAB_STOPS_SIZE(image))) == NULL)
	{
		return  0 ;
	}
	ml_image_set_tab_size( image , tab_size) ;

	ml_line_hints_init( &image->line_hints , image->num_of_rows) ;

	return  1 ;
}

int
ml_image_final(
	ml_image_t *  image
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < image->num_of_rows ; counter ++)
	{
		ml_imgline_final( &image->lines[counter]) ;
	}
	
	free( image->lines) ;

	ml_char_final( &image->sp_ch) ;
	ml_char_final( &image->prev_recv_ch) ;

	free( image->tab_stops) ;

	ml_line_hints_final( &image->line_hints) ;

	return  1 ;
}

int
ml_image_resize(
	ml_image_t *  image ,
	u_int  num_of_cols ,
	u_int  num_of_rows
	)
{
	int  is_using_bidi ;
	int  old_row ;
	int  new_row ;
	int  counter ;
	u_int  copy_rows ;
	u_int  copy_len ;
	ml_image_line_t *  lines_p ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " resizing to %d %d , char index %d row %d ->" ,
		num_of_cols , num_of_rows , image->cursor.char_index , image->cursor.row) ;
#endif

	if( num_of_cols == image->num_of_cols && num_of_rows == image->num_of_rows)
	{
		/* not resized */
		
		return  0 ;
	}

	is_using_bidi = ml_image_is_using_bidi(image) ;

	ml_image_stop_bidi( image) ;

	if( ( lines_p = malloc( sizeof( ml_image_line_t) * num_of_rows)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	copy_len = K_MIN(num_of_cols , image->num_of_cols) ;

	if( num_of_cols < image->num_of_cols)
	{
		if( image->cursor.char_index >= num_of_cols)
		{
			image->cursor.char_index = num_of_cols - 1 ;
		}
	}

	new_row = 0 ;
	if( num_of_rows >= image->num_of_filled_rows)
	{
		old_row = 0 ;
		copy_rows = image->num_of_filled_rows ;
	}
	else
	{
		old_row = image->num_of_filled_rows - num_of_rows ;
		copy_rows = num_of_rows ;

		if( image->cursor.row < old_row)
		{
			image->cursor.row = 0 ;
			image->cursor.char_index = 0 ;
			image->cursor.col = 0 ;
		}
		else
		{
			image->cursor.row -= old_row ;
		}
	}

	/*
	 * updating each line.
	 */

	for( new_row = 0 ; new_row < copy_rows ; new_row ++)
	{
		ml_imgline_init( &lines_p[new_row] , num_of_cols) ;

		ml_imgline_copy_line( &lines_p[new_row] , &IMAGE_LINE(image,old_row)) ;
		old_row ++ ;
		
		ml_imgline_set_modified_all( &lines_p[new_row]) ;
	}

	image->num_of_filled_rows = new_row ;

	for( ; new_row < num_of_rows ; new_row ++)
	{
		ml_imgline_init( &lines_p[new_row] , num_of_cols) ;

		ml_imgline_set_modified_all( &lines_p[new_row]) ;
	}

	/*
	 * freeing old data.
	 */
	 
	for( counter = 0 ; counter < image->num_of_rows ; counter ++)
	{
		ml_imgline_final( &IMAGE_LINE(image,counter)) ;
	}

	free( image->lines) ;
	
	image->lines = lines_p ;

	image->num_of_rows = num_of_rows ;
	image->num_of_cols = num_of_cols ;

	image->beg_line = 0 ;

	image->wraparound_ready_line = NULL ;
	
	image->scroll_region_beg = 0 ;
	image->scroll_region_end = image->num_of_rows - 1 ;

	free( image->tab_stops) ;
	if( ( image->tab_stops = malloc( image->num_of_rows * LINE_TAB_STOPS_SIZE(image))) == NULL)
	{
		return  0 ;
	}
	ml_image_set_tab_size( image , image->tab_size) ;
	
	ml_line_hints_final( &image->line_hints) ;
	ml_line_hints_init( &image->line_hints , image->num_of_rows) ;

	if( image->cursor.row > image->num_of_rows)
	{
		image->cursor.row = END_ROW(image) ;
		image->cursor.char_index = ml_imgline_end_char_index( &CURSOR_LINE(image)) ;
		image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
					image->cursor.char_index , 0) ;
	}
	else if( image->cursor.col > image->num_of_cols)
	{
		image->cursor.char_index = ml_imgline_end_char_index( &CURSOR_LINE(image)) ;
		image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
					image->cursor.char_index , 0) ;
	}

	if( is_using_bidi)
	{
		ml_image_use_bidi( image) ;

		/* bidi-rendering all lines. */
		ml_image_render_bidi( image) ;
	}

#ifdef  __DEBUG
	fprintf( stderr , "-> char index %d row %d\n" ,
		image->cursor.char_index , image->cursor.row) ;
#endif

	return  1 ;
}

int
ml_image_is_using_bidi(
	ml_image_t *  image
	)
{
	return  ml_imgline_is_using_bidi(&(image)->lines[0]) ;
}

int
ml_image_use_bidi(
	ml_image_t *  image
	)
{
	int  row ;
	
	if( ml_image_is_using_bidi(image))
	{
		return  0 ;
	}

	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		ml_imgline_use_bidi( &image->lines[row]) ;
	}

	return  1 ;
}

int
ml_image_unuse_bidi(
	ml_image_t *  image
	)
{
	int  row ;
	
	if( ! ml_image_is_using_bidi(image))
	{
		return  0 ;
	}

	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		ml_imgline_unuse_bidi( &image->lines[row]) ;
	}
	
	return  1 ;
}

int
ml_image_render_bidi(
	ml_image_t *  image
	)
{
	int  row ;

	if( ! ml_image_is_using_bidi(image))
	{
		return  0 ;
	}

	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		if( image->lines[row].is_modified)
		{
			ml_imgline_render_bidi( &image->lines[row]) ;
		}
	}

	return  1 ;
}

int
ml_image_start_bidi(
	ml_image_t *  image
	)
{
	int  row ;
	u_int  cols_rest ;

	if( ! ml_image_is_using_bidi(image))
	{
		return  0 ;
	}

	if( IS_BIDI_VISUAL(image))
	{
		/* already started. blocking multiple call */
		
		return  1 ;
	}
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;

	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		ml_imgline_start_bidi( &image->lines[row]) ;
	}

	image->cursor.char_index = ml_convert_char_index_normal_to_bidi(
					&CURSOR_LINE(image) , image->cursor.char_index) ;
	image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
					image->cursor.char_index , 0) + cols_rest ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	return  1 ;
}

int
ml_image_stop_bidi(
	ml_image_t *  image
	)
{
	int  row ;
	u_int  cols_rest ;

	if( ! ml_image_is_using_bidi(image))
	{
		return  0 ;
	}

	if( ! IS_BIDI_VISUAL(image))
	{
		/* already stopped. blocking multiple call */
		
		return  1 ;
	}
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	
	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		ml_imgline_stop_bidi( &image->lines[row]) ;
	}

	image->cursor.char_index = ml_convert_char_index_bidi_to_normal(
					&CURSOR_LINE(image) , image->cursor.char_index) ;
	image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
					image->cursor.char_index , 0) + cols_rest ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif
	
	return  1 ;
}

int
ml_image_insert_chars(
	ml_image_t *  image ,
	ml_char_t *  ins_chars ,
	u_int  num_of_ins_chars
	)
{
	return  insert_chars( image , ins_chars , num_of_ins_chars , 1) ;
}

int
ml_image_insert_blank_chars(
	ml_image_t *  image ,
	u_int  num_of_blank_chars
	)
{
	int  counter ;
	ml_char_t *  blank_chars ;

	if( ( blank_chars = ml_str_alloca( num_of_blank_chars)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif

		return  0 ;
	}

	for( counter = 0 ; counter < num_of_blank_chars ; counter ++)
	{
		ml_char_copy( &blank_chars[counter] , &image->sp_ch) ;
	}

	ml_str_final( blank_chars , num_of_blank_chars) ;

	/* the cursor will not moved. */
	return  insert_chars( image , blank_chars , num_of_blank_chars , 0) ;
}

int
ml_image_overwrite_chars(
	ml_image_t *  image ,
	ml_char_t *  ow_chars ,
	u_int  num_of_ow_chars
	)
{
	int  cursor_index ;
	int  new_char_index ;
	int  new_row ;
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  filled_len ;
	int  cols_rest ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	buf_len = num_of_ow_chars + image->num_of_cols ;

	if( ( buffer = ml_str_alloca( buf_len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	filled_len = 0 ;

	/* before cursor(excluding cursor) */
	if( image->cursor.char_index > 0)
	{
		ml_str_copy( &buffer[filled_len] , CURSOR_LINE(image).chars , image->cursor.char_index) ;
		filled_len += image->cursor.char_index ;
	}
	
	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	if( cols_rest)
	{
		int  counter ;

		/*
		 * padding spaces before cursor.
		 */
		for( counter = 0 ; counter < cols_rest ; counter ++)
		{
			ml_char_copy( &buffer[filled_len ++] , &image->sp_ch) ;
		}
	}
	
	/* appending overwriting chars */
	ml_str_copy( &buffer[filled_len] , ow_chars , num_of_ow_chars) ;
	filled_len += num_of_ow_chars ;
	
	cursor_index = filled_len ;

	/*
	 * overwriting
	 */

	/* overwriting lines with scrolling */
	overwrite_lines( image , buffer , filled_len) ;

	ml_str_final( buffer , buf_len) ;

	/* wraparound_ready_line member is set in this function */
	get_pos( image , &new_row , &new_char_index , image->cursor.row , 0 ,
		ml_get_num_of_lines_by_hints( &image->line_hints) + image->cursor.row - 1 , cursor_index) ;

	/* for wraparound line checking */
	if( image->wraparound_ready_line != NULL)
	{
		if( num_of_ow_chars == 1)
		{
			ml_char_copy( &image->prev_recv_ch , ow_chars) ;
		}
		else
		{
			reset_wraparound_checker( image) ;
		}
	}
	
	cursor_goto_by_char_index( image , new_char_index , new_row , BREAK_BOUNDARY) ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif
	
	return  1 ;
}

/*
 * deleting chars within a line.
 */
int
ml_image_delete_cols(
	ml_image_t *  image ,
	u_int  delete_cols
	)
{
	int  char_index ;
	int  cursor_char_index ;
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  filled_len ;
	u_int  cols ;
	int  cols_rest ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	reset_wraparound_checker( image) ;

	if( image->cursor.row == END_ROW(image) &&
		image->cursor.char_index == ml_imgline_end_char_index( &CURSOR_LINE(image)))
	{
		/* if you are in the end of image , nothing is deleted. */
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" deletion is requested in the end of image(%d,%d) , nothing is deleted.\n" ,
			image->cursor.char_index , image->cursor.row) ;
	#endif
		
		return  1 ;
	}
	
	/*
	 * collecting chars after cursor line.
	 */
	 
	buf_len = CURSOR_LINE(image).num_of_filled_chars ;

	if( ( buffer = ml_str_alloca( buf_len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	filled_len = 0 ;

	/* before cursor(excluding cursor) */
	if( image->cursor.char_index > 0)
	{
		ml_str_copy( &buffer[filled_len] , CURSOR_LINE(image).chars , image->cursor.char_index) ;
		filled_len += image->cursor.char_index ;
	}

	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	if( cols_rest)
	{
		int  cols_after ;
		int  counter ;

	#ifdef  DEBUG
		if( ml_char_cols( &CURSOR_CHAR(image)) <= cols_rest)
		{
			kik_warn_printf( KIK_DEBUG_TAG " illegal cols_rest.\n") ;
		}
	#endif
	
		cols_after = ml_char_cols( &CURSOR_CHAR(image)) - cols_rest ;
		
		/*
		 * padding spaces before cursor.
		 */
		for( counter = 0 ; counter < cols_rest ; counter ++)
		{
			ml_char_copy( &buffer[filled_len ++] , &image->sp_ch) ;
		}

		cursor_char_index = filled_len ;

		if( delete_cols >= cols_after)
		{
			delete_cols -= cols_after ;
		}
		else
		{
			delete_cols = 0 ;

			/*
			 * padding spaces after cursor.
			 */
			for( counter = 0 ; counter < cols_after - delete_cols ; counter ++)
			{
				ml_char_copy( &buffer[filled_len ++] , &image->sp_ch) ;
			}
		}
	}
	else
	{
		cursor_char_index = filled_len ;
	}

	/* after cursor(excluding cursor) + delete_cols */

	char_index = filled_len ;
	
	if( delete_cols)
	{
		cols = ml_char_cols( &CURSOR_LINE(image).chars[char_index++]) ;
		while( cols < delete_cols && char_index <= ml_imgline_end_char_index( &CURSOR_LINE(image)))
		{
			cols += ml_char_cols( &CURSOR_LINE(image).chars[char_index++]) ;
		}
	}

	ml_str_copy( &buffer[filled_len] , &CURSOR_LINE(image).chars[char_index] ,
		CURSOR_LINE(image).num_of_filled_chars - char_index) ;
	filled_len += (CURSOR_LINE(image).num_of_filled_chars - char_index) ;

	if( filled_len > 0)
	{
		/*
		 * overwriting.
		 */

		ml_imgline_overwrite_all( &CURSOR_LINE(image) , image->cursor.char_index ,
			buffer , filled_len , ml_str_cols( buffer , filled_len)) ;
	}
	else
	{
		ml_imgline_clear( &CURSOR_LINE(image) , 0 , &image->sp_ch) ;
	}

	ml_str_final( buffer , buf_len) ;

	cursor_goto_by_char_index( image , cursor_char_index , image->cursor.row , BREAK_BOUNDARY) ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif
	
	return  1 ;
}

int
ml_image_insert_new_line(
	ml_image_t *  image
	)
{
	return  ml_imgscrl_insert_new_line( image) ;
}

int
ml_image_delete_line(
	ml_image_t *  image
	)
{
	return  ml_imgscrl_delete_line( image) ;
}

int
ml_image_clear_line_to_right(
	ml_image_t *  image
	)
{
	int  cols_rest ;

	reset_wraparound_checker( image) ;

	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	if( cols_rest)
	{
		int  counter ;

		if( image->cursor.char_index + cols_rest >= CURSOR_LINE(image).num_of_filled_chars)
		{
			u_int  size ;
			u_int  actual_size ;

			size = image->cursor.char_index + cols_rest -
				CURSOR_LINE(image).num_of_filled_chars + 1 ;

			actual_size = ml_imgline_break_boundary( &CURSOR_LINE(image) , size ,
					&image->sp_ch) ;
			if( actual_size < size)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_imgline_break_boundary() failed.\n") ;
			#endif
			
				if( size - actual_size > cols_rest)
				{
					cols_rest = 0 ;
				}
				else
				{
					cols_rest -= (size - actual_size) ;
				}
			}
		}

		/*
		 * padding spaces.
		 *
		 * if cols_rest is over 1 , the next char of cursor is overwritten(cleared) by sp_ch ,
		 * but in any event it will be cleared in ml_image_clear_line() , so it doesn't do harm.
		 */
		for( counter = 0 ; counter < cols_rest ; counter ++)
		{
			ml_char_copy( &CURSOR_CHAR(image) , &image->sp_ch) ;
			image->cursor.char_index ++ ;
		}
	}
	
	return  ml_image_clear_line( image , image->cursor.row , image->cursor.char_index) ;
}

int
ml_image_clear_line_to_left(
	ml_image_t *  image
	)
{
	int  counter ;
	u_int  num_of_sp ;
	ml_char_t *  buf ;
	u_int  buf_size ;

	reset_wraparound_checker( image) ;

	/* XXX cursor char will be cleared , is this ok? */
	buf_size = CURSOR_LINE(image).num_of_filled_chars - (image->cursor.char_index + 1) ;
	if( buf_size > 0)
	{
		u_int  padding ;
		u_int  filled ;
		
		if( ml_char_cols( &CURSOR_CHAR(image)) > 1)
		{
			ml_convert_col_to_char_index( &CURSOR_LINE(image) , &padding ,
				image->cursor.col , 0) ;
			buf_size += padding ;
		}
		else
		{
			padding = 0 ;
		}

		if( ( buf = ml_str_new( buf_size)) == NULL)
		{
			return  0 ;
		}

		filled = 0 ;

		if( padding)
		{
			/*
			 * padding spaces.
			 */
			for( counter = 0 ; counter < padding ; counter ++)
			{
				ml_char_copy( &buf[filled++] , &image->sp_ch) ;
			}
		}

		/* excluding cursor char */
		ml_str_copy( &buf[filled] , &CURSOR_CHAR(image) + 1 , buf_size - padding) ;
	}

	num_of_sp = image->cursor.col / ml_char_cols( &image->sp_ch) ;
	image->cursor.col = 0 ;
	for( counter = 0 ; counter < num_of_sp ; counter ++)
	{
		ml_char_copy( &CURSOR_LINE(image).chars[counter] , &image->sp_ch) ;
		image->cursor.col += ml_char_cols( &image->sp_ch) ;
	}

	image->cursor.char_index = counter ;

	/* char on cursor */
	ml_char_copy( &CURSOR_LINE(image).chars[counter++] , &image->sp_ch) ;

	if( buf_size > 0)
	{
		ml_str_copy( &CURSOR_LINE(image).chars[counter] , buf , buf_size) ;
		
		CURSOR_LINE(image).num_of_filled_chars = counter + buf_size ;

		ml_str_delete( buf , buf_size) ;
	}

	ml_imgline_set_modified(
		&CURSOR_LINE(image) , 0 , image->cursor.char_index , 0) ;

	return  1 ;
}

int
ml_image_clear_below(
	ml_image_t *  image
	)
{
	reset_wraparound_checker( image) ;
	
	return  ml_image_clear_lines( image , image->cursor.row ,
			image->num_of_filled_rows - image->cursor.row) ;
}

int
ml_image_clear_above(
	ml_image_t *  image
	)
{
	reset_wraparound_checker( image) ;

	if( ! ml_image_clear_lines( image , 0 , image->cursor.row) ||
		! ml_image_clear_line_to_left( image))
	{
		return  0 ;
	}

	return  1 ;
}

int
ml_image_set_scroll_region(
	ml_image_t *  image ,
	int  beg ,
	int  end
	)
{
	image->scroll_region_beg = beg ;
	image->scroll_region_end = end ;

	return  1 ;
}

int
ml_image_scroll_upward(
	ml_image_t *  image ,
	u_int  size
	)
{
	int  cursor_row ;
	int  cursor_col ;
	
	cursor_row = image->cursor.row ;
	cursor_col = image->cursor.col ;
	
	ml_imgscrl_scroll_upward( image , size) ;
	cursor_goto_by_col( image , cursor_col , cursor_row , BREAK_BOUNDARY) ;

	return  1 ;
}

int
ml_image_scroll_downward(
	ml_image_t *  image ,
	u_int  size
	)
{
	int  cursor_row ;
	int  cursor_col ;
	
	cursor_row = image->cursor.row ;
	cursor_col = image->cursor.col ;

	ml_imgscrl_scroll_downward( image , size) ;
	cursor_goto_by_col( image , cursor_col , cursor_row , BREAK_BOUNDARY) ;

	return  1 ;
}

int
ml_image_vertical_tab(
	ml_image_t *  image
	)
{
	int  col ;
	u_char *  tab_stops ;

	reset_wraparound_checker( image) ;

	tab_stops = &image->tab_stops[ image->cursor.row * LINE_TAB_STOPS_SIZE(image)] ;

	for( col = image->cursor.col + 1 ; col < image->num_of_cols ; col ++)
	{
		if( tab_stops[col / 8] & ( 1 << (7 - col % 8)))
		{
			break ;
		}
	}

	cursor_goto_by_col( image , col , image->cursor.row , BREAK_BOUNDARY) ;
	
	return  1 ;
}

int
ml_image_set_tab_size(
	ml_image_t *  image ,
	u_int  tab_size
	)
{
	int  row ;
	int  col ;
	u_int8_t *  tab_stops ;

	if( tab_size == 0)
	{
		tab_size = DEFAULT_TAB_SIZE ;
	}

	ml_image_clear_all_tab_stops( image) ;

	tab_stops = image->tab_stops ;
	
	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		col = 0 ;
		
		while( 1)
		{
			if( col % tab_size == 0)
			{
				(*tab_stops) |= ( 1 << (7 - col % 8)) ;
			}

			col ++ ;
			
			if( col % 8 == 0)
			{
				tab_stops ++ ;

				if( col >= image->num_of_cols)
				{
					break ;
				}
			}
			else if( col >= image->num_of_cols)
			{
				tab_stops ++ ;

				break ;
			}
		}
	}

	image->tab_size = tab_size ;

	return  1 ;
}

int
ml_image_set_tab_stop(
	ml_image_t *  image
	)
{
	image->tab_stops[ image->cursor.row * LINE_TAB_STOPS_SIZE(image) + image->cursor.col / 8]
		|= (1 << (7 - image->cursor.col % 8)) ;
	
	return  1 ;
}

int
ml_image_clear_tab_stop(
	ml_image_t *  image
	)
{
	image->tab_stops[ image->cursor.row * LINE_TAB_STOPS_SIZE(image) + image->cursor.col / 8]
		&= ~(1 << (7 - image->cursor.col % 8)) ;
	
	return  1 ;
}

int
ml_image_clear_all_tab_stops(
	ml_image_t *  image
	)
{
	memset( image->tab_stops , 0 , image->num_of_rows * LINE_TAB_STOPS_SIZE(image)) ;
	
	return  1 ;
}

ml_image_line_t *
ml_image_get_line(
	ml_image_t *  image ,
	int  row
	)
{
	if( row < 0 || image->num_of_rows <= row)
	{
		return  NULL ;
	}
	else
	{
		return  &IMAGE_LINE(image,row) ;
	}
}

int
ml_cursor_is_beg_of_line(
	ml_image_t *  image
	)
{
	return  (image->cursor.row == 0) ;
}

int
ml_cursor_is_end_of_line(
	ml_image_t *  image
	)
{
	return  (image->cursor.char_index == ml_imgline_end_char_index( &CURSOR_LINE(image))) ;
}

int
ml_cursor_goto_beg_of_line(
	ml_image_t *  image
	)
{
	reset_wraparound_checker( image) ;
	
	image->cursor.char_index = 0 ;
	image->cursor.col = 0 ;

	return  1 ;
}

int
ml_cursor_goto_end_of_line(
	ml_image_t *  image
	)
{
	reset_wraparound_checker( image) ;
	
	image->cursor.char_index = ml_imgline_end_char_index( &CURSOR_LINE(image)) ;
	image->cursor.col =
		ml_convert_char_index_to_col( &CURSOR_LINE(image) , image->cursor.char_index , 0) ;

	return  1 ;
}

int
ml_cursor_goto_home(
	ml_image_t *  image
	)
{
	reset_wraparound_checker( image) ;
	
	image->cursor.char_index = 0 ;
	image->cursor.col = 0 ;
	image->cursor.row = 0 ;

	return  1 ;
}

int
ml_cursor_goto_end(
	ml_image_t *  image
	)
{
	reset_wraparound_checker( image) ;
	
	image->cursor.char_index = ml_imgline_end_char_index( &END_LINE(image)) ;
	image->cursor.row = END_ROW( image) ;
	image->cursor.col =
		ml_convert_char_index_to_col( &CURSOR_LINE(image) , image->cursor.char_index , 0) ;

	return  1 ;
}

int
ml_cursor_go_forward(
	ml_image_t *  image ,
	int  flag		/* WARPAROUND | SCROLL | BREAK_BOUNDARY */
	)
{
	int  cols_rest ;
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " going forward from char index %d col %d row %d, then ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	reset_wraparound_checker( image) ;

	/*
	 * full width char check.
	 */

	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest ,
		K_MIN(image->num_of_cols - 1,image->cursor.col + 1) , flag & BREAK_BOUNDARY) ;
	if( cols_rest)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " cursor is at 2nd byte of multi byte char.\n") ;
	#endif

		image->cursor.col ++ ;

		return  1 ;
	}

	/*
	 * moving forward.
	 */
	 
	if( image->cursor.char_index == ml_imgline_end_char_index( &CURSOR_LINE(image)))
	{
		if( flag & BREAK_BOUNDARY)
		{
			if( ml_imgline_break_boundary( &CURSOR_LINE(image) , 1 , &image->sp_ch))
			{
				image->cursor.char_index ++ ;

				goto  end ;
			}
		}
		
		if( ! ( flag & WRAPAROUND))
		{
			return  0 ;
		}

		if( ml_is_scroll_lowerlimit( image , image->cursor.row))
		{
			if( ! ( flag & SCROLL))
			{
				return  0 ;
			}

			if( ! ml_imgscrl_scroll_upward( image , 1))
			{
				return  0 ;
			}
		}
		
		if( image->cursor.row == END_ROW(image))
		{
			if( ! ( flag & BREAK_BOUNDARY))
			{
				return  0 ;
			}

			if( ! break_row_boundary( image , 1))
			{
				return  0 ;
			}
		}
		
		image->cursor.row ++ ;
		image->cursor.char_index = 0 ;
	}
	else
	{
		image->cursor.char_index ++ ;
	}

end:
	image->cursor.col =
		ml_convert_char_index_to_col( &CURSOR_LINE(image) , image->cursor.char_index , 0) ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> char index %d col %d row %d\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	return  1 ;
}

int
ml_cursor_go_back(
	ml_image_t *  image ,
	int  flag		/* WRAPAROUND | SCROLL */
	)
{
	int  cols_rest ;
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " going back from char index %d col %d row %d ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	reset_wraparound_checker( image) ;
	
	/*
	 * full width char check.
	 */
	 
	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	if( cols_rest)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " cursor is at 2nd byte of multi byte char.\n") ;
	#endif
	
		image->cursor.col -- ;

		return  1 ;
	}

	/*
	 * moving backward.
	 */
	 
	if( image->cursor.char_index == 0)
	{
		if( ! ( flag & WRAPAROUND))
		{
			return  0 ;
		}

		if( ml_is_scroll_upperlimit( image , image->cursor.row))
		{
			if( ! ( flag & SCROLL))
			{
				return  0 ;
			}

			if( ! ml_imgscrl_scroll_downward( image , 1))
			{
				return  0 ;
			}
		}
		
		image->cursor.char_index = ml_imgline_end_char_index( &CURSOR_LINE( image)) ;
	}
	else
	{
		image->cursor.char_index -- ;
	}

	image->cursor.col =
		ml_convert_char_index_to_col( &CURSOR_LINE(image) , image->cursor.char_index , 0) ;
	image->cursor.col += (ml_char_cols( &CURSOR_CHAR(image)) - 1) ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , " to char index %d col %d row %d\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	return  1 ;
}

int
ml_cursor_go_upward(
	ml_image_t *  image ,
	int  flag		/* SCROLL | BREAK_BOUNDARY */
	)
{
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " going upward from char index %d(col %d) ->" ,
		image->cursor.char_index , image->cursor.col) ;
#endif
	
	reset_wraparound_checker( image) ;
	
	if( ml_is_scroll_upperlimit( image , image->cursor.row))
	{
		if( ! ( flag & SCROLL) || ! ( flag & BREAK_BOUNDARY))
		{
			return  0 ;
		}

		if( ! ml_image_scroll_downward( image , 1))
		{
			return  0 ;
		}
	}
	else
	{
		if( ! cursor_goto_by_col( image , image->cursor.col , image->cursor.row - 1 ,
			flag & BREAK_BOUNDARY))
		{
			return  0 ;
		}
	}
	
#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> upward to char index %d (col %d)\n" ,
		image->cursor.char_index , image->cursor.col) ;
#endif
	
	return  1 ;
}

int
ml_cursor_go_downward(
	ml_image_t *  image ,
	int  flag		/* SCROLL | BREAK_BOUNDARY */
	)
{
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " going downward from char index %d col %d row %d ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif
	
	reset_wraparound_checker( image) ;
	
	if( ml_is_scroll_lowerlimit( image , image->cursor.row))
	{
		if( ! ( flag & (SCROLL)) || ! ( flag & BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf(
				KIK_DEBUG_TAG " cursor cannot go downward(reaches scroll lower limit).\n") ;
		#endif

			return  0 ;
		}

		if( ! ml_image_scroll_upward( image , 1))
		{
			return  0 ;
		}
	}
	else
	{
		if( ! cursor_goto_by_col( image , image->cursor.col , image->cursor.row + 1 ,
			flag & BREAK_BOUNDARY))
		{
			return  0 ;
		}
	}

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> downward to char_index %d col %d row %d\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif
	
	return  1 ;
}

int
ml_cursor_goto(
	ml_image_t *  image ,
	int  col ,
	int  row ,
	int  flag
	)
{
	reset_wraparound_checker( image) ;

	if( row < 0 || image->num_of_rows <= row)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " row %d is over num_of_rows %d" ,
			row , image->num_of_rows) ;
	#endif

		row = image->num_of_rows - 1 ;

	#ifdef  DEBUG
		fprintf( stderr , " ... modified to %d.\n" , row) ;
	#endif
	}
	
	if( col < 0 || image->num_of_cols <= col)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " col %d is over num_of_cols %d" ,
			col , image->num_of_cols) ;
	#endif

		col = image->num_of_cols - 1 ;

	#ifdef  DEBUG
		fprintf( stderr , " ... modified to %d.\n" , col) ;
	#endif
	}

	if( ! cursor_goto_by_col( image , col , row , flag))
	{
		return  0 ;
	}

	image->cursor.col = col ;

	return  1 ;
}

int
ml_cursor_set_color(
	ml_image_t *  image ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color
	)
{
	image->cursor.fg_color = fg_color ;
	image->cursor.bg_color = bg_color ;

	return  1 ;
}

int
ml_cursor_save(
	ml_image_t *  image
	)
{
	image->saved_cursor = image->cursor ;
	image->cursor_is_saved = 1 ;

	return  1 ;
}

int
ml_cursor_restore(
	ml_image_t *  image
	)
{
	if( ! image->cursor_is_saved)
	{
		return  0 ;
	}
	
	if( cursor_goto_by_char_index( image , image->saved_cursor.char_index ,
		image->saved_cursor.row , BREAK_BOUNDARY) == 0)
	{
		return  0 ;
	}
	
	image->cursor_is_saved = 0 ;

	return  1 ;
}

int
ml_highlight_cursor(
	ml_image_t *  image
	)
{
	if( image->cursor.is_highlighted)
	{
		/* already highlited */
		
		return  0 ;
	}

	image->cursor.orig_fg = ml_char_fg_color( &CURSOR_CHAR(image)) ;
	ml_char_set_fg_color( &CURSOR_CHAR(image) , image->cursor.fg_color) ;
	
	image->cursor.orig_bg = ml_char_bg_color( &CURSOR_CHAR(image)) ;
	ml_char_set_bg_color( &CURSOR_CHAR(image) , image->cursor.bg_color) ;

	image->cursor.is_highlighted = 1 ;

	return  1 ;
}

int
ml_unhighlight_cursor(
	ml_image_t *  image
	)
{
	if( ! image->cursor.is_highlighted)
	{
		/* already highlited */
		
		return  0 ;
	}
	
	ml_char_set_fg_color( &CURSOR_CHAR(image) , image->cursor.orig_fg) ;
	ml_char_set_bg_color( &CURSOR_CHAR(image) , image->cursor.orig_bg) ;

	image->cursor.is_highlighted = 0 ;
	
	return  1 ;
}

int
ml_cursor_char_index(
	ml_image_t *  image
	)
{
	return  image->cursor.char_index ;
}

int
ml_cursor_col(
	ml_image_t *  image
	)
{
	return  image->cursor.col ;
}

int
ml_cursor_row(
	ml_image_t *  image
	)
{
	return  image->cursor.row ;
}

void
ml_image_set_modified_all(
	ml_image_t *  image
	)
{
	int  counter ;

	for( counter = 0 ; counter < image->num_of_rows ; counter ++)
	{
		ml_imgline_set_modified_all( &IMAGE_LINE(image,counter)) ;
	}
}

inline u_int
ml_image_get_cols(
	ml_image_t *  image
	)
{
	return  image->num_of_cols ;
}

inline u_int
ml_image_get_rows(
	ml_image_t *  image
	)
{
	return  image->num_of_rows ;
}

int
ml_image_fill_all(
	ml_image_t *  image ,
	ml_char_t *  ch
	)
{
	int  row ;
	
	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		ml_imgline_fill_all( &IMAGE_LINE(image,row) , ch ,
			image->num_of_cols / ml_char_cols(ch)) ;
	}

	image->num_of_filled_rows = image->num_of_rows ;

	return  1 ;
}

/*
 * for debugging.
 */

#ifdef  DEBUG

void
ml_image_dump(
	ml_image_t *  image
	)
{
	int  row ;
	
	kik_debug_printf( KIK_DEBUG_TAG
		" ===> dumping image...[(filled rows)%d] [cursor(index)%d (col)%d (row)%d]\n" ,
		image->num_of_filled_rows , image->cursor.char_index , image->cursor.col ,
		image->cursor.row) ;

	for( row = 0 ; row < image->num_of_rows ; row++)
	{
		int  char_index ;

		fprintf( stderr , "[%.2d %.2d]" , IMAGE_LINE(image,row).num_of_filled_chars ,
			IMAGE_LINE(image,row).num_of_filled_cols) ;
			
		if( IMAGE_LINE(image,row).num_of_filled_chars > 0)
		{
			for( char_index = 0 ; char_index < IMAGE_LINE(image,row).num_of_filled_chars ;
				char_index ++)
			{
				if( image->cursor.row == row && image->cursor.char_index == char_index)
				{
					fprintf( stderr , "**") ;
				}

				ml_char_dump( &IMAGE_LINE(image,row).chars[char_index]) ;

				if( image->cursor.row == row && image->cursor.char_index == char_index)
				{
					fprintf( stderr , "**") ;
				}
			}
		}
		
		fprintf( stderr , "\n") ;
	}

	kik_debug_printf( KIK_DEBUG_TAG " <=== end of image.\n\n") ;
}

void
ml_image_dump_updated(
	ml_image_t *  image
	)
{
	int  row ;

	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		fprintf( stderr , "(%.2d)%d " , row , IMAGE_LINE(image,row).is_modified) ;
	}

	fprintf( stderr , "\n") ;
}

#endif

