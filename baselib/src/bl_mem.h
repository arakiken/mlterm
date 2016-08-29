/*
 *	$Id$
 */

#ifndef __BL_MEM_H__
#define __BL_MEM_H__

#include <stdlib.h>

#include "bl_types.h" /* size_t */
#include "bl_def.h"

#if defined(BL_DEBUG)

#define malloc(size) bl_mem_malloc(size, __FILE__, __LINE__, __FUNCTION__)

#define calloc(number, size) bl_mem_calloc(number, size, __FILE__, __LINE__, __FUNCTION__)

#define realloc(ptr, size) bl_mem_realloc(ptr, size, __FILE__, __LINE__, __FUNCTION__)

#define free(ptr) bl_mem_free(ptr, __FILE__, __LINE__, __FUNCTION__)

#elif !defined(CALLOC_CHECK_OVERFLOW)

/*
 * In some environment (where CALLOC_CHECK_OVERFLOW is not defined by configure
 * script),
 * calloc doesn't check if number*size is over sizeof(size_t).
 */
#define calloc(number, size) bl_mem_calloc(number, size, NULL, 0, NULL)

#endif

void* bl_mem_malloc(size_t size, const char* file, int line, const char* func);

void* bl_mem_calloc(size_t number, size_t size, const char* file, int line, const char* func);

void* bl_mem_realloc(void* ptr, size_t size, const char* file, int line, const char* func);

void bl_mem_remove(void* ptr, const char* file, int line, const char* func);

void bl_mem_free(void* ptr, const char* file, int line, const char* func);

#ifdef BL_DEBUG

void bl_mem_dump_all(void);

int bl_mem_free_all(void);

#else

#define bl_mem_free_all()

#endif

#ifndef HAVE_ALLOCA

#undef alloca
#ifdef BL_DEBUG
#include <string.h> /* memset */
#define alloca(size) memset(bl_alloca(size), 0xff, size)
#else
#define alloca(size) bl_alloca(size)
#endif

void* bl_alloca(size_t size);

int bl_alloca_begin_stack_frame(void);

int bl_alloca_end_stack_frame(void);

int bl_alloca_garbage_collect(void);

#else /* HAVE_ALLOCA */

#define bl_alloca_begin_stack_frame() 1
#define bl_alloca_end_stack_frame() 1
#define bl_alloca_garbage_collect() 1

/* If glib/galloca.h has been already included, following hack is disabled. */
#ifndef __G_ALLOCA_H__

/* AIX requires this to be the first thing in the file.  */
#ifndef __GNUC__

#ifdef HAVE_ALLOCA_H

#include <alloca.h>

#else /* HAVE_ALLOCA_H */

#ifdef _AIX

#pragma alloca

#else /* _AIX */

/* predefined by HP cc +Olibcalls */
#ifndef alloca
char* alloca();
#endif

#endif /* _AIX */

#endif /* HAVE_ALLOCA_H */

#else /* __GNUC__ */

#ifdef BL_DEBUG

/* This debug hack can be available in GNUC alone. */
#undef alloca
#include <string.h> /* memset */
#define alloca(size) memset(alloca(size), 0xff, size)

#else /* BL_DEBUG */

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif /* HAVE_ALLOCA_H */

#endif /* BL_DEBUG */

#endif /* __GNUC__ */

#endif /* __G_ALLOCA_H__ */

#endif /* HAVE_ALLOCA */

#endif
