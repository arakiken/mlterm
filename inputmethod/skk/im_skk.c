/*
 *	$Id$
 */

#include  <stdio.h>

#include  <kiklib/kik_mem.h>	/* malloc/alloca/free */
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup kik_str_sep kik_snprintf*/
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_conf_io.h>

#include  <x_im.h>

#include  "../im_info.h"
#include  "mkf_str_parser.h"
#include  "dict.h"


#if  0
#define  IM_SKK_DEBUG  1
#endif

#define  MAX_DIGIT_NUM  3	/* 100 */
#define  MAX_CANDS  100
#define  CAND_WINDOW_ROWS  5


typedef u_int16_t wchar ;

typedef struct im_skk
{
	/* input method common object */
	x_im_t  im ;

	int  is_enabled ;
	/*
	 * 1: direct input 2: convert 3: convert with okurigana
	 * 4: convert with sokuon okurigana
	 */
	int  is_preediting ;

	ml_char_encoding_t  term_encoding ;

	char *  encoding_name ;		/* encoding of conversion engine */

	/* conv is NULL if term_encoding == skk encoding */
	mkf_parser_t *  parser_term ;	/* for term encoding */
	mkf_conv_t *  conv ;		/* for term encoding */

	mkf_char_t  preedit[32] ;
	u_int  preedit_len ;

	char *  caption ;
	char *  cands[MAX_CANDS] ;
	u_int  num_of_cands ;
	int  cur_cand ;

	int  dan ;
	int  prev_dan ;

	int  is_katakana ;

	int  is_editing_new_word ;
	mkf_char_t  new_word[32] ;
	u_int  new_word_len ;

	mkf_char_t  preedit_orig[32] ;
	u_int  preedit_orig_len ;
	int  is_preediting_orig ;
	int  prev_dan_orig ;
	mkf_char_t  visual_chars[2] ;

} im_skk_t ;


/* --- static variables --- */

static int  ref_count = 0 ;
static x_im_export_syms_t *  syms = NULL ; /* mlterm internal symbols */


/* --- static functions --- */

static void
preedit_add(
	im_skk_t *  skk ,
	wchar  wch
	)
{
	mkf_char_t  ch ;

	if( skk->preedit_len >= sizeof(skk->preedit) / sizeof(skk->preedit[0]))
	{
		return ;
	}

	if( wch > 0xff)
	{
		if( skk->is_katakana && 0xa4a1 <= wch && wch <= 0xa4f3)
		{
			wch += 0x100 ;
		}

		ch.ch[0] = (wch >> 8) & 0x7f ;
		ch.ch[1] = wch & 0x7f ;
		ch.size = 2 ;
		ch.cs = JISX0208_1983 ;
	}
	else
	{
		ch.ch[0] = wch ;
		ch.size = 1 ;
		ch.cs = US_ASCII ;
	}
	ch.property = 0 ;

	skk->preedit[skk->preedit_len++] = ch ;
}

static void
preedit_delete(
	im_skk_t *  skk
	)
{
	--skk->preedit_len ;
}

static void
preedit_clear(
	im_skk_t *  skk
	)
{
	skk->preedit_len = 0 ;
	skk->is_preediting = 0 ;
	skk->cur_cand = -1 ;
	skk->dan = 0 ;
	skk->prev_dan = 0 ;
}

static void
preedit_to_visual(
	im_skk_t *  skk
	)
{
	if( skk->prev_dan)
	{
		if( skk->is_preediting == 4)
		{
			skk->preedit[skk->preedit_len] = skk->visual_chars[1] ;
			skk->preedit[skk->preedit_len - 1] = skk->visual_chars[0] ;
			skk->preedit_len ++ ;
		}
		else
		{
			skk->preedit[skk->preedit_len - 1] = skk->visual_chars[0] ;
		}
	}
}

static void
preedit_backup_visual_chars(
	im_skk_t *  skk
	)
{
	if( skk->prev_dan)
	{
		if( skk->is_preediting == 4)
		{
			skk->visual_chars[1] = skk->preedit[skk->preedit_len - 1] ;
			skk->visual_chars[0] = skk->preedit[skk->preedit_len - 2] ;
		}
		else
		{
			skk->visual_chars[0] = skk->preedit[skk->preedit_len - 1] ;
		}
	}
}

