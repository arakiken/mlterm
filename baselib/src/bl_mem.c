/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
 * !! Notice !!
 * don't use other bl_xxx functions(macros may be ok) as much as possible since
 * they may use
 * these memory management functions.
 */

#include "bl_mem.h"

#include <stdio.h>  /* fprintf */
#include <string.h> /* memset */

#include "bl_slist.h"

#if 0
#define __DEBUG
#endif

#ifndef HAVE_ALLOCA

#define ALLOCA_PAGE_UNIT_SIZE 4096
#define MAX_STACK_FRAMES 10

typedef struct alloca_page {
  void *ptr;
  size_t size;
  size_t used_size;

  struct alloca_page *prev_page;

} alloca_page_t;

/* --- static variables --- */

static size_t total_allocated_size = 0;
static alloca_page_t *alloca_page;
static int stack_frame_bases[MAX_STACK_FRAMES];
static int current_stack_frame = 0;

/* --- global functions --- */

void *bl_alloca(size_t size) {
  void *ptr;

  /* arranging bytes boundary */
  size = ((size + sizeof(char *) - 1) / sizeof(char *)) * sizeof(char *);

  if (alloca_page == NULL || alloca_page->used_size + size > alloca_page->size) {
    alloca_page_t *new_page;

    if ((new_page = malloc(sizeof(alloca_page_t))) == NULL) {
      return NULL;
    }

    new_page->size = ((size / ALLOCA_PAGE_UNIT_SIZE) + 1) * ALLOCA_PAGE_UNIT_SIZE;
    new_page->used_size = 0;

    if ((new_page->ptr = malloc(new_page->size)) == NULL) {
      free(new_page);

      return NULL;
    }

#ifdef DEBUG
    memset(new_page->ptr, 0xff, new_page->size);
#endif

#ifdef __DEBUG
    fprintf(stderr, "new page(size %d) created.\n", new_page->size);
#endif

    new_page->prev_page = alloca_page;
    alloca_page = new_page;
  }

  /* operations of void * cannot be done in some operating systems */
  ptr = (char *)alloca_page->ptr + alloca_page->used_size;

  alloca_page->used_size += size;
  total_allocated_size += size;

#ifdef __DEBUG
  fprintf(stderr, "memory %p(size %d) was successfully allocated.\n", ptr, size);
#endif

  return ptr;
}

int bl_alloca_begin_stack_frame(void) {
  if (alloca_page == NULL) {
    /* no page is found */

    return 0;
  }

  current_stack_frame++;

  if (current_stack_frame >= MAX_STACK_FRAMES) {
#ifdef __DEBUG
    fprintf(stderr, "end of stack frame.\n");
#endif

    return 1;
  }
#ifdef DEBUG
  else if (current_stack_frame < 0) {
    fprintf(stderr, "current stack frame must not be less than 0.\n");

    return 1;
  }
#endif

  stack_frame_bases[current_stack_frame] = total_allocated_size;

#ifdef __DEBUG
  fprintf(stderr, "stack frame (base %d (%d) size 0) created.\n",
          stack_frame_bases[current_stack_frame], current_stack_frame);
#endif

  return 1;
}

int bl_alloca_end_stack_frame(void) {
  if (alloca_page == NULL) {
    /* no page is found */

    return 0;
  }

  if (current_stack_frame == 0) {
#ifdef __DEBUG
    fprintf(stderr, "beg of stack frame.\n");
#endif

    return 1;
  }
#ifdef DEBUG
  else if (current_stack_frame < 0) {
    fprintf(stderr, "current stack frame must not be less than 0.\n");

    return 1;
  }
#endif
  else if (current_stack_frame < MAX_STACK_FRAMES) {
    size_t shrink_size;
    alloca_page_t *page;

#ifdef __DEBUG
    fprintf(stderr, "stack frame (base %d (%d) size %d) deleted\n",
            stack_frame_bases[current_stack_frame], current_stack_frame,
            total_allocated_size - stack_frame_bases[current_stack_frame]);
#endif

    shrink_size = total_allocated_size - stack_frame_bases[current_stack_frame];

    page = alloca_page;

    while (shrink_size > page->used_size) {
      alloca_page_t *_page;

      if ((_page = page->prev_page) == NULL) {
#ifdef DEBUG
        fprintf(stderr, "strange page link list...\n");
#endif

        /* so as not to seg fault ... */
        shrink_size = page->used_size;

        break;
      }

      shrink_size -= page->used_size;

#ifdef __DEBUG
      fprintf(stderr, "page(size %d) deleted.\n", page->size);
#endif

      free(page->ptr);
      free(page);

      page = _page;
    }

    alloca_page = page;

    alloca_page->used_size -= shrink_size;

    total_allocated_size = stack_frame_bases[current_stack_frame];
  }

  current_stack_frame--;

  return 1;
}

int bl_alloca_garbage_collect(void) {
  alloca_page_t *page;

#ifdef __DEBUG
  fprintf(stderr, "allocated memory(size %d) for alloca() is discarded.\n", total_allocated_size);
#endif

  page = alloca_page;

  while (page) {
    alloca_page_t *_page;

    _page = page->prev_page;

    free(page->ptr);
    free(page);

    page = _page;
  }

  alloca_page = NULL;

  total_allocated_size = 0;
  current_stack_frame = 0;

  return 1;
}

