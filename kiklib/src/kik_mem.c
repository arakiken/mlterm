/*
 *	$Id$
 */

/*
 * !! Notice !!
 * don't use other kik_xxx functions(macros may be ok) as much as possible since they may use
 * these memory management functions.
 */

#include  "kik_mem.h"

#include  <stdio.h>	/* fprintf */
#include  <string.h>	/* memset */

#include  "kik_list.h"


#if  0
#define   __DEBUG
#endif


#undef  malloc
#undef  calloc
#undef  realloc
#undef  free
#undef  kik_mem_free_all


typedef struct  mem_log
{
	void *  ptr ;

	size_t  size ;
	
	const char *  file ;
	int  line ;
	const char *  func ;

} mem_log_t ;


KIK_LIST_TYPEDEF( mem_log_t) ;


/* --- static variables --- */

static KIK_LIST(mem_log_t)  mem_logs = NULL ;


/* --- static functions --- */

static KIK_LIST(mem_log_t) 
get_mem_logs(void)
{
	if( mem_logs == NULL)
	{
		kik_list_new( mem_log_t , mem_logs) ;
	}

	return  mem_logs ;
}

static mem_log_t *
search_mem_log(
	void *  ptr
	)
{
	KIK_ITERATOR(mem_log_t)   iterator = NULL ;

	iterator = kik_list_first( get_mem_logs()) ;
	while( iterator)
	{
		if( kik_iterator_indirect( iterator) == NULL)
		{
			kik_error_printf(
				"iterator found , but it has no logs."
				"don't you cross over memory boundaries anywhere?\n") ;
		}
		else if( kik_iterator_indirect( iterator)->ptr == ptr)
		{
			return  kik_iterator_indirect( iterator) ;
		}

		iterator = kik_iterator_next( iterator) ;
	}

	return  NULL ;
}


/* --- global functions --- */

void *
kik_mem_malloc(
	size_t  size ,
	const char *  file ,	/* should be allocated memory. */
	int  line ,
	const char *  func	/* should be allocated memory. */
	)
{
	mem_log_t *  log = NULL ;

	if( ( log = malloc( sizeof( mem_log_t))) == NULL)
	{
		return  NULL ;
	}
	
	if( ( log->ptr = malloc( size)) == NULL)
	{
		free( log) ;
	
		return  NULL ;
	}

	log->size = size ;
	log->file = file ;
	log->line = line ;
	log->func = func ;

	kik_list_insert_head( mem_log_t , get_mem_logs() , log) ;

#ifdef  __DEBUG
	fprintf( stderr ,
		" memory %p(size %d) was successfully allocated at %s(l.%d in %s) , logged in %p.\n" ,
		log->ptr , log->size , log->func , log->line , log->file , log) ;
#endif
		
	return  log->ptr ;
}

void *
kik_mem_calloc(
	size_t  number ,
	size_t  size ,
	const char *  file ,	/* should be allocated memory. */
	int  line ,
	const char *  func	/* should be allocated memory. */
	)
{
	void *  ptr = NULL ;

	if( ( ptr = kik_mem_malloc( number * size , file , line , func)) == NULL)
	{
		return  NULL ;
	}

	memset( ptr , 0 , number * size) ;

	return  ptr ;
}

void
kik_mem_free(
	void *  ptr ,
	const char *  file ,	/* should be allocated memory. */
	int  line ,
	const char *  func	/* should be allocated memory. */
	)
{
	mem_log_t *  log ;

	if( ptr == NULL)
	{
		free( ptr) ;
	}
	else if( ( log = search_mem_log( ptr)) == NULL)
	{
	#ifdef  DEBUG
		fprintf( stderr , " %p is freed at %s[l.%d in %s] but not logged.\n" ,
			ptr , func , line , file) ;
	#endif
	
		free( ptr) ;
	}
	else
	{
	#ifdef  __DEBUG
		fprintf( stderr , 
			" %p(size %d , alloced at %s[l.%d in %s] and freed at %s[l.%d in %s] logged in %p) was successfully freed.\n" ,
			ptr , log->size , log->func , log->line , log->file , file , line , func , log) ;
	#endif
		
		kik_list_search_and_remove( mem_log_t , get_mem_logs() , log) ;
		memset( ptr , 0xff , log->size) ;
		free( log) ;
		free( ptr) ;
	}
}