static void
preedit(
	im_skk_t *  skk ,
	mkf_char_t *  preedit ,
	u_int  preedit_len ,
	int  rev_len ,
	char *  candidateword ,		/* already converted to term encoding */
	size_t  candidateword_len ,
	char *  pos
	)
{
	int  x ;
	int  y ;

	if( skk->preedit_orig_len > 0)
	{
		mkf_char_t *  p ;

		if( ( p = alloca( sizeof(*p) * (skk->preedit_orig_len + 1 +
					preedit_len + skk->new_word_len))))
		{
			memcpy( p , skk->preedit_orig , sizeof(*p) * skk->preedit_orig_len) ;
			p[skk->preedit_orig_len].ch[0] = ':' ;
			p[skk->preedit_orig_len].size = 1 ;
			p[skk->preedit_orig_len].property = 0 ;
			p[skk->preedit_orig_len].cs = US_ASCII ;

			if( skk->new_word_len > 0)
			{
				memcpy( p + skk->preedit_orig_len + 1 , skk->new_word ,
					sizeof(*p) * skk->new_word_len) ;
			}

			if( preedit_len > 0)
			{
				memcpy( p + skk->preedit_orig_len + 1 + skk->new_word_len ,
					preedit , preedit_len * sizeof(*p)) ;
			}

			preedit = p ;
			preedit_len += (skk->preedit_orig_len + 1 + skk->new_word_len) ;
		}
	}

	if( preedit == NULL)
	{
		goto  candidate ;
	}
	else if( preedit_len == 0)
	{
		if( skk->im.preedit.filled_len > 0)
		{
			/* Stop preediting. */
			skk->im.preedit.filled_len = 0 ;
		}
	}
	else
	{
		mkf_char_t  ch ;
		ml_char_t *  p ;
		size_t  pos_len ;
		u_int  count ;
		ml_unicode_policy_t  upolicy ;

		pos_len = strlen(pos) ;

		if( ( p = realloc( skk->im.preedit.chars ,
				sizeof(ml_char_t) * (preedit_len + pos_len))) == NULL)
		{
			return ;
		}

		(*syms->ml_str_init)( skk->im.preedit.chars = p ,
			skk->im.preedit.num_of_chars = ( preedit_len + pos_len)) ;

		upolicy = (*skk->im.listener->get_unicode_policy)( skk->im.listener->self) ;
		if( skk->term_encoding == ML_UTF8)
		{
			upolicy |= ONLY_USE_UNICODE_FONT ;
		}
		else
		{
			upolicy |= NOT_USE_UNICODE_FONT ;
		}

		skk->im.preedit.filled_len = 0 ;
		for( count = 0 ; count < preedit_len ; count++)
		{
			int  is_fullwidth ;
			int  is_comb ;

			ch = preedit[count] ;

			if( (*syms->ml_convert_to_internal_ch)( &ch , upolicy , US_ASCII) <= 0)
			{
				continue ;
			}

			if( ch.property & MKF_FULLWIDTH)
			{
				is_fullwidth = 1 ;
			}
			else if( ch.property & MKF_AWIDTH)
			{
				/* TODO: check col_size_of_width_a */
				is_fullwidth = 1 ;
			}
			else
			{
				is_fullwidth = IS_FULLWIDTH_CS(ch.cs) ;
			}

			if( ch.property & MKF_COMBINING)
			{
				is_comb = 1 ;

				if( (*syms->ml_char_combine)( p - 1 ,
					mkf_char_to_int(&ch) ,
					ch.cs , is_fullwidth , is_comb ,
					ML_FG_COLOR , ML_BG_COLOR ,
					0 , 0 , 1 , 0 , 0))
				{
					continue ;
				}

				/*
				 * if combining failed , char is normally appended.
				 */
			}
			else
			{
				is_comb = 0 ;
			}

			if( 0 <= skk->im.preedit.filled_len &&
			    skk->im.preedit.filled_len < rev_len)
			{
				(*syms->ml_char_set)( p , mkf_char_to_int(&ch) , ch.cs ,
					      is_fullwidth , is_comb ,
					      ML_BG_COLOR , ML_FG_COLOR ,
					      0 , 0 , 1 , 0 , 0) ;
			}
			else
			{
				(*syms->ml_char_set)( p , mkf_char_to_int(&ch) , ch.cs ,
					      is_fullwidth , is_comb ,
					      ML_FG_COLOR , ML_BG_COLOR ,
					      0 , 0 , 1 , 0 , 0) ;
			}

			p ++ ;
			skk->im.preedit.filled_len ++ ;
		}

		for( ; pos_len > 0 ; pos_len --)
		{
			(*syms->ml_char_set)( p++ , *(pos++) , US_ASCII , 0 , 0 ,
				ML_FG_COLOR , ML_BG_COLOR , 0 , 0 , 1 , 0 , 0) ;
			skk->im.preedit.filled_len ++ ;
		}
	}

	(*skk->im.listener->draw_preedit_str)( skk->im.listener->self ,
					       skk->im.preedit.chars ,
					       skk->im.preedit.filled_len ,
					       skk->im.preedit.cursor_offset) ;

candidate:
	if( candidateword == NULL)
	{
		return ;
	}
	else if( candidateword_len == 0)
	{
		if( skk->im.stat_screen)
		{
			(*skk->im.stat_screen->delete)( skk->im.stat_screen) ;
			skk->im.stat_screen = NULL ;
		}
	}
	else
	{
		u_char *  tmp = NULL ;

		(*skk->im.listener->get_spot)( skk->im.listener->self ,
					       skk->im.preedit.chars ,
					       skk->im.preedit.segment_offset ,
					       &x , &y) ;

		if( skk->im.stat_screen == NULL)
		{
			if( ! ( skk->im.stat_screen = (*syms->x_im_status_screen_new)(
					skk->im.disp , skk->im.font_man ,
					skk->im.color_man ,
					(*skk->im.listener->is_vertical)(
						skk->im.listener->self) ,
					(*skk->im.listener->get_line_height)(
						skk->im.listener->self) ,
					x , y)))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" x_im_status_screen_new() failed.\n") ;
			#endif

				return ;
			}
		}
		else
		{
			(*skk->im.stat_screen->show)( skk->im.stat_screen) ;
			(*skk->im.stat_screen->set_spot)( skk->im.stat_screen , x , y) ;
		}

		(*skk->im.stat_screen->set)( skk->im.stat_screen ,
			skk->parser_term , candidateword) ;

		if( tmp)
		{
			free( tmp) ;
		}
	}
}