#endif /* HAVE_ALLOCA */

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef bl_mem_free_all

typedef struct mem_log {
  void *ptr;

  size_t size;

  const char *file;
  int line;
  const char *func;

  struct mem_log *next;

} mem_log_t;

/* --- static variables --- */

static mem_log_t *mem_logs = NULL;

/* --- static functions --- */

static mem_log_t *search_mem_log(void *ptr) {
  mem_log_t *log;

  log = mem_logs;
  while (log) {
    if (log->ptr == ptr) {
      return log;
    }

    log = bl_slist_next(log);
  }

  return NULL;
}

/* --- global functions --- */

void *bl_mem_malloc(size_t size, const char *file, /* should be allocated memory. */
                    int line, const char *func     /* should be allocated memory. */
                    ) {
  mem_log_t *log;

  if ((log = malloc(sizeof(mem_log_t))) == NULL) {
    return NULL;
  }

  if ((log->ptr = malloc(size)) == NULL) {
    free(log);

    return NULL;
  }

  memset(log->ptr, 0xff, size);

  log->size = size;
  log->file = file;
  log->line = line;
  log->func = func;

  bl_slist_insert_head(mem_logs, log);

#ifdef __DEBUG
  fprintf(stderr,
          " memory %p(size %d) was successfully allocated at %s(l.%d in %s) , "
          "logged in %p.\n",
          log->ptr, log->size, log->func, log->line, log->file, log);
#endif

  return log->ptr;
}

void *bl_mem_calloc(size_t number, size_t size,
                    const char *file, /* should be allocated memory. If NULL, not logged. */
                    int line, const char *func /* should be allocated memory. */
                    ) {
  void *ptr;
  size_t total_size;

  total_size = number * size;
  if (number && size && !total_size) {
    /* integer overflow */
    return NULL;
  }
  if (total_size && (total_size / number != size)) {
    /* integer overflow */
    return NULL;
  }

  if (file == NULL) {
    ptr = malloc(total_size);
  } else {
    ptr = bl_mem_malloc(total_size, file, line, func);
  }

  if (ptr) {
    memset(ptr, 0, total_size);
  }

  return ptr;
}

void bl_mem_remove(void *ptr, const char *file, /* should be allocated memory. */
                   int line, const char *func   /* should be allocated memory. */
                   ) {
  if (ptr) {
    mem_log_t *log;

    if ((log = search_mem_log(ptr)) == NULL) {
#ifdef DEBUG
      fprintf(stderr, " %p is freed at %s[l.%d in %s] but not logged.\n", ptr, func, line, file);
#endif
    } else {
#ifdef __DEBUG
      fprintf(stderr,
              " %p(size %d , alloced at %s[l.%d in %s] and freed at"
              " %s[l.%d in %s] logged in %p) was successfully freed.\n",
              ptr, log->size, log->func, log->line, log->file, file, line, func, log);
#endif
      bl_slist_remove(mem_logs, log);
      memset(ptr, 0xff, log->size);
      free(log);
    }
  }
}

void bl_mem_free(void *ptr, const char *file, /* should be allocated memory. */
                 int line, const char *func   /* should be allocated memory. */
                 ) {
  bl_mem_remove(ptr, file, line, func);
  free(ptr);
}

void *bl_mem_realloc(void *ptr, size_t size, const char *file, /* should be allocated memory. */
                     int line, const char *func                /* should be allocated memory. */
                     ) {
  void *new_ptr;
  mem_log_t *log;

  if (ptr == NULL) {
    return bl_mem_malloc(size, file, line, func);
  }

  if ((log = search_mem_log(ptr)) == NULL) {
#ifdef DEBUG
    fprintf(stderr, " %p is reallocated at %s[l.%d in %s] but not logged.\n", ptr, func, line,
            file);
#endif
    return realloc(ptr, size);
  }

  /*
   * allocate new memory.
   */
  if ((new_ptr = bl_mem_malloc(size, file, line, func)) == NULL) {
    return NULL;
  }

  if (log->size < size) {
    memcpy(new_ptr, ptr, log->size);
  } else {
    memcpy(new_ptr, ptr, size);
  }

  /*
   * free old memory.
   */
  bl_mem_free(ptr, __FILE__, __LINE__, __FUNCTION__);

  return new_ptr;
}

void bl_mem_dump_all(void) {
  mem_log_t *log;

  log = mem_logs;
  while (log) {
    fprintf(stderr, "%p: ", log);
    fprintf(stderr, "%p(size %d , alloced at %s[l.%d in %s] is allocated.\n", log->ptr,
            (int)log->size, log->func, log->line, log->file);
    log = bl_slist_next(log);
  }
}

int bl_mem_free_all(void) {
  mem_log_t *log;

  if ((log = mem_logs)) {
    mem_log_t *prev;

    do {
      fprintf(stderr, "%p: ", log);
      fprintf(stderr, "%p(size %d , alloced at %s[l.%d in %s] is not freed.\n", log->ptr,
              (int)log->size, log->func, log->line, log->file);
      free(log->ptr);
      log = bl_slist_next((prev = log));
      free(prev);
    } while (log);

    mem_logs = NULL;

    return 0;
  } else {
#ifdef __DEBUG
    fprintf(stderr, "YOUR MEMORY MANAGEMENT IS PERFECT!");
#endif

    return 1;
  }
}
