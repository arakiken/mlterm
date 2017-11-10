/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_gdiobj_pool.h"

#include <pobl/bl_mem.h>

typedef struct stock_pen {
  HPEN pen;
  u_long rgb;
  int ref_count;

} stock_pen_t;

typedef struct stock_brush {
  HBRUSH brush;
  u_long rgb;
  int ref_count;

} stock_brush_t;

/* --- static variables --- */

static stock_pen_t *stock_pens;
static u_int num_stock_pens;
static stock_brush_t *stock_brushes;
static u_int num_stock_brushes;

/* --- static functions --- */

static int garbage_unused_objects(void) {
  int count;

  for (count = 0; count < num_stock_pens;) {
    if (stock_pens[count].ref_count <= 0) {
      DeleteObject(stock_pens[count].pen);
      stock_pens[count] = stock_pens[--num_stock_pens];
    } else {
      count++;
    }
  }

  for (count = 0; count < num_stock_brushes;) {
    if (stock_brushes[count].ref_count <= 0) {
      DeleteObject(stock_brushes[count].brush);
      stock_brushes[count] = stock_brushes[--num_stock_brushes];
    } else {
      count++;
    }
  }

  return 1;
}

/* --- global functions --- */

int ui_gdiobj_pool_init(void) { return 1; }

int ui_gdiobj_pool_final(void) {
  u_int count;

  for (count = 0; count < num_stock_pens; count++) {
    DeleteObject(stock_pens[count].pen);
  }

  for (count = 0; count < num_stock_brushes; count++) {
    DeleteObject(stock_brushes[count].brush);
  }

  free(stock_pens);
  free(stock_brushes);

  return 1;
}

HPEN ui_acquire_pen(u_long rgb) {
  u_int count;

  /* Remove alpha */
  rgb &= 0xffffff;

  for (count = 0; count < num_stock_pens; count++) {
    if (rgb == stock_pens[count].rgb) {
      stock_pens[count].ref_count++;

      return stock_pens[count].pen;
    }
  }

  if (rgb == RGB(0, 0, 0)) {
    return GetStockObject(BLACK_PEN);
  } else if (rgb == RGB(0xff, 0xff, 0xff)) {
    return GetStockObject(WHITE_PEN);
  } else {
    void *p;

    if (num_stock_pens % 10 == 9) {
      garbage_unused_objects();
    }

    if ((p = realloc(stock_pens, sizeof(stock_pen_t) * (num_stock_pens + 1))) == NULL) {
      return None;
    }

    stock_pens = p;

    stock_pens[num_stock_pens].rgb = rgb;
    stock_pens[num_stock_pens].pen = CreatePen(PS_SOLID, 1, rgb);
    stock_pens[num_stock_pens].ref_count = 1;

    return stock_pens[num_stock_pens++].pen;
  }
}

int ui_release_pen(HPEN pen) {
  u_int count;

  for (count = 0; count < num_stock_pens; count++) {
    if (pen == stock_pens[count].pen) {
      --stock_pens[count].ref_count;

      return 1;
    }
  }

  return 0;
}

HBRUSH
ui_acquire_brush(u_long rgb) {
  u_int count;

  /* Remove alpha */
  rgb &= 0xffffff;

  for (count = 0; count < num_stock_brushes; count++) {
    if (rgb == stock_brushes[count].rgb) {
      stock_brushes[count].ref_count++;

      return stock_brushes[count].brush;
    }
  }

  if (rgb == RGB(0, 0, 0)) {
    return GetStockObject(BLACK_BRUSH);
  } else if (rgb == RGB(0xff, 0xff, 0xff)) {
    return GetStockObject(WHITE_BRUSH);
  } else {
    void *p;

    if (num_stock_brushes % 10 == 9) {
      garbage_unused_objects();
    }

    if ((p = realloc(stock_brushes, sizeof(stock_brush_t) * (num_stock_brushes + 1))) == NULL) {
      return None;
    }

    stock_brushes = p;

    stock_brushes[num_stock_brushes].rgb = rgb;
    stock_brushes[num_stock_brushes].brush = CreateSolidBrush(rgb);
    stock_brushes[num_stock_brushes].ref_count = 1;

    return stock_brushes[num_stock_brushes++].brush;
  }
}

int ui_release_brush(HBRUSH brush) {
  u_int count;

  for (count = 0; count < num_stock_brushes; count++) {
    if (brush == stock_brushes[count].brush) {
      --stock_brushes[count].ref_count;

      return 1;
    }
  }

  return 0;
}