static void
commit(
	im_skk_t *  skk
	)
{
	mkf_parser_t *  parser ;
	u_char  conv_buf[256] ;
	size_t  filled_len ;

	parser = mkf_str_parser_init( skk->preedit , skk->preedit_len) ;
	(*skk->conv->init)( skk->conv) ;

	while( ! parser->is_eos)
	{
		filled_len = (*skk->conv->convert)( skk->conv , conv_buf ,
						    sizeof( conv_buf) ,
						    parser) ;
		if( filled_len == 0)
		{
			/* finished converting */
			break ;
		}

		(*skk->im.listener->write_to_term)( skk->im.listener->self ,
						    conv_buf , filled_len) ;
	}
}

static int
insert_char(
	im_skk_t *  skk ,
	u_char  key_char
	)
{
	static struct
	{
		wchar  a ;
		wchar  i ;
		wchar  u ;
		wchar  e ;
		wchar  o ;

	}  kana_table[] =
	{
		/* a */    /* i */  /* u */  /* e */  /* o */
		{ 0xa4a2 , 0xa4a4 , 0xa4a6 , 0xa4a8 , 0xa4aa } ,
		{ 0xa4d0 , 0xa4d3 , 0xa4d6 , 0xa4d9 , 0xa4dc } , /* b */
		{ 0xa4ab , 0xa4ad , 0xa4af , 0xa4b1 , 0xa4b3 } , /* c */
		{ 0xa4c0 , 0xa4c2 , 0xa4c5 , 0xa4c7 , 0xa4c9 } , /* d */
		{ 0xa4e3 , 0xa4a3 , 0xa4e5 , 0xa4a7 , 0xa4e7 } , /* xy/dh */
		{ 0 ,      0 ,      0xa4d5 , 0 ,      0 ,    } , /* f */
		{ 0xa4ac , 0xa4ae , 0xa4b0 , 0xa4b2 , 0xa4b4 } , /* g */
		{ 0xa4cf , 0xa4d2 , 0xa4d5 , 0xa4d8 , 0xa4db } , /* h */
		{ 0xa4e3 , 0 ,      0xa4e5 , 0xa4a7 , 0xa4e7 } , /* ch/sh */
		{ 0 ,      0xa4b8 , 0 ,      0 ,      0 ,    } , /* j */
		{ 0xa4ab , 0xa4ad , 0xa4af , 0xa4b1 , 0xa4b3 } , /* k */
		{ 0xa4a1 , 0xa4a3 , 0xa4a5 , 0xa4a7 , 0xa4a9 } , /* l */
		{ 0xa4de , 0xa4df , 0xa4e0 , 0xa4e1 , 0xa4e2 } , /* m */
		{ 0xa4ca , 0xa4cb , 0xa4cc , 0xa4cd , 0xa4ce } , /* n */
		{ 0 ,      0 ,      0 ,      0 ,      0 ,    } ,
		{ 0xa4d1 , 0xa4d4 , 0xa4d7 , 0xa4da , 0xa4dd } , /* p */
		{ 0 ,      0 ,      0 ,      0 ,      0 ,    } ,
		{ 0xa4e9 , 0xa4ea , 0xa4eb , 0xa4ec , 0xa4ed } , /* r */
		{ 0xa4b5 , 0xa4b7 , 0xa4b9 , 0xa4bb , 0xa4bd } , /* s */
		{ 0xa4bf , 0xa4c1 , 0xa4c4 , 0xa4c6 , 0xa4c8 } , /* t */
		{ 0 ,      0 ,      0 ,      0 ,      0 ,    } ,
		{ 0 ,      0 ,      0 ,      0 ,      0 ,    } ,
		{ 0xa4ef , 0xa4f0 , 0      , 0xa4f1 , 0xa4f2 } , /* w */
		{ 0xa4a1 , 0xa4a3 , 0xa4a5 , 0xa4a7 , 0xa4a9 } , /* x */
		{ 0xa4e4 , 0 ,      0xa4e6 , 0      , 0xa4e8 } , /* y */
		{ 0xa4b6 , 0xa4b8 , 0xa4ba , 0xa4bc , 0xa4be } , /* z */
	} ;
	static wchar  sign_table1[] =
	{
	                 0xa1aa , 0xa1c9 , 0xa1f4 , 0xa1f0 , 0xa1f3 , 0xa1f5 , 0xa1c7 ,
		0xa1ca , 0xa1cb , 0xa1f6 , 0xa1dc , 0xa1a4 , 0xa1bc , 0xa1a3 , 0xa1bf ,
		0xa3b0 , 0xa3b1 , 0xa3b2 , 0xa3b3 , 0xa3b4 , 0xa3b5 , 0xa3b6 , 0xa3b7 ,
		0xa3b8 , 0xa3b9 , 0xa1a7 , 0xa1a8 , 0xa1e3 , 0xa1e1 , 0xa1e4 , 0xa1a9 ,
		0xa1f7 ,
	} ;
	static wchar  sign_table2[] =
	{
		0xa1ce , 0xa1ef , 0xa1cf , 0xa1b0 , 0xa1b2 ,
	} ;
	static wchar  sign_table3[] =
	{
		0xa1d0 , 0xa1c3 , 0xa1d1 , 0xa1c1 ,
	} ;
	wchar  wch ;

	if( skk->dan)
	{
		preedit_delete( skk) ;
	}

	if( key_char == 'a' || key_char == 'i' || key_char == 'u' ||
	    key_char == 'e' || key_char == 'o')
	{
		if( skk->dan == 'f' - 'a')
		{
			if( key_char != 'u')
			{
				preedit_add( skk , 0xa4d5) ;	/* hu */
				skk->dan = 'x' - 'a' ;
			}
		}
		else if( skk->dan == 'j' - 'a')
		{
			if( key_char != 'i')
			{
				preedit_add( skk , 0xa4b8) ;	/* zi */
				skk->dan = 'e' - 'a' ;
			}
		}

		if( key_char == 'a')
		{
			wch = kana_table[skk->dan].a ;
			skk->dan = 0 ;
		}
		else if( key_char == 'i')
		{
			if( skk->dan == 'i' - 'a')
			{
				skk->dan = 0 ;

				return  0 ;	/* shi/chi */
			}

			wch = kana_table[skk->dan].i ;
			skk->dan = 0 ;
		}
		else if( key_char == 'u')
		{
			if( skk->dan == 'j' - 'a')
			{
				preedit_add( skk , 0xa4b8) ;	/* zi */
				skk->dan = 'e' - 'a' ;
			}
			wch = kana_table[skk->dan].u ;
			skk->dan = 0 ;
		}
		else if( key_char == 'e')
		{
			if( skk->dan == 'f' - 'a')
			{
				preedit_add( skk , 0xa4d5) ;	/* hu */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'j' - 'a')
			{
				preedit_add( skk , 0xa4b8) ;	/* zi */
				skk->dan = 'x' - 'a' ;
			}
			wch = kana_table[skk->dan].e ;
			skk->dan = 0 ;
		}
		else /* if( key_char == 'o') */
		{
			if( skk->dan == 'f' - 'a')
			{
				preedit_add( skk , 0xa4d5) ;	/* hu */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'j' - 'a')
			{
				preedit_add( skk , 0xa4b8) ;	/* zi */
				skk->dan = 'e' - 'a' ;
			}
			wch = kana_table[skk->dan].o ;
			skk->dan = 0 ;
		}
	}
	else if( ( '!' <= key_char && key_char <= '@') ||
	         ( '[' <= key_char && key_char <= '_') ||
		 ( '{' <= key_char && key_char <= '~'))
	{
		if( skk->dan)
		{
			preedit_add( skk , skk->dan + 'a') ;
			skk->dan = 0 ;
		}

		if( key_char <= '@')
		{
			wch = sign_table1[key_char - '!'] ;
		}
		else if( key_char <= '_')
		{
			wch = sign_table2[key_char - '['] ;
		}
		else
		{
			wch = sign_table3[key_char - '{'] ;
		}
	}
	else
	{
		if( skk->dan == 'n' - 'a' && key_char == 'n')
		{
			wch = 0xa4f3 ;	/* n */
			skk->dan = 0 ;
		}
		else if( key_char == skk->dan + 'a')
		{
			preedit_add( skk , 0xa4c3) ;
			wch = key_char ;
		}
		else if( key_char == 'y')
		{
			if( skk->dan == 'k' - 'a')
			{
				preedit_add( skk , 0xa4ad) ;	/* ki */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'g' - 'a')
			{
				preedit_add( skk , 0xa4ae) ;	/* gi */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 's' - 'a')
			{
				preedit_add( skk , 0xa4b7) ;	/* si */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'z' - 'a')
			{
				preedit_add( skk , 0xa4b8) ;	/* zi */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 't' - 'a' || skk->dan == 'c' - 'a')
			{
				preedit_add( skk , 0xa4c1) ;	/* ti */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'd' - 'a')
			{
				preedit_add( skk , 0xa4c2) ;	/* di */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'n' - 'a')
			{
				preedit_add( skk , 0xa4cb) ;	/* ni */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'h' - 'a')
			{
				preedit_add( skk , 0xa4d2) ;	/* hi */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'b' - 'a')
			{
				preedit_add( skk , 0xa4d3) ;	/* bi */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'p' - 'a')
			{
				preedit_add( skk , 0xa4d4) ;	/* pi */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'm' - 'a')
			{
				preedit_add( skk , 0xa4df) ;	/* mi */
				skk->dan = 'x' - 'a' ;
			}
			else if( skk->dan == 'r' - 'a')
			{
				preedit_add( skk , 0xa4ea) ;	/* ri */
				skk->dan = 'x' - 'a' ;
			}

			if( skk->dan == 'x' - 'a')
			{
				skk->dan = 'e' - 'a' ;
				wch = 'y' ;
			}
			else if( skk->dan == 'v' - 'a')
			{
				if( key_char == 'u')
				{
					wch = 0xa5f4 ;
					skk->dan = 0 ;
				}
				else
				{
					preedit_add( skk , 0xa5f4) ;	/* v */
					skk->dan = 'x' - 'a' ;

					return  insert_char( skk , key_char) ;
				}
			}
			else
			{
				goto  normal ;
			}
		}
		else if( key_char == 'h')
		{
			if( skk->dan == 'c' - 'a')
			{
				preedit_add( skk , 0xa4c1) ;	/* ti */
				skk->dan = 'i' - 'a' ;
			}
			else if( skk->dan == 's' - 'a')
			{
				preedit_add( skk , 0xa4b7) ;	/* si */
				skk->dan = 'i' - 'a' ;
			}
			else if( skk->dan == 'd' - 'a')
			{
				preedit_add( skk , 0xa4c7) ;	/* di */
				skk->dan = 'e' - 'a' ;
			}
			else
			{
				goto  normal ;
			}

			wch = 'h' ;
		}
		else
		{
		normal:
			if( skk->dan)
			{
				preedit_add( skk , skk->dan + 'a') ;
			}

			wch = key_char ;

			if( 'a' <= key_char && key_char <= 'z')
			{
				skk->dan = key_char - 'a' ;
			}
			else
			{
				skk->dan = 0 ;
			}
		}
	}

	if( wch == 0)
	{
		return  1 ;
	}

	preedit_add( skk , wch) ;

	return  0 ;
}