void *
kik_mem_realloc(
	void *  ptr ,
	size_t  size ,
	const char *  file ,	/* should be allocated memory. */
	int  line ,
	const char *  func	/* should be allocated memory. */
	)
{
	void *  new_ptr = NULL ;
	mem_log_t *  log = NULL ;

	if( ptr == NULL)
	{
		return  kik_mem_malloc( size , file , line , func) ;
	}
	
	if( ( log = search_mem_log( ptr)) == NULL)
	{
	#ifdef  DEBUG
		fprintf( stderr , " %p is reallocated at %s[l.%d in %s] but not logged.\n" ,
			ptr , func , line , file) ;
	#endif

		return  realloc( ptr , size) ;
	}

	/*
	 * allocate new memory.
	 */
	if( ( new_ptr = kik_mem_malloc( size , file , line , func)) == NULL)
	{
		return  NULL ;
	}

	if( log->size < size)
	{
		memcpy( new_ptr , ptr , log->size) ;
	}
	else
	{
		memcpy( new_ptr , ptr , size) ;
	}

	/*
	 * free old memory.
	 */
	kik_mem_free( ptr , __FILE__ , __LINE__ , __FUNCTION__) ;
	
	return  new_ptr ;
}

int
kik_mem_free_all(void)
{
	KIK_ITERATOR( mem_log_t)   iterator = NULL ;

	iterator = kik_list_first( get_mem_logs()) ;
	if( iterator)
	{
		while( iterator)
		{
			fprintf( stderr , "%p(size %d , alloced at %s[l.%d in %s] is not freed.\n" ,
				kik_iterator_indirect( iterator)->ptr ,
				kik_iterator_indirect( iterator)->size ,
				kik_iterator_indirect( iterator)->func ,
				kik_iterator_indirect( iterator)->line ,
				kik_iterator_indirect( iterator)->file) ;

			free( kik_iterator_indirect( iterator)->ptr) ;
			free( kik_iterator_indirect( iterator)) ;
			
			iterator = kik_iterator_next( iterator) ;
		}

		kik_list_delete( mem_log_t , get_mem_logs()) ;
		mem_logs = NULL ;

		return  0 ;
	}
	else
	{
	#ifdef  __DEBUG
		fprintf( stderr , "YOUR MEMORY MANAGEMENT IS PERFECT!") ;
	#endif

		kik_list_delete( mem_log_t , get_mem_logs()) ;
		mem_logs = NULL ;
	
		return  1 ;
	}
}


#ifndef  HAVE_ALLOCA

#if  0
#define  __DEBUG
#endif

#define  ALLOCA_PAGE_UNIT_SIZE  4096
#define  MAX_STACK_FRAMES  10

typedef struct  alloca_page
{
	void *  ptr ;
	size_t  size ;
	size_t  used_size ;

	struct alloca_page *  prev_page ;

} alloca_page_t ;


/* --- static variables --- */

static size_t  total_allocated_size = 0 ;
static alloca_page_t *  alloca_page ;
static int  stack_frame_bases[MAX_STACK_FRAMES] ;
static int  current_stack_frame = 0 ;


/* --- global functions --- */

void *
kik_alloca(
	size_t  size
	)
{
	void *  ptr ;

	/* arranging bytes boundary */
	size = ((size + sizeof(char *) - 1) / sizeof(char *)) * sizeof(char *) ;
	
	if( alloca_page == NULL || alloca_page->used_size + size > alloca_page->size)
	{
		alloca_page_t *  new_page ;
		
		if( ( new_page = malloc( sizeof( alloca_page_t))) == NULL)
		{
			return  NULL ;
		}
		
		new_page->size = ((size / ALLOCA_PAGE_UNIT_SIZE) + 1) * ALLOCA_PAGE_UNIT_SIZE ;
		new_page->used_size = 0 ;
		
		if( ( new_page->ptr = malloc( new_page->size)) == NULL)
		{
			free( new_page) ;
			
			return  NULL ;
		}

	#ifdef  __DEBUG
		fprintf( stderr , "new page(size %d) created.\n" , new_page->size) ;
	#endif
	
		new_page->prev_page = alloca_page ;
		alloca_page = new_page ;
	}

	/* operations of void * cannot be done in some operating systems */
	ptr = (char *)alloca_page->ptr + alloca_page->used_size ;
	
	alloca_page->used_size += size ;
	total_allocated_size += size ;

#ifdef  __DEBUG
	fprintf( stderr , "memory %p(size %d) was successfully allocated.\n" , ptr , size) ;
#endif
	
	return  ptr ;
}

