/*
 *	$Id$
 */

#ifndef  __KIK_ITERATOR_H__
#define  __KIK_ITERATOR_H__


#include  "kik_debug.h"


/*
 * only for debug.
 */
#if  0

#define  kik_iterator_dump( iterator) \
kik_debug_printf( "%p->%p->%p\n" , (iterator)->prev , (iterator) , (iterator)->next)

#else

#define  kik_iterator_dump( iterator)

#endif


/*
 * iterator type.
 */
 
#define  KIK_ITERATOR( type)  __ ## type ## _iterator_t

#define  KIK_ITERATOR_TYPEDEF( type) \
typedef struct  __ ## type ## _iterator \
{ \
	type *  object ; \
	\
	struct __ ## type ## _iterator *  next ; \
	struct __ ## type ## _iterator *  prev ; \
	\
} *  __ ## type ## _iterator_t


/*
 * iterator operation.
 */
 
#define  kik_iterator_next( iterator)  ((iterator)->next)

#define  kik_iterator_prev( iterator)  ((iterator)->prev)

#define  kik_iterator_indirect( iterator)  ((iterator)->object)


#endif
