/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <Application.h>
#include <Window.h>
#include <Rect.h>
#include <View.h>
#include <Font.h>
#include <Bitmap.h>
#include <Clipboard.h>
#include <Screen.h>
#include <Region.h>
#include <Beep.h>
#include <TranslationUtils.h>

extern "C" {
#include "ui_window.h"
#include "beos.h"
#include "../ui_screen.h"

#include <pobl/bl_privilege.h>
#include <pobl/bl_unistd.h> /* usleep/bl_getuid/bl_getgid */
#include <pobl/bl_file.h> /* bl_file_set_cloexec */
#include <pobl/bl_debug.h>
#include <sys/select.h>
#include <mef/ef_utf16_parser.h>
#include <mef/ef_utf8_conv.h>
}

class MLWindow : public BWindow {
public:
  MLWindow(BRect frame, const char *title, window_type type, uint32 flags);
  virtual void WindowActivated(bool active);
  virtual void FrameResized(float width, float height);
  virtual void Quit();
  ui_window_t *uiwindow;
};

class MLView : public BView {
private:
  int32 buttons;
  const BFont *cur_font;

public:
  MLView(BRect frame, const char *name, uint32 resizemode, uint32 flags);
  virtual void Draw(BRect updaterect);
  virtual void KeyDown(const char *bytes, int32 numBytes);
  virtual void MouseDown(BPoint where);
  virtual void MouseMoved(BPoint where, uint32 code, const BMessage *dragMessage);
  virtual void MouseUp(BPoint where);
  virtual void SetFont(const BFont *font, uint32 mask = B_FONT_ALL);
  ui_window_t *uiwindow;
};

/* --- static variables --- */

static ef_parser_t *utf16_parser;
static ef_conv_t *utf8_conv;

/* --- static functions --- */

static void set_input_focus(MLView *view) {
  view->MakeFocus(true);

  XEvent ev;

  ev.type = UI_KEY_FOCUS_IN;
  ui_window_receive_event(view->uiwindow, &ev);
}

/* --- class methods --- */

MLWindow::MLWindow(BRect frame, const char *title, window_type type, uint32 flags)
  : BWindow(frame, title, type, flags) {
  frame.OffsetTo(0, 0);
}

