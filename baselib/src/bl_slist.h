/*
 *	$Id$
 */

#ifndef __BL_SLIST_H__
#define __BL_SLIST_H__

#define bl_slist_insert_head(list, new) \
  {                                     \
    (new)->next = (list);               \
    (list) = (new);                     \
  }

#define bl_slist_remove(list, target)    \
  {                                      \
    if ((list) == (target)) {            \
      (list) = (list)->next;             \
    } else if (list) {                   \
      void *orig = (list);               \
      while ((list)->next) {             \
        if ((list)->next == (target)) {  \
          (list)->next = (target)->next; \
          break;                         \
        } else {                         \
          (list) = (list)->next;         \
        }                                \
      }                                  \
      (list) = orig;                     \
    }                                    \
  }

#define bl_slist_next(list) ((list) ? (list)->next : NULL)

#define bl_slist_is_empty(list) ((list) == NULL)

#endif