/*
 * methods of x_im_t
 */

static int
delete(
	x_im_t *  im
	)
{
	im_skk_t *  skk ;

	skk = (im_skk_t*) im ;

	(*skk->parser_term->delete)( skk->parser_term) ;

	if( skk->conv)
	{
		(*skk->conv->delete)( skk->conv) ;
	}

	free( skk) ;

	ref_count -- ;

	if( ref_count == 0)
	{
		dict_final() ;
	}

#ifdef  IM_SKK_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count) ;
#endif

	return  ref_count ;
}

static int
switch_mode(
	x_im_t *  im
	)
{
	im_skk_t *  skk ;

	skk = (im_skk_t*) im ;

	if( ( skk->is_enabled = ( ! skk->is_enabled)))
	{
		preedit( skk , NULL , 0 , 0 , NULL , 0 , "") ;
	}
	else
	{
		preedit_clear( skk) ;
		preedit( skk , "" , 0 , 0 , "" , 0 , "") ;
	}

	return  1 ;
}


static int
candidate_exists(
	const char **  cands ,
	u_int  num_of_cands ,
	const char *  cand
	)
{
	u_int  count ;

	for( count = 0 ; count < num_of_cands ; count++)
	{
		if( strcmp( cands[count] , cand) == 0)
		{
			return  1 ;
		}
	}

	return  0 ;
}