void MLWindow::WindowActivated(bool active) {
  if (!uiwindow) {
    /* WindowActivated() can be called by BWindow::Quit() in window_dealloc() */
    return;
  }

  XEvent ev;

  if (active) {
    ev.type = UI_FOCUS_IN;
  } else {
    ev.type = UI_FOCUS_OUT;
  }

  if (LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ui_window_receive_event(ui_get_root_window(uiwindow), &ev);

  UnlockLooper();
}

void MLWindow::FrameResized(float width, float height) {
  if (LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  uiwindow->width = width - uiwindow->hmargin * 2;
  uiwindow->height = height - uiwindow->vmargin * 2;

  (*uiwindow->window_resized)(uiwindow);

  UnlockLooper();
}

void MLWindow::Quit() {
  if (LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  if (uiwindow) {
    if (uiwindow->disp->num_roots == 1) {
      be_app->PostMessage(B_QUIT_REQUESTED);
    }

    XEvent ev;

    ev.type = UI_CLOSE_WINDOW;
    ui_window_receive_event(uiwindow, &ev);
  } else {
    /* Called from window_dealloc() */

    BWindow::Quit();
    /* Not reach. This thread exits in BWindow::Quit(). */
  }
}

MLView::MLView(BRect frame, const char *title, uint32 resizemode, uint32 flags)
  : BView(frame, title, resizemode, flags) {
  SetViewColor(255, 255, 255);
  SetHighColor(0, 0, 0);
  SetLowColor(0, 0, 0);
  SetFontSize(14);
  SetDrawingMode(B_OP_OVER);
}

void MLView::Draw(BRect update) {
  if (!uiwindow ||
      (uiwindow->xim_listener /* is ui_screen_t */ && !((ui_screen_t *)uiwindow)->term)) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  XExposeEvent ev;

  ev.type = UI_EXPOSE;
  ev.x = update.left;
  ev.y = update.top;
  ev.width = update.right - update.left + 1.0;
  ev.height = update.bottom - update.top + 1.0;
  ev.force_expose = 1;

  if (LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ui_window_receive_event(uiwindow, (XEvent *)&ev);
  UnlockLooper();
  uiwindow->update_window_flag = 0;
}

void MLView::KeyDown(const char *bytes, int32 numBytes) {
  if (LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  int32 modifiers;

  this->Window()->CurrentMessage()->FindInt32((const char*)"modifiers", &modifiers);

  XKeyEvent kev;

  kev.type = UI_KEY_PRESS;
  kev.state = modifiers & (ShiftMask|ControlMask|Mod1Mask|CommandMask);

  if (numBytes == 1) {
    if (bytes[0] == B_FUNCTION_KEY) {
      int32 key;

      this->Window()->CurrentMessage()->FindInt32((const char*)"key", &key);
      kev.keysym = (key | 0xf000);
      kev.utf8 = NULL;
    } else {
      kev.keysym = *bytes;

      switch(kev.keysym) {
      case B_LEFT_ARROW:
      case B_RIGHT_ARROW:
      case B_UP_ARROW:
      case B_DOWN_ARROW:
      case B_INSERT:
      case B_DELETE:
      case B_HOME:
      case B_END:
      case B_PAGE_UP:
      case B_PAGE_DOWN:
      case B_KATAKANA_HIRAGANA:
      case B_HANKAKU_ZENKAKU:
      case B_HANGUL:
      case B_HANGUL_HANJA:
        kev.utf8 = NULL;
        break;
      default:
        kev.utf8 = bytes;
        break;
      }
    }
  } else {
    kev.keysym = 0;
    kev.utf8 = bytes;
  }

  ui_window_receive_event(uiwindow, (XEvent *)&kev);

  UnlockLooper();
}

void MLView::MouseDown(BPoint where) {
  if (LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  if (!IsFocus()) {
    set_input_focus(this);
  }

  int32 clicks;
  int64 when; /* microsec */
  int32 modifiers;

  this->Window()->CurrentMessage()->FindInt32((const char*)"buttons", &this->buttons);
  this->Window()->CurrentMessage()->FindInt32((const char*)"clicks", &clicks);
  this->Window()->CurrentMessage()->FindInt64((const char*)"when", &when);
  this->Window()->CurrentMessage()->FindInt32((const char*)"modifiers", &modifiers);

  XButtonEvent bev;

  bev.type = UI_BUTTON_PRESS;
  bev.time = when / 1000; /* msec */
  bev.x = where.x;
  bev.y = where.y;
  bev.state = modifiers & (ShiftMask|ControlMask|Mod1Mask|CommandMask);
  bev.click_count = clicks;
  if (buttons == 0) {
    bev.button = 0;
  } else {
    if (buttons & B_PRIMARY_MOUSE_BUTTON) {
      bev.button = 1;
    } else if (buttons & B_SECONDARY_MOUSE_BUTTON) {
      bev.button = 3;
    } else /* if (buttons & B_TERTIARY_MOUSE_BUTTON) */ {
      bev.button = 2;
    }
  }

  ui_window_receive_event(uiwindow, (XEvent *)&bev);

  UnlockLooper();
}

void MLView::MouseMoved(BPoint where, uint32 code, const BMessage *dragMessage) {
  if (LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  int32 buttons;
  int64 when; /* microsec */
  int32 modifiers;

  this->Window()->CurrentMessage()->FindInt32((const char*)"buttons", &buttons);
  this->Window()->CurrentMessage()->FindInt64((const char*)"when", &when);
  this->Window()->CurrentMessage()->FindInt32((const char*)"modifiers", &modifiers);

  XMotionEvent mev;

  mev.time = when / 1000; /* msec */
  mev.x = where.x;
  mev.y = where.y;
  mev.state = modifiers & (ShiftMask|ControlMask|Mod1Mask|CommandMask);
  if (buttons == 0) {
    mev.type = UI_POINTER_MOTION;
  } else {
    if (buttons & B_PRIMARY_MOUSE_BUTTON) {
      mev.state |= Button1Mask;
    }
    if (buttons & B_SECONDARY_MOUSE_BUTTON) {
      mev.state |= Button3Mask;
    }
    if (buttons & B_TERTIARY_MOUSE_BUTTON) {
      mev.state |= Button2Mask;
    }
    mev.type = UI_BUTTON_MOTION;
  }

  ui_window_receive_event(uiwindow, (XEvent *)&mev);

  UnlockLooper();
}

void MLView::MouseUp(BPoint where) {
  if (LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  int32 clicks;
  int64 when; /* microsec */

  this->Window()->CurrentMessage()->FindInt32((const char*)"clicks", &clicks);
  this->Window()->CurrentMessage()->FindInt64((const char*)"when", &when);

  XButtonEvent bev;

  bev.type = UI_BUTTON_RELEASE;
  bev.time = when / 1000; /* msec */
  bev.x = where.x;
  bev.y = where.y;
  bev.state = 0;
  if (buttons == 0) {
    bev.button = 0;
  } else {
    if (buttons & B_PRIMARY_MOUSE_BUTTON) {
      bev.button = 1;
    } else if (buttons & B_SECONDARY_MOUSE_BUTTON) {
      bev.button = 3;
    } else /* if (buttons & B_TERTIARY_MOUSE_BUTTON) */ {
      bev.button = 2;
    }
  }
  this->buttons = 0;

  ui_window_receive_event(uiwindow, (XEvent *)&bev);

  UnlockLooper();
}

/*
 * SetFont() doesn't check if the specified font is different from the current font,
 * while SetHighColor() checks if the specified RGB is different from the current RGB.
 * (https://github.com/haiku/haiku/blob/master/src/kits/interface/View.cpp)
 */
void MLView::SetFont(const BFont *font, uint32 mask) {
  if (cur_font != font) {
    cur_font = font;
    BView::SetFont(font, mask);
  }
}

/* --- global functions --- */

void view_alloc(ui_window_t *uiwindow, int x, int y, u_int width, u_int height) {
  MLView *view = new MLView(BRect(x, y, x + width, y + height), "mlterm",
                            B_FOLLOW_NONE, B_WILL_DRAW /*|B_INPUT_METHOD_AWARE*/);

  MLWindow *win = (MLWindow*)uiwindow->parent->my_window;
  if (win->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    delete view;

    return;
  }

  view->uiwindow = uiwindow;
  win->AddChild(view);
  uiwindow->my_window = (void*)view;

  if (uiwindow->inputtable) {
    view->MakeFocus(true);
  }

  win->UnlockLooper();
}

void view_dealloc(/* BView */ void *view) {
  BWindow *win = ((BView*)view)->Window();

  if (win->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((MLView*)view)->uiwindow = None;
  win->RemoveChild((BView*)view);
  delete (BView*)view;

  win->UnlockLooper();
}

void view_update(/* MLView */ void *view) {
  ui_window_t *uiwindow = ((MLView*)view)->uiwindow;
  int x;
  int y;

  if (!uiwindow->xim_listener ||
      !(*uiwindow->xim_listener->get_spot)(uiwindow->xim_listener->self, &x, &y)) {
    x = y = 0;
  }

  x += (uiwindow->hmargin);
  y += (uiwindow->vmargin);

  ((MLView*)view)->Draw(BRect(x, y, x + 1, y + 1));
}

void view_set_clip(/* MLView */ void *view, int x, int y, u_int width, u_int height) {
  BRegion *region = new BRegion(BRect(x, y, x + width, y + height));
  ((MLView*)view)->ConstrainClippingRegion(region);
}

void view_unset_clip(/* MLView */ void *view) {
  ((MLView*)view)->ConstrainClippingRegion(NULL);
}

/* Not lock because this function is called from MLView::Draw() */
void view_draw_string(/* MLView */ void *view, ui_font_t *font, ui_color_t *fg_color,
                      int x, int y, char *str, size_t len) {
  u_int32_t pixel = fg_color->pixel;
  ((MLView*)view)->SetHighColor((pixel >> 16) & 0xff, (pixel >> 8) & 0xff, pixel & 0xff,
                                (pixel >> 24) & 0xff);
  ((MLView*)view)->SetFont((BFont*)font->xfont->fid);
  ((MLView*)view)->MovePenTo(x, y);
  ((MLView*)view)->DrawString(str, len);

  if (font->double_draw_gap) {
    ((MLView*)view)->MovePenTo(x + font->double_draw_gap, y);
    ((MLView*)view)->DrawString(str, len);
  }
}

/* Not lock because this function is called from MLView::Draw() */
void view_draw_string16(/* MLView */ void *view, ui_font_t *font, ui_color_t *fg_color,
                        int x, int y, XChar2b *str, size_t len) {
  if (!utf16_parser) {
    /* XXX leaked */
    utf16_parser = ef_utf16le_parser_new();
    utf8_conv = ef_utf8_conv_new();
  }

  char str2[len * UTF_MAX_SIZE];

  (*utf16_parser->init)(utf16_parser);
  (*utf16_parser->set_str)(utf16_parser, (u_char*)str, len * 2);
  (*utf8_conv->init)(utf8_conv);
  len = (*utf8_conv->convert)(utf8_conv, (u_char*)str2, len * UTF_MAX_SIZE, utf16_parser);

  view_draw_string(view, font, fg_color, x, y, str2, len);
}

/* Not lock because this function is called from MLView::Draw() */
void view_fill_with(/* MLView */ void *view, ui_color_t *color, int x, int y,
                    u_int width /* > 0 */, u_int height /* > 0 */) {
  u_long pixel = color->pixel;
  ((MLView*)view)->SetLowColor((pixel >> 16) & 0xff, (pixel >> 8) & 0xff, pixel & 0xff,
                               (pixel >> 24) & 0xff);
  ((MLView*)view)->FillRect(BRect(x, y, x + width - 1, y + height - 1), B_SOLID_LOW);
}

/* Not lock because this function is called from MLView::Draw() */
void view_draw_rect_frame(/* MLView */ void *view, ui_color_t *color, int x1, int y1,
                          int x2, int y2) {
  u_long pixel = color->pixel;
  rgb_color rgb;

  rgb.red = (pixel >> 16) & 0xff;
  rgb.green = (pixel >> 8) & 0xff;
  rgb.blue = pixel & 0xff;
  rgb.alpha = (pixel >> 24) & 0xff;

  ((MLView*)view)->BeginLineArray(4);
  ((MLView*)view)->AddLine(BPoint(x1, y1), BPoint(x1, y2), rgb);
  ((MLView*)view)->AddLine(BPoint(x1, y2), BPoint(x2, y2), rgb);
  ((MLView*)view)->AddLine(BPoint(x2, y1), BPoint(x2, y2), rgb);
  ((MLView*)view)->AddLine(BPoint(x1, y1), BPoint(x2, y1), rgb);
  ((MLView*)view)->EndLineArray();
}

/* Not lock because this function is called from MLView::Draw() */
void view_copy_area(/* MLView */ void *view, Pixmap src, int src_x, int src_y,
                    u_int width, u_int height, int dst_x, int dst_y) {
  ((MLView*)view)->DrawBitmap((BBitmap*)src,
                              BRect(src_x, src_y, src_x + width, src_y + height),
                              BRect(dst_x, dst_y, dst_x + width, dst_y + height));
}

void view_scroll(/* MLView */ void *view, int src_x, int src_y, u_int width,
                 u_int height, int dst_x, int dst_y) {
}

void view_bg_color_changed(/* MLView */ void *view) {
}

void view_visual_bell(/* MLView */ void *view) {
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  u_long pixel = ((MLView*)view)->uiwindow->fg_color.pixel;
  ((MLView*)view)->SetLowColor((pixel >> 16) & 0xff, (pixel >> 8) & 0xff, pixel & 0xff,
                               (pixel >> 24) & 0xff);

  ui_window_t *win = ((MLView*)view)->uiwindow;
  ((MLView*)view)->FillRect(BRect(0, 0, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win)), B_SOLID_LOW);
  ((BView*)view)->UnlockLooper();

  usleep(100000); /* 100 msec */

  ((MLView*)view)->Draw(BRect(0, 0, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win)));
}

void view_set_input_focus(/* BView */ void *view) {
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  set_input_focus((MLView*)view);

  ((BView*)view)->UnlockLooper();
}

void view_resize(/* BView */ void *view, u_int width, u_int height) {
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BView*)view)->ResizeTo((float)width, (float)height);

  ((BView*)view)->UnlockLooper();
}

void view_move(/* BView */ void *view, int x, int y) {
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BView*)view)->MoveTo((float)x, (float)y);

  ((BView*)view)->UnlockLooper();
}

void view_set_hidden(/* BView */ void *view, int flag) {
}

void view_reset_uiwindow(ui_window_t *uiwindow) {
  ((MLView*)uiwindow->my_window)->uiwindow = uiwindow;
}

void window_alloc(ui_window_t *root, u_int width, u_int height) {
#if 1
  /* XXX */
  static int count = 0;
  BRect r = BScreen().Frame();
  int x = ((r.right - width) / 8) * count + 50;
  int y = ((r.bottom - height) / 8) * count + 50;
  if (++count == 8) {
    count = 0;
  }
#endif

  MLWindow *win = new MLWindow(BRect(x, y, x + width, y + height),
                               "mlterm", B_DOCUMENT_WINDOW, 0);
  win->uiwindow = root;
  root->my_window = (void*)win;
}

void window_show(ui_window_t *root) {
  if (((MLWindow*)root->my_window)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((MLWindow*)root->my_window)->Show();

  ((MLWindow*)root->my_window)->UnlockLooper();
}

void window_dealloc(/* BWindow */ void *window) {
  ((MLWindow*)window)->uiwindow = None;
  /* LockLooperWithTimeout() is called in MLWindow::Quit() */
  ((BWindow*)window)->Quit();
}

void window_resize(/* BWindow */ void *window, int width, int height) {
  if (((BWindow*)window)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BWindow*)window)->ResizeTo((float)width, (float)height);

  ((BWindow*)window)->UnlockLooper();
}

void window_move_resize(/* BWindow */ void *window, int x, int y, int width, int height) {
  if (((BWindow*)window)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BWindow*)window)->ResizeTo((float)width, (float)height);
  ((BWindow*)window)->MoveTo((float)x, (float)y);

  ((BWindow*)window)->UnlockLooper();
}

void window_get_position(/* BWindow */ void *window, int *x, int *y) {
}

void window_set_title(/* BWindow */ void *window, const char *title /* utf8 */) {
  ((BWindow*)window)->SetTitle(title);
}

void app_urgent_bell(int on) {
}

void *beos_create_font(const char *font_family, float size, int is_italic, int is_bold) {
  BFont *bfont = new BFont();

  bfont->SetFamilyAndStyle(font_family, is_bold ? "Bold" : "Regular");
  bfont->SetSpacing(B_FIXED_SPACING);
  bfont->SetSize(size);
  if (is_italic) {
    bfont->SetShear(100.0);
  }

  return bfont;
}

#ifdef USE_OT_LAYOUT
char *beos_get_font_path(void *bfont) {
  return NULL;
}
#endif /* USE_OT_LAYOUT */

void beos_release_font(void *bfont) {
  delete (BFont*)bfont;
}

void beos_font_get_metrics(void *bfont, u_int *width, u_int *height, u_int *ascent) {
  const char *strs[] = { "M" };
  int32 lens[] = { 1 };
  float widths[1];

  ((BFont*)bfont)->GetStringWidths(strs, lens, 1, widths);
  *width = widths[0] + 0.5;

  font_height height_info;
  ((BFont*)bfont)->GetHeight(&height_info);
  *height = (height_info.ascent + height_info.descent + height_info.leading) + 0.5;
  *ascent = (height_info.ascent + 0.5);
}

u_int beos_font_get_advance(void *bfont, int size_attr,
                            unichar *utf16, u_int len, u_int32_t glyph) {
  if (!utf16_parser) {
    /* XXX leaked */
    utf16_parser = ef_utf16le_parser_new();
    utf8_conv = ef_utf8_conv_new();
  }

  char utf8[len * UTF_MAX_SIZE];

  (*utf16_parser->init)(utf16_parser);
  (*utf16_parser->set_str)(utf16_parser, (u_char*)utf16, len * 2);
  (*utf8_conv->init)(utf8_conv);
  len = (*utf8_conv->convert)(utf8_conv, (u_char*)utf8, len * UTF_MAX_SIZE, utf16_parser);

  const char *strs[] = { utf8 };
  int32 lens[] = { len };
  float widths[1];

  ((BFont*)bfont)->GetStringWidths(strs, lens, 1, widths);

  return (u_int)(widths[0] + 0.5);
}

void beos_clipboard_set(const u_char *utf8, size_t len) {
  if (be_clipboard->Lock()) {
    be_clipboard->Clear();

    BMessage *clip = be_clipboard->Data();
    clip->AddData("text/plain", B_MIME_TYPE, utf8, len);
    be_clipboard->Commit();
    be_clipboard->Unlock();
  }
}

int beos_clipboard_get(u_char **str, size_t *len) {
  if (be_clipboard->Lock()) {
    BMessage *clip = be_clipboard->Data();

    clip->FindData("text/plain", B_MIME_TYPE, (const void **)str, (ssize_t*)len);
    be_clipboard->Unlock();

    return 1;
  } else {
    return 0;
  }
}

void beos_beep(void) {
  beep();
}

void *beos_create_image(const void *data, u_int len, u_int width, u_int height) {
  BBitmap *bitmap = new BBitmap(BRect(0, 0, width - 1, height), 0, B_RGBA32);
  bitmap->SetBits(data, len, 0, B_RGBA32);

  return bitmap;
}

void beos_destroy_image(void *bitmap) {
  delete (BBitmap*)bitmap;
}

void *beos_load_image(const char *path, u_int *width, u_int *height) {
  BBitmap *bitmap = BTranslationUtils::GetBitmap(path);
  BRect rect = bitmap->Bounds();
  *width = rect.right;
  *height = rect.bottom;

  return bitmap;
}

void *beos_resize_image(void *bitmap, u_int width, u_int height) {
  /* XXX Not implemented yet. */
  return bitmap;
}

char *beos_dialog_password(const char *msg) {
  return NULL;
}

int beos_dialog_okcancel(const char *msg) {
  return 0;
}

int beos_dialog_alert(const char *msg) {
  return 0;
}