int
kik_alloca_begin_stack_frame(void)
{
	if( alloca_page == NULL)
	{
		/* no page is found */

		return  0 ;
	}

	current_stack_frame ++ ;
	
	if( current_stack_frame >= MAX_STACK_FRAMES)
	{
	#ifdef  __DEBUG
		fprintf( stderr , "end of stack frame.\n") ;
	#endif
	
		return  1 ;
	}
#ifdef  DEBUG
	else if( current_stack_frame < 0)
	{
		fprintf( stderr , "current stack frame must not be less than 0.\n") ;
	
		return  1 ;
	}
#endif

	stack_frame_bases[current_stack_frame] = total_allocated_size ;

#ifdef  __DEBUG
	fprintf( stderr ,  "stack frame (base %d (%d) size 0) created.\n" ,
		stack_frame_bases[current_stack_frame] , current_stack_frame) ;
#endif

	return  1 ;
}

int
kik_alloca_end_stack_frame(void)
{
	if( alloca_page == NULL)
	{
		/* no page is found */

		return  0 ;
	}

	if( current_stack_frame == 0)
	{
	#ifdef  __DEBUG
		fprintf( stderr , "beg of stack frame.\n") ;
	#endif
	
		return  1 ;
	}
#ifdef  DEBUG
	else if( current_stack_frame < 0)
	{
		fprintf( stderr , "current stack frame must not be less than 0.\n") ;
	
		return  1 ;
	}
#endif
	else if( current_stack_frame < MAX_STACK_FRAMES)
	{
		size_t  shrink_size ;
		alloca_page_t *  page ;
		
	#ifdef  __DEBUG
		fprintf( stderr , "stack frame (base %d (%d) size %d) deleted\n" ,
			stack_frame_bases[current_stack_frame] ,
			current_stack_frame ,
			total_allocated_size - stack_frame_bases[current_stack_frame]) ;
	#endif
	
		shrink_size = total_allocated_size - stack_frame_bases[current_stack_frame] ;

		page = alloca_page ;

		while( shrink_size > page->used_size)
		{
			alloca_page_t *  _page ;

			if( ( _page = page->prev_page) == NULL)
			{
			#ifdef  DEBUG
				fprintf( stderr , "strange page link list...\n") ;
			#endif

				/* so as not to seg fault ... */
				shrink_size = page->used_size ;

				break ;
			}
			
			shrink_size -= page->used_size ;
			
		#ifdef  __DEBUG
			fprintf( stderr , "page(size %d) deleted.\n" , page->size) ;
		#endif
			
			free( page->ptr) ;
			free( page) ;

			page = _page ;
		}
		
		alloca_page = page ;

		alloca_page->used_size -= shrink_size ;
		
		total_allocated_size = stack_frame_bases[current_stack_frame] ;
	}

	current_stack_frame -- ;
	
	return  1 ;
}

int
kik_alloca_garbage_collect(void)
{
	alloca_page_t *  page ;
	
#ifdef  __DEBUG
	fprintf( stderr , "allocated memory(size %d) for alloca() is discarded.\n" , total_allocated_size) ;
#endif

	page = alloca_page ;

	while( page)
	{
		alloca_page_t *  _page ;
		
		_page = page->prev_page ;
		
		free( page->ptr) ;
		free( page) ;

		page = _page ;
	}
	
	alloca_page = NULL ;
	
	total_allocated_size = 0 ;

	return  1 ;
}

#endif