static void
candidate_get(
	im_skk_t *  skk
	)
{
	char *  p ;
	char *  p2 ;
	int  count ;

#ifdef  DEBUG
	if( skk->num_of_cands > 0)
	{
		kik_debug_printf( KIK_DEBUG_TAG
			" candidate_get() is called before candidate_clear().\n") ;
	}
#endif

	if( ! ( skk->caption = dict_search( skk->preedit , skk->preedit_len , skk->conv)))
	{
		skk->num_of_cands = 0 ;

		return ;
	}

	count = 0 ;
	p = strchr( skk->caption , '/') ;	/* skip caption */
	*(p++) = '\0' ;

	while( *p)
	{
		/* skip [] */
		if( *p == '[')
		{
			if( ( p2 = strstr( p + 1 , "]/")))
			{
				p = p2 + 2 ;

				continue ;
			}
		}

		skk->cands[count] = p ;
		if( ( p2 = strchr( p , ';')))
		{
			*(p++) = '\0' ;
		}
		if( ( p = strchr( p , '/')))
		{
			*(p++) = '\0' ;
		}

		if( ! candidate_exists( skk->cands , count , skk->cands[count]))
		{
			count ++ ;
		}

		if( ! p || count >= MAX_CANDS - 1)
		{
			break ;
		}
	}

	skk->num_of_cands = count ;
}

static void
start_to_register_new_word(
	im_skk_t *  skk
	)
{
	memcpy( skk->preedit_orig , skk->preedit ,
		sizeof(skk->preedit[0]) * skk->preedit_len) ;
	if( skk->prev_dan)
	{
		if( skk->is_preediting == 4)
		{
			skk->preedit_len -- ;
		}

		skk->preedit_orig[skk->preedit_len - 1].ch[0] = skk->prev_dan + 'a' ;
		skk->preedit_orig[skk->preedit_len - 1].size = 1 ;
		skk->preedit_orig[skk->preedit_len - 1].cs = US_ASCII ;
		skk->preedit_orig[skk->preedit_len - 1].property = 0 ;
	}

	skk->preedit_orig_len = skk->preedit_len ;
	skk->is_preediting_orig = skk->is_preediting ;
	skk->dan = 0 ;
	skk->prev_dan_orig = skk->prev_dan ;
	skk->num_of_cands = 0 ;

	skk->new_word_len = 0 ;
	skk->is_editing_new_word = 1 ;
	preedit_clear( skk) ;
	skk->is_preediting = 1 ;
}

