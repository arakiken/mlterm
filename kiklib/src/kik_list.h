/*
 *	$Id$
 */

#ifndef  __KIK_LIST_H__
#define  __KIK_LIST_H__


#include  "kik_debug.h"
#include  "kik_iterator.h"
#include  "kik_mem.h"


/*
 * only for debug.
 */
#if  0

#define  kik_list_dump( list) kik_debug_printf( "%p->...->%p\n" , (list)->first , (list)->last)

#else

#define  kik_list_dump( list)

#endif


/*
 * list type.
 */
 
#define  KIK_LIST( type) __ ## type ## _list_t

#define  KIK_LIST_TYPEDEF( type) \
KIK_ITERATOR_TYPEDEF( type) ; \
\
typedef struct  __ ## type ## _list \
{ \
	KIK_ITERATOR( type)  first ; \
	KIK_ITERATOR( type)  last ; \
} *  __ ## type ## _list_t


/*
 * followings are the list operation macros.
 */
 
#define  kik_list_new( type , list) \
{ \
	if( ( list = malloc( sizeof( *(list)))) == NULL) \
	{ \
		kik_error_printf( "malloc() failed in kik_list_new().\n") ; \
		abort() ; \
	} \
	(list)->first = NULL ; \
	(list)->last = NULL ; \
}

#define  kik_list_delete( type , target_list) \
{ \
	KIK_ITERATOR(type)   iterator = NULL ; \
	\
	for( iterator = (target_list)->first ; iterator ; iterator = iterator->next) \
	{ \
		if( iterator->prev != NULL) \
		{ \
			kik_iterator_dump( iterator->prev) ; \
			free( iterator->prev) ; \
		} \
	} \
	kik_iterator_dump( (target_list)->last) ; \
	free( (target_list)->last) ; \
	free( target_list) ; \
}

#define  kik_list_insert_after( type , target_list , iterator , new_object)  \
{ \
	KIK_ITERATOR( type)   new_iterator = NULL ; \
	\
	if( ( new_iterator = malloc( sizeof( KIK_ITERATOR( type)))) == NULL) \
	{ \
		kik_error_printf( "malloc() failed in kik_list_insert_after().\n") ; \
		abort() ; \
	} \
	new_iterator->object = new_object ; \
	\
	if( ( new_iterator->next = (iterator)->next) != NULL) \
	{ \
		(iterator)->next->prev = new_iterator ; \
	} \
	else \
	{ \
		(target_list)->last = new_iterator ; \
		new_iterator->next = NULL ; \
	} \
	(iterator)->next = new_iterator ; \
	new_iterator->prev = (iterator) ; \
}

#define  kik_list_insert_before( type , target_list , iterator , new_object) \
{ \
	KIK_ITERATOR( type)   new_iterator = NULL ; \
	\
	if( ( new_iterator = malloc( sizeof( KIK_ITERATOR( type)))) == NULL) \
	{ \
		kik_error_printf( "malloc() failed in kik_list_insert_before().\n") ; \
		abort() ; \
	} \
	new_iterator->object = new_object ; \
	\
	if( ( new_iterator->prev = (iterator)->prev) != NULL) \
	{ \
		(iterator)->prev->next = new_iterator ; \
	} \
	else \
	{ \
		(target_list)->first = new_iterator ; \
		new_iterator->prev = NULL ; \
	} \
	(iterator)->prev = new_iterator ; \
	new_iterator->next = (iterator) ; \
}

#define  kik_list_insert_head( type , target_list , new_object) \
{ \
	KIK_ITERATOR( type)   new_iterator = NULL ; \
	\
	if( ( new_iterator = malloc( sizeof( KIK_ITERATOR( type)))) == NULL) \
	{ \
		kik_error_printf( "malloc() failed in kik_list_insert_head().\n") ; \
		abort() ; \
	} \
	new_iterator->object = new_object ; \
	\
	if( ( new_iterator->next = (target_list)->first) != NULL) \
	{ \
		(target_list)->first->prev = new_iterator ; \
	} \
	else \
	{ \
		(target_list)->last = new_iterator ; \
	} \
	(target_list)->first = new_iterator ; \
	new_iterator->prev = NULL ; \
}

#define  kik_list_insert_tail( type , target_list , new_object) \
{ \
	KIK_ITERATOR( type)   new_iterator = NULL ; \
	\
	if( ( new_iterator = malloc( sizeof( KIK_ITERATOR( type)))) == NULL) \
	{ \
		kik_error_printf( "malloc() failed in kik_list_insert_tail().\n") ; \
		abort() ; \
	} \
	new_iterator->object = new_object ; \
	\
	if( ( new_iterator->prev = (target_list)->last) != NULL) \
	{ \
		(target_list)->last->next = new_iterator ; \
	} \
	else \
	{ \
		(target_list)->first = new_iterator ; \
	} \
	(target_list)->last = new_iterator ; \
	new_iterator->next = NULL ; \
}

#define  kik_list_append_another_list( type , target_list , appended_list) \
{ \
	KIK_ITERATOR( type)   iterator = NULL ; \
	\
	iterator = (appended_list)->first ; \
	while( iterator) \
	{ \
		kik_list_insert_tail( type , target_list , iterator->object) ; \
		iterator = iterator->next ; \
	} \
}

#define  kik_list_remove( type , target_list , iterator) \
{ \
	KIK_ITERATOR( type)  next_p = (iterator)->next ; \
	KIK_ITERATOR( type)  prev_p = (iterator)->prev ; \
	kik_iterator_dump( iterator) ; \
	\
	if( (target_list)->first == (iterator) && (target_list)->last == (iterator)) \
	{ \
		(target_list)->first = NULL ; \
		(target_list)->last = NULL ; \
	} \
	else if( (target_list)->first == (iterator)) \
	{ \
		(target_list)->first = next_p ; \
		(target_list)->first->prev = NULL ; \
	} \
	else if( (target_list)->last == (iterator)) \
	{ \
		(target_list)->last = prev_p ; \
		(target_list)->last->next = NULL ; \
	} \
	else \
	{ \
		if( next_p != NULL) \
		{ \
			next_p->prev = prev_p ; \
		} \
		if( prev_p != NULL) \
		{ \
			prev_p->next = next_p ; \
		} \
	} \
	\
	free( iterator) ; \
}

#define  kik_list_search_and_remove( type , target_list , target_object) \
{ \
	KIK_ITERATOR( type)   iterator = NULL ; \
	for( iterator = (target_list)->first ; iterator ; iterator = iterator->next) \
	{ \
		if( iterator->object == target_object) \
		{ \
			kik_list_remove( type , target_list , iterator) ; \
			break ; \
		} \
	} \
}

#define  kik_list_first( target_list)  ((target_list)->first)

#define  kik_list_last( target_list)  ((target_list)->last)

#define  kik_list_is_empty( target_list)  ((target_list)->first == NULL && (target_list)->last == NULL)

#endif
