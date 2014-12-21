/*
 *	$Id$
 */

#ifndef  __KIK_SLIST_H__
#define  __KIK_SLIST_H__


#define  kik_slist_insert_head( list , new) \
	{ \
		(new)->next = (list) ; \
		(list) = (new) ; \
	}

#define  kik_slist_remove( list , target) \
	{ \
		if( (list) == (target)) \
		{ \
			(list) = (list)->next ; \
		} \
		else if( list) \
		{ \
			void *  orig = (list) ; \
			while( (list)->next) \
			{ \
				if( (list)->next == (target)) \
				{ \
					(list)->next = (target)->next ; \
					break ; \
				} \
				else \
				{ \
					(list) = (list)->next ; \
				} \
			} \
			(list) = orig ; \
		} \
	}

#define  kik_slist_next( list)  ((list) ? (list)->next : NULL)

#define  kik_slist_is_empty( list)  ((list) == NULL)


#endif