static void
stop_to_register_new_word(
	im_skk_t *  skk
	)
{
	memcpy( skk->preedit , skk->preedit_orig ,
		sizeof(skk->preedit_orig[0]) * skk->preedit_orig_len) ;
	skk->preedit_len = skk->preedit_orig_len ;
	skk->preedit_orig_len = 0 ;
	skk->dan = 0 ;
	skk->prev_dan = skk->prev_dan_orig ;

	skk->new_word_len = 0 ;
	skk->is_editing_new_word = 0 ;
	skk->is_preediting = skk->is_preediting_orig ;

	preedit_to_visual( skk) ;
}

static void
candidate_set(
	im_skk_t *  skk
	)
{
	if( skk->preedit_len == 0)
	{
		return ;
	}

	if( skk->prev_dan)
	{
		if( skk->is_preediting == 4)
		{
			skk->visual_chars[1] = skk->preedit[--skk->preedit_len] ;
		}

		skk->visual_chars[0] = skk->preedit[skk->preedit_len - 1] ;
		skk->preedit[skk->preedit_len - 1].ch[0] = skk->prev_dan + 'a' ;
		skk->preedit[skk->preedit_len - 1].size = 1 ;
		skk->preedit[skk->preedit_len - 1].cs = US_ASCII ;
		skk->preedit[skk->preedit_len - 1].property = 0 ;
	}

	if( skk->num_of_cands == 0)
	{
		candidate_get( skk) ;

		if( skk->num_of_cands == 0)
		{
		#if  0
			if( skk->prev_dan)
			{
				skk->preedit[skk->preedit_len - 1] = skk->visual_chars[0] ;

				if( skk->is_preediting == 4)
				{
					skk->preedit_len ++ ;
				}
			}
		#endif

			start_to_register_new_word( skk) ;

			skk->cur_cand = -1 ;

			return ;
		}
	}

	if( skk->cur_cand == -1)
	{
		skk->cur_cand = 0 ;
	}

	(*skk->parser_term->init)( skk->parser_term) ;
	(*skk->parser_term->set_str)( skk->parser_term , (u_char*)skk->cands[skk->cur_cand] ,
		strlen( skk->cands[skk->cur_cand])) ;

	skk->preedit_len = 0 ;
	while( (*skk->parser_term->next_char)( skk->parser_term ,
			skk->preedit + (skk->preedit_len ++)) && ! skk->parser_term->is_eos) ;

	if( skk->prev_dan)
	{
		skk->preedit[skk->preedit_len++] = skk->visual_chars[0] ;

		if( skk->is_preediting == 4)
		{
			skk->preedit[skk->preedit_len++] = skk->visual_chars[1] ;
		}
	}

	if( skk->dan)
	{
		mkf_char_t *  ch ;

		ch = skk->preedit + (skk->preedit_len++) ;
		ch->ch[0] = skk->dan + 'a' ;
		ch->cs = US_ASCII ;
		ch->size = 0 ;
		ch->property = 0 ;
	}
}

static void
candidate_unset(
	im_skk_t *  skk
	)
{
	(*skk->parser_term->init)( skk->parser_term) ;
	(*skk->parser_term->set_str)( skk->parser_term , (u_char*)skk->caption ,
		strlen( skk->caption)) ;

	skk->preedit_len = 0 ;
	while( (*skk->parser_term->next_char)( skk->parser_term ,
			skk->preedit + (skk->preedit_len ++)) && ! skk->parser_term->is_eos) ;

	preedit_to_visual( skk) ;

	skk->cur_cand = -1 ;
}

static void
candidate_clear(
	im_skk_t *  skk
	)
{
	if( skk->num_of_cands > 0)
	{
		free( skk->caption) ;
		skk->cur_cand = -1 ;
		skk->num_of_cands = 0 ;
	}
}

static int
fix(
	im_skk_t *  skk
	)
{
	if( skk->preedit_len > 0)
	{
		if( skk->num_of_cands > 0)
		{
			dict_add( skk->caption , skk->cands[skk->cur_cand] , skk->parser_term) ;
		}

		if( skk->is_editing_new_word)
		{
			memcpy( skk->new_word + skk->new_word_len , skk->preedit ,
				skk->preedit_len * sizeof(skk->preedit[0])) ;
			skk->new_word_len += skk->preedit_len ;
			preedit( skk , "" , 0 , 0 , "" , 0 , "") ;
			preedit_clear( skk) ;
			skk->is_preediting = 1 ;
		}
		else
		{
			preedit( skk , "" , 0 , 0 , "" , 0 , "") ;
			commit( skk) ;
			preedit_clear( skk) ;
		}
		candidate_clear( skk) ;

		return  0 ;
	}
	else if( skk->is_editing_new_word)
	{
		if( skk->new_word_len > 0)
		{
			mkf_parser_t *  parser ;
			char  caption[1024] ;
			char  word[1024] ;
			size_t  len ;

			parser = mkf_str_parser_init( skk->preedit_orig , skk->preedit_orig_len) ;
			(*skk->conv->init)( skk->conv) ;
			len = (*skk->conv->convert)( skk->conv , caption , sizeof(caption) ,
					parser) ;
			caption[len] = '\0' ;

			parser = mkf_str_parser_init( skk->new_word , skk->new_word_len) ;
			(*skk->conv->init)( skk->conv) ;
			len = (*skk->conv->convert)( skk->conv , word , sizeof(word) ,
					parser) ;
			word[len] = '\0' ;

			dict_add( caption , word , skk->parser_term) ;

			candidate_clear( skk) ;
			stop_to_register_new_word( skk) ;
			candidate_set( skk) ;
			commit( skk) ;
			preedit_clear( skk) ;
			candidate_clear( skk) ;
		}
		else
		{
			stop_to_register_new_word( skk) ;
			preedit_clear( skk) ;
			candidate_clear( skk) ;
		}

		return  0 ;
	}

	return  1 ;
}

static int
key_event(
	x_im_t *  im ,
	u_char  key_char ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	im_skk_t *  skk ;
	int  ret = 0 ;
	char *  pos = "" ;
	char *  cand = NULL ;
	u_int  cand_len = 0 ;

	skk = (im_skk_t*) im ;

	if( key_char == ' ' && ( event->state & ShiftMask))
	{
		fix( skk) ;
		switch_mode( im) ;

		if( ! skk->is_enabled)
		{
			return  0 ;
		}
	}
	else if( ! skk->is_enabled)
	{
		return  1 ;
	}
	else if( key_char == 'l')
	{
		/* skk is disabled */
		fix( skk) ;
		switch_mode( im) ;

		return  0 ;
	}
	else if( key_char == '\r' || key_char == '\n')
	{
		ret = fix( skk) ;
	}
	else if( ksym == XK_BackSpace || ksym == XK_Delete || key_char == 0x08 /* Ctrl+h */)
	{
		if( skk->preedit_len > 0)
		{
			if( skk->cur_cand != -1)
			{
				candidate_unset( skk) ;
				cand = "" ;
			}
			else
			{
				skk->dan = skk->prev_dan = 0 ;
				skk->is_preediting = 1 ;

				candidate_clear( skk) ;
				preedit_delete( skk) ;
			}
		}
		else
		{
			ret = 1 ;
		}
	}
	else if( ( event->state & ControlMask) && key_char == '\x07')
	{
		if( skk->cur_cand != -1)
		{
			candidate_unset( skk) ;
			cand = "" ;
		}
		else if( skk->is_editing_new_word)
		{
			stop_to_register_new_word( skk) ;
			cand = "" ;
		}
	}
	else if( key_char != ' ' && key_char != '\0')
	{
		if( key_char < ' ')
		{
			ret = 1 ;
		}
		else if( key_char == 'q')
		{
			skk->is_katakana = ! skk->is_katakana ;
		}
		else
		{
			if( skk->cur_cand != -1 && ! skk->dan)
			{
				fix( skk) ;
			}

			while( event->state & ShiftMask)
			{
				if( 'A' <= key_char && key_char <= 'Z')
				{
					key_char += 0x20 ;
				}
				else if( key_char < 'a' || 'z' < key_char)
				{
					break ;
				}

				if( skk->preedit_len == 0)
				{
					skk->is_preediting = 1 ;
				}
				else if( skk->is_preediting && ! skk->dan)
				{
					skk->is_preediting = 2 ;
				}

				break ;
			}

			if( insert_char( skk , key_char) != 0)
			{
				ret = 1 ;
			}
			else if( skk->is_preediting >= 2)
			{
				if( skk->dan)
				{
					if( skk->dan == skk->prev_dan)
					{
						/* sokuon */
						skk->is_preediting ++ ;
					}
					else
					{
						skk->prev_dan = skk->dan ;
					}
				}
				else
				{
					if( ! skk->prev_dan &&
					    ( key_char == 'i' || key_char == 'u' ||
					      key_char == 'e' || key_char == 'o'))
					{
						skk->prev_dan = key_char - 'a' ;
					}

					skk->is_preediting ++ ;
					candidate_set( skk) ;
				}
			}
			else if( ! skk->dan && ! skk->is_preediting)
			{
				fix( skk) ;
			}
		}
	}
	else if( skk->is_preediting &&
	         ( key_char == ' ' || ksym == XK_Up || ksym == XK_Down ||
	           ksym == XK_Right || ksym == XK_Left))
	{
		int  update = 0 ;

		if( skk->cur_cand == -1)
		{
			if( key_char != ' ')
			{
				return  1 ;
			}
		}
		else
		{
			if( ! skk->im.cand_screen)
			{
				update = 1 ;
			}

			if( ksym == XK_Left)
			{
				if( skk->cur_cand % CAND_WINDOW_ROWS == 0)
				{
					if( skk->cur_cand == 0)
					{
						skk->cur_cand = skk->num_of_cands - 1 ;
					}

					update = 1 ;
				}

				skk->cur_cand -- ;
			}
			else if( ksym == XK_Up)
			{
				if( skk->cur_cand < CAND_WINDOW_ROWS)
				{
					skk->cur_cand = skk->num_of_cands - skk->cur_cand -
								CAND_WINDOW_ROWS ;
				}
				else
				{
					skk->cur_cand -= CAND_WINDOW_ROWS ;
				}

				update = 1 ;
			}
			else if( ksym == XK_Down)
			{
				skk->cur_cand += CAND_WINDOW_ROWS ;

				if( skk->cur_cand >= skk->num_of_cands)
				{
					skk->cur_cand -= skk->num_of_cands ;
				}

				update = 1 ;
			}
			else
			{
				if( skk->cur_cand == skk->num_of_cands - 1)
				{
					if( ! skk->is_editing_new_word)
					{
						preedit_backup_visual_chars( skk) ;
						candidate_unset( skk) ;
						candidate_clear( skk) ;
						start_to_register_new_word( skk) ;
						cand = "" ;

						goto  end ;
					}
					else
					{
						skk->cur_cand = 0 ;
						update = 1 ;
					}
				}
				else
				{
					if( skk->cur_cand % CAND_WINDOW_ROWS ==
					    CAND_WINDOW_ROWS - 1)
					{
						update = 1 ;
					}

					skk->cur_cand ++ ;
				}
			}
		}

		candidate_set( skk) ;

		if( skk->num_of_cands > 1 && update)
		{
			char *  dst ;
			u_int  count ;
			int  start_cand ;

			start_cand = skk->cur_cand - (skk->cur_cand % CAND_WINDOW_ROWS) ;

			for( count = 0 ;
			     count < CAND_WINDOW_ROWS &&
			     start_cand + count < skk->num_of_cands ;
			     count++)
			{
				cand_len +=
					(MAX_DIGIT_NUM + 1 +
					 strlen(skk->cands[start_cand + count]) +
					 1) ;
			}

			if( ( dst = cand = alloca( cand_len * sizeof(mkf_char_t))))
			{
				for( count = 0 ;
				     count < CAND_WINDOW_ROWS &&
				     start_cand + count < skk->num_of_cands ;
				     count++)
				{
					sprintf( dst , "%d %s " ,
						start_cand + count + 1 ,
						skk->cands[start_cand + count]) ;
					dst += strlen(dst) ;
				}
				/* -1 removes the last ' ' */
				cand_len = (dst - cand - 1) ;
			}
			else
			{
				cand_len = 0 ;
			}
		}

		if( skk->num_of_cands > 0 && ( pos = alloca( 4 + DIGIT_STR_LEN(int) * 2 + 1)))
		{
			sprintf( pos , " [%d/%d]" , skk->cur_cand + 1 ,
				skk->num_of_cands) ;
		}
	}
	else
	{
		ret = 1 ;
	}

end:
	preedit( skk , skk->preedit , skk->preedit_len ,
		(skk->is_preediting && ! skk->is_editing_new_word) ? skk->preedit_len : 0 ,
		cand , cand_len , pos) ;

	return  ret ;
}

static int
is_active(
	x_im_t *  im
	)
{
	return  ((im_skk_t*)im)->is_enabled ;
}

static void
focused(
	x_im_t *  im
	)
{
	im_skk_t *  skk ;

	skk =  (im_skk_t*)  im ;

	if( skk->im.cand_screen)
	{
		(*skk->im.cand_screen->show)( skk->im.cand_screen) ;
	}
}

static void
unfocused(
	x_im_t *  im
	)
{
	im_skk_t *  skk ;

	skk = (im_skk_t*)  im ;

	if( skk->im.cand_screen)
	{
		(*skk->im.cand_screen->hide)( skk->im.cand_screen) ;
	}
}


/* --- global functions --- */

x_im_t *
im_skk_new(
	u_int64_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_export_syms_t *  export_syms ,
	char *  engine ,
	u_int  mod_ignore_mask
	)
{
	im_skk_t *  skk ;

	if( magic != (u_int64_t) IM_API_COMPAT_CHECK_MAGIC)
	{
		kik_error_printf( "Incompatible input method API.\n") ;

		return  NULL ;
	}

	if( ref_count == 0)
	{
		syms = export_syms ;
	}

	if( ! ( skk = calloc( 1 , sizeof( im_skk_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		goto  error ;
	}

	if( engine)
	{
		dict_set_global( engine) ;
	}

	skk->term_encoding = term_encoding ;
	skk->encoding_name = (*syms->ml_get_char_encoding_name)( term_encoding) ;

	if( ! ( skk->conv = (*syms->ml_conv_new)( term_encoding)))
	{
		goto  error ;
	}

	if( ! ( skk->parser_term = (*syms->ml_parser_new)( term_encoding)))
	{
		goto  error ;
	}

	skk->cur_cand = -1 ;

	/*
	 * set methods of x_im_t
	 */
	skk->im.delete = delete ;
	skk->im.key_event = key_event ;
	skk->im.switch_mode = switch_mode ;
	skk->im.is_active = is_active ;
	skk->im.focused = focused ;
	skk->im.unfocused = unfocused ;

	ref_count ++;

#ifdef  IM_SKK_DEBUG
	kik_debug_printf("New object was created. ref_count is %d.\n", ref_count) ;
#endif

	return  (x_im_t*) skk ;

error:
	if( skk)
	{
		if( skk->parser_term)
		{
			(*skk->parser_term->delete)( skk->parser_term) ;
		}

		if( skk->conv)
		{
			(*skk->conv->delete)( skk->conv) ;
		}

		free( skk) ;
	}

	return  NULL ;
}


/* --- API for external tools --- */

im_info_t *
im_skk_get_info(
	char *  locale ,
	char *  encoding
	)
{
	im_info_t *  result ;

	if( ( result = malloc( sizeof( im_info_t))))
	{
		result->id = strdup( "skk") ;
		result->name = strdup( "Skk") ;
		result->num_of_args = 0 ;
		result->args = NULL ;
		result->readable_args = NULL ;
	}

	return  result ;
}

