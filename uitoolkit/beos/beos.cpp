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
#include <Input.h>
#include <Alert.h>
#include <TextControl.h>
#include <LayoutBuilder.h>
#include <Button.h>
#include <Entry.h>
#include <Path.h>

#include <pthread.h>

extern "C" {
#include "ui_window.h"
#include "beos.h"
#include "../ui_screen.h"
#include "../ui_selection_encoding.h"

#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_privilege.h>
#include <pobl/bl_unistd.h> /* usleep/bl_getuid/bl_getgid */
#include <pobl/bl_file.h> /* bl_file_set_cloexec */
#include <pobl/bl_debug.h>
#include <sys/select.h>
#include <mef/ef_utf16_parser.h>
}

#define IS_UISCREEN(win) ((win)->xim_listener)

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
  virtual void MessageReceived(BMessage *msg);
  ui_window_t *uiwindow;
  BMessenger *messenger;
};

class ConnectDialog : public BWindow {
private:
  int button;

public:
  ConnectDialog();
  bool Go();
  void ConnectDialog::Position(BWindow *parent);
  virtual void MessageReceived(BMessage *message);
  BTextControl *text;
};

enum {
  MSG_OK = 'ok',
  MSG_CANCEL = 'can'
};

static pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;

/* --- static variables --- */

static ef_parser_t *utf16_parser;

/* --- static functions --- */

static void set_input_focus(MLView *view) {
  view->MakeFocus(true);

  XEvent ev;

  ev.type = UI_KEY_FOCUS_IN;
  ui_window_receive_event(view->uiwindow, &ev);
}

static void update_ime_text(ui_window_t *uiwindow, const char *preedit_text) {
  (*uiwindow->preedit)(uiwindow, preedit_text, NULL);
}

static void draw_screen(ui_window_t *uiwindow, BRect update, int force_expose) {
  XExposeEvent ev;

  ev.type = UI_EXPOSE;
  ev.x = update.left;
  ev.y = update.top;
  ev.width = update.right - update.left + 1.0;
  ev.height = update.bottom - update.top + 1.0;
  ev.force_expose = force_expose;

  ui_window_receive_event(uiwindow, (XEvent *)&ev);
  uiwindow->update_window_flag = 0;
}

static void window_lock(MLWindow *window) {
#ifdef __DEBUG
  bl_debug_printf("Locking %p(%s)\n", uiwindow, uiwindow->parent ? "child" : "root");
#endif

#ifdef DEBUG
  if (!window->IsLocked()) {
    bl_debug_printf(BL_DEBUG_TAG " Looper is not locked\n");
    beos_lock();
  } else
#endif
  {
    window->UnlockLooper();
    beos_lock();
    window->LockLooper();
  }

#ifdef __DEBUG
  bl_debug_printf("Done\n");
#endif
}

static void view_lock(MLView *view) {
#ifdef __DEBUG
  bl_debug_printf("Locking %p(%s)\n", uiwindow, uiwindow->parent ? "child" : "root");
#endif

#ifdef DEBUG
  if (!view->Window()->IsLocked()) {
    bl_debug_printf(BL_DEBUG_TAG " Looper is not locked\n");
    beos_lock();
  } else
#endif
  {
    view->UnlockLooper();
    beos_lock();
    view->LockLooper();
  }

#ifdef __DEBUG
  bl_debug_printf("Done\n");
#endif
}

/* --- class methods --- */

/* Lock is unnecessary because no mlterm function is called. */
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

  window_lock(this);

  ui_window_receive_event(ui_get_root_window(uiwindow), &ev);

  beos_unlock();
}

/*
 * I don't know why but the value of 'width' or 'height' is 'right - left' or
 * 'bottom - top'.
 */
void MLWindow::FrameResized(float width, float height) {
  window_lock(this);

  /* MLWindow::ResizeTo() causes FrameResized() event. */
  if (uiwindow->width != ((u_int)width) + 1 - uiwindow->hmargin * 2 ||
      uiwindow->height != ((u_int)height) + 1 - uiwindow->vmargin * 2) {
    uiwindow->width = width + 1 - uiwindow->hmargin * 2;
    uiwindow->height = height + 1 - uiwindow->vmargin * 2;

    XEvent ev;

    ev.type = UI_RESIZE;
    ui_window_receive_event(uiwindow, &ev);
  }

  beos_unlock();
}

void MLWindow::Quit() {
  if (uiwindow) {
    if (uiwindow->disp->num_roots == 1) {
      be_app->PostMessage(B_QUIT_REQUESTED);
    }

    XEvent ev;

    ev.type = UI_CLOSE_WINDOW;

    window_lock(this);

    ui_window_receive_event(uiwindow, &ev);

    /*
     * Not reach.
     * window_destroyed() => ui_window_final() => window_dealloc() =>
     * MLWindow::Quit() => BWindow::Quit()
     */
  } else {
    /* Called from window_dealloc() */

    beos_unlock();

    BWindow::Quit();

    /* Not reach. This thread exits in BWindow::Quit(). */
  }
}

/* Lock is unnecessary because no mlterm function is called. */
MLView::MLView(BRect frame, const char *title, uint32 resizemode, uint32 flags)
  : BView(frame, title, resizemode, flags) {
  SetViewColor(255, 255, 255);
  SetHighColor(0, 0, 0);
  SetLowColor(0, 0, 0);
  SetFontSize(14);
  SetDrawingMode(B_OP_OVER);
  cur_font = NULL;
  buttons = 0;
  messenger = NULL;

  /* uiwindow is initialized in view_alloc() or window_alloc(). */
#if 0
  uiwindow = NULL;
#endif
}

void MLView::Draw(BRect update) {
  if (!uiwindow || (IS_UISCREEN(uiwindow) && !((ui_screen_t *)uiwindow)->term)) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  view_lock(this);

  draw_screen(uiwindow, update, 0);

  beos_unlock();
}

void MLView::KeyDown(const char *bytes, int32 numBytes) {
  if (!uiwindow || (IS_UISCREEN(uiwindow) && !((ui_screen_t *)uiwindow)->term)) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  view_lock(this);

  int32 modifiers;

  Window()->CurrentMessage()->FindInt32((const char*)"modifiers", &modifiers);

  XKeyEvent kev;

  kev.type = UI_KEY_PRESS;
  kev.state = modifiers & (ShiftMask|ControlMask|Mod1Mask|CommandMask);

  if (numBytes == 1) {
    if (bytes[0] == B_FUNCTION_KEY) {
      int32 key;

      Window()->CurrentMessage()->FindInt32((const char*)"key", &key);
      kev.keysym = (key | 0xf000);
      kev.utf8 = NULL;
    } else {
      kev.keysym = *bytes;

      switch(kev.keysym) {
      case B_LEFT_ARROW:  /* 0x61 (raw key - jp106) */
      case B_RIGHT_ARROW: /* 0x63 */
      case B_UP_ARROW:    /* 0x57 */
      case B_DOWN_ARROW:  /* 0x62 */
      case B_INSERT:      /* 0x1f */
      case B_DELETE:      /* 0x34 */
      case B_HOME:        /* 0x20 */
      case B_END:         /* 0x35 */
      case B_PAGE_UP:     /* 0x21 */
      case B_PAGE_DOWN:   /* 0x36 */
        if (kev.state & ControlMask) {
          int32 key;

          Window()->CurrentMessage()->FindInt32((const char*)"key", &key); /* raw key */
          if (key <= 0x1e || (0x22 <= key && key <= 0x33) || (0x37 <= key && key <= 0x56) ||
              (0x58 <= key && key <= 0x60) || 0x64 <= key) {
            kev.utf8 = bytes;
            break;
          }
        }

        kev.keysym |= 0xe000;
        kev.utf8 = NULL;
        break;

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

  beos_unlock();
}

void MLView::MouseDown(BPoint where) {
  if (!uiwindow || (IS_UISCREEN(uiwindow) && !((ui_screen_t *)uiwindow)->term)) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  view_lock(this);

  if (!IsFocus()) {
    set_input_focus(this);
  }

  int32 clicks;
  int64 when; /* microsec */
  int32 modifiers;

  Window()->CurrentMessage()->FindInt32((const char*)"buttons", &this->buttons);
  Window()->CurrentMessage()->FindInt32((const char*)"clicks", &clicks);
  Window()->CurrentMessage()->FindInt64((const char*)"when", &when);
  Window()->CurrentMessage()->FindInt32((const char*)"modifiers", &modifiers);

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

  beos_unlock();
}

void MLView::MouseMoved(BPoint where, uint32 code, const BMessage *dragMessage) {
  if (dragMessage || !uiwindow || (IS_UISCREEN(uiwindow) && !((ui_screen_t *)uiwindow)->term)) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  view_lock(this);

  int32 buttons;
  int64 when; /* microsec */
  int32 modifiers;

  Window()->CurrentMessage()->FindInt32((const char*)"buttons", &buttons);
  Window()->CurrentMessage()->FindInt64((const char*)"when", &when);
  Window()->CurrentMessage()->FindInt32((const char*)"modifiers", &modifiers);

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

  beos_unlock();
}

void MLView::MouseUp(BPoint where) {
  if (!uiwindow || (IS_UISCREEN(uiwindow) && !((ui_screen_t *)uiwindow)->term)) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  view_lock(this);

  int32 clicks;
  int64 when; /* microsec */

  Window()->CurrentMessage()->FindInt32((const char*)"clicks", &clicks);
  Window()->CurrentMessage()->FindInt64((const char*)"when", &when);

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

  beos_unlock();
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

void MLView::MessageReceived(BMessage *message) {
  if (message->what == B_SIMPLE_DATA) {
    int32 count = 0;
    entry_ref ref;

    while (message->FindRef("refs", count++, &ref) == B_OK) {
      BPath path(&ref);

      XSelectionNotifyEvent ev;
      ev.type = UI_SELECTION_NOTIFIED;
      ev.data = (char*)path.Path();
      ev.len = strlen(ev.data);

      view_lock(this);
      ui_window_receive_event(uiwindow, (XEvent*)&ev);
      beos_unlock();
    }
  } else if (message->what == B_INPUT_METHOD_EVENT) {
    if (IS_UISCREEN(uiwindow) && !((ui_screen_t *)uiwindow)->term) {
      /* It has been already removed from ui_layout or term has been detached. */
      return;
    }

    view_lock(this);

    int32 opcode;

    /*
     * If a focused view is changed from a view where you are inputting with
     * ime to another in splitted screen, following check enclosed by
     * #if 1 ... #endif is necessary.
     */
    if (message->FindInt32("be:opcode", &opcode) == B_OK) {
      if (opcode == B_INPUT_METHOD_STARTED) {
        BMessenger m;

        if (message->FindMessenger("be:reply_to", &m) == B_OK) {
          if (messenger) {
            delete messenger;
          }

          messenger = new BMessenger(m);
        }
      } else if (opcode == B_INPUT_METHOD_STOPPED) {
        if (messenger) {
          delete messenger;
          messenger = NULL;
        }
      } else if (opcode == B_INPUT_METHOD_CHANGED) {
        const char *str;

        if (message->FindString("be:string", &str) != B_OK || *str == '\0') {
          update_ime_text(uiwindow, "");

          goto end;
        }

        bool confirmed;

        if (message->FindBool("be:confirmed", &confirmed) == B_OK && confirmed) {
          update_ime_text(uiwindow, "");

          XKeyEvent kev;

          kev.type = UI_KEY_PRESS;
          kev.state = 0;
          kev.keysym = 0;
          kev.utf8 = str;

          ui_window_receive_event(uiwindow, (XEvent *)&kev);
        } else {
          int32 start;
          int32 end;
#if 0
          int32 count = 0;

          while (message->FindInt32("be:clause_start", count, &start) == B_OK &&
                 message->FindInt32("be:clause_end", count, &end) == B_OK) {
            bl_debug_printf("Clause %d: %d-%d\n", count, start, end);
            count++;
          }
#endif

          message->FindInt32("be:selection", 1, &end);
          if (end > 0) {
            message->FindInt32("be:selection", 0, &start);

            char str2[strlen(str) + 10];
            char *p = str2;

            memcpy(p, str, start);
            strcpy((p += start), "\x1b[7m");

            int32 len = end - start;
            strncpy((p += 4), str + start, len);
            strcpy((p += len), "\x1b[27m");
            strcpy((p += 5), str + end);

            update_ime_text(uiwindow, str2);
          } else {
            update_ime_text(uiwindow, str);
          }
        }
      } else if (opcode == B_INPUT_METHOD_LOCATION_REQUEST && messenger) {
        int x;
        int y;

        if (!uiwindow->xim_listener ||
            !(*uiwindow->xim_listener->get_spot)(uiwindow->xim_listener->self, &x, &y)) {
          x = y = 0;
        }

        BMessage message(B_INPUT_METHOD_EVENT);
        message.AddInt32("be:opcode", B_INPUT_METHOD_LOCATION_REQUEST);
        BPoint where((float)x, (float)y);
        ConvertToScreen(&where);

        float height = ui_line_height((ui_screen_t*)uiwindow);
        message.AddPoint("be:location_reply", where);
        message.AddFloat("be:height_reply", height);
        messenger->SendMessage(&message);
      }
    }

  end:
    beos_unlock();
  }
}

/* --- global functions --- */

void view_alloc(ui_window_t *uiwindow, int x, int y, u_int width, u_int height) {
  uint32 flags = B_WILL_DRAW;

  if (uiwindow->key_pressed) {
    flags |= B_INPUT_METHOD_AWARE;
  }

  MLView *view = new MLView(BRect(x, y, x + width - 1, y + height - 1), "mlterm",
                            B_FOLLOW_NONE, flags);

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

  win->RemoveChild((BView*)view);
  ((MLView*)view)->uiwindow = None;
  delete (BView*)view;

  win->UnlockLooper();
}

void view_update(/* BView */ void *view, int force_expose) {
  ui_window_t *uiwindow = ((MLView*)view)->uiwindow;
  int x;
  int y;

  if (!uiwindow->xim_listener ||
      !(*uiwindow->xim_listener->get_spot)(uiwindow->xim_listener->self, &x, &y)) {
    x = y = 0;
  }

  x += (uiwindow->hmargin);
  y += (uiwindow->vmargin);

  draw_screen(uiwindow, BRect(x, y, x, y), force_expose);
}

void view_set_clip(/* BView */ void *view, int x, int y, u_int width, u_int height) {
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  BRegion *region = new BRegion(BRect(x, y, x + width - 1, y + height - 1));
  ((BView*)view)->ConstrainClippingRegion(region);

  ((BView*)view)->UnlockLooper();
}

void view_unset_clip(/* BView */ void *view) {
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BView*)view)->ConstrainClippingRegion(NULL);

  ((BView*)view)->UnlockLooper();
}

void view_draw_string(/* BView */ void *view, ui_font_t *font, ui_color_t *fg_color,
                      int x, int y, char *str, size_t len) {
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  u_int32_t pixel = fg_color->pixel;
  ((BView*)view)->SetHighColor((pixel >> 16) & 0xff, (pixel >> 8) & 0xff, pixel & 0xff,
                               (pixel >> 24) & 0xff);
  ((BView*)view)->SetFont((BFont*)font->xfont->fid);
  ((BView*)view)->MovePenTo(x, y);
  ((BView*)view)->DrawString(str, len);

  if (font->double_draw_gap) {
    ((BView*)view)->MovePenTo(x + font->double_draw_gap, y);
    ((BView*)view)->DrawString(str, len);
  }

  ((BView*)view)->UnlockLooper();
}

void view_draw_string16(/* BView */ void *view, ui_font_t *font, ui_color_t *fg_color,
                        int x, int y, XChar2b *str, size_t len) {
  if (!utf16_parser) {
    utf16_parser = ef_utf16le_parser_new(); /* XXX leaked */
  }
  ef_conv_t *utf8_conv = ui_get_selection_conv(1);

  char str2[len * UTF_MAX_SIZE];

  (*utf16_parser->init)(utf16_parser);
  (*utf16_parser->set_str)(utf16_parser, (u_char*)str, len * 2);
  (*utf8_conv->init)(utf8_conv);
  len = (*utf8_conv->convert)(utf8_conv, (u_char*)str2, len * UTF_MAX_SIZE, utf16_parser);

  /* LockLooper() is called in view_draw_string() */
  view_draw_string(view, font, fg_color, x, y, str2, len);
}

void view_fill_with(/* BView */ void *view, ui_color_t *color, int x, int y,
                    u_int width /* > 0 */, u_int height /* > 0 */) {
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  u_long pixel = color->pixel;
  ((BView*)view)->SetLowColor((pixel >> 16) & 0xff, (pixel >> 8) & 0xff, pixel & 0xff,
                              (pixel >> 24) & 0xff);
  ((BView*)view)->FillRect(BRect(x, y, x + width - 1, y + height - 1), B_SOLID_LOW);

  ((BView*)view)->UnlockLooper();
}

void view_draw_rect_frame(/* BView */ void *view, ui_color_t *color, int x1, int y1,
                          int x2, int y2) {
  u_long pixel = color->pixel;
  rgb_color rgb;

  rgb.red = (pixel >> 16) & 0xff;
  rgb.green = (pixel >> 8) & 0xff;
  rgb.blue = pixel & 0xff;
  rgb.alpha = (pixel >> 24) & 0xff;

  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BView*)view)->BeginLineArray(4);
  ((BView*)view)->AddLine(BPoint(x1, y1), BPoint(x1, y2), rgb);
  ((BView*)view)->AddLine(BPoint(x1, y2), BPoint(x2, y2), rgb);
  ((BView*)view)->AddLine(BPoint(x2, y1), BPoint(x2, y2), rgb);
  ((BView*)view)->AddLine(BPoint(x1, y1), BPoint(x2, y1), rgb);
  ((BView*)view)->EndLineArray();

  ((BView*)view)->UnlockLooper();
}

void view_copy_area(/* BView */ void *view, Pixmap src, int src_x, int src_y,
                    u_int width, u_int height, int dst_x, int dst_y) {
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BView*)view)->DrawBitmap((BBitmap*)src,
                             BRect(src_x, src_y, src_x + width - 1, src_y + height - 1),
                             BRect(dst_x, dst_y, dst_x + width - 1, dst_y + height - 1));

  ((BView*)view)->UnlockLooper();
}

void view_scroll(/* BView */ void *view, int src_x, int src_y, u_int width,
                 u_int height, int dst_x, int dst_y) {
#if 0
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BView*)view)->CopyBits(BRect(src_x, src_y, src_x + width - 1, src_y + height - 1),
                           BRect(dst_x, dst_y, dst_x + width - 1, dst_y + height - 1));

  ((BView*)view)->UnlockLooper();
#endif
}

void view_bg_color_changed(/* BView */ void *view) {
}

void view_visual_bell(/* BView */ void *view) {
  if (((BView*)view)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  u_long pixel = ((MLView*)view)->uiwindow->fg_color.pixel;
  ((BView*)view)->SetLowColor((pixel >> 16) & 0xff, (pixel >> 8) & 0xff, pixel & 0xff,
                              (pixel >> 24) & 0xff);

  ui_window_t *win = ((MLView*)view)->uiwindow;
  ((BView*)view)->FillRect(BRect(0, 0, ACTUAL_WIDTH(win) - 1, ACTUAL_HEIGHT(win) - 1), B_SOLID_LOW);

  ((BView*)view)->UnlockLooper();

  usleep(100000); /* 100 msec */

  draw_screen(((MLView*)view)->uiwindow,
              BRect(0, 0, ACTUAL_WIDTH(win) - 1, ACTUAL_HEIGHT(win) - 1), 1);
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

  ((BView*)view)->ResizeTo((float)width - 1.0, (float)height - 1.0);

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

void window_alloc(ui_window_t *root, int x, int y, u_int width, u_int height, int popup) {
  MLWindow *window;

  if (popup) {
    window = new MLWindow(BRect(x, y, x + width - 1, y + height - 1), "",
                          B_BORDERED_WINDOW,
                          B_NOT_RESIZABLE|B_NOT_CLOSABLE|B_NOT_ZOOMABLE|B_NOT_MINIMIZABLE|
                          B_NOT_MOVABLE|B_AVOID_FOCUS|B_NOT_ANCHORED_ON_ACTIVATE);

    MLView *view = new MLView(BRect(0, 0, width - 1, height - 1), "IM", B_FOLLOW_NONE, B_WILL_DRAW);

    view->uiwindow = root;
    window->AddChild(view);
  } else {
#if 1
    /* XXX */
    static int count = 0;
    BRect r = BScreen(window).Frame();
    int x = ((r.right - width) / 8) * count + 50;
    int y = ((r.bottom - height) / 8) * count + 50;
    if (++count == 8) {
      count = 0;
    }
#endif

    window = new MLWindow(BRect(x, y, x + width - 1, y + height - 1),
                          "mlterm", B_DOCUMENT_WINDOW, 0);
    if (root->disp->width == 0) {
      BScreen *screen = new BScreen(window);
      BRect rect = screen->Frame();
      root->disp->width = rect.right + 1;
      root->disp->height = rect.bottom + 1;
      delete screen;
    }
  }

  window->uiwindow = root;
  root->my_window = (void*)window;
}

void window_show(void *window) {
  if (((BWindow*)window)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BWindow*)window)->Show();

  ((BWindow*)window)->UnlockLooper();
}

void window_dealloc(/* BWindow */ void *window) {
  if (((BWindow*)window)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((MLWindow*)window)->uiwindow = None;

  ((BWindow*)window)->Quit();
  /* Not reach. This thread exits in BWindow::Quit(). */
}

void window_move(/* BWindow */ void *window, int x, int y) {
  if (((BWindow*)window)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BWindow*)window)->MoveTo((float)x, (float)y);

  ((BWindow*)window)->UnlockLooper();
}

void window_resize(/* BWindow */ void *window, int width, int height) {
  if (((BWindow*)window)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BWindow*)window)->ResizeTo((float)width - 1.0, (float)height - 1.0);

  BView *view;
  if ((view = (BView*)window_get_orphan(window, 0))) {
    ((BView*)view)->ResizeTo((float)width - 1.0, (float)height - 1.0);
  }

  ((BWindow*)window)->UnlockLooper();
}

void window_get_position(/* BWindow */ void *window, int *x, int *y) {
  BRect rect = ((BWindow*)window)->Frame();
  *x = rect.left;
  *y = rect.top;
}

void window_set_title(/* BWindow */ void *window, const char *title /* utf8 */) {
  if (((BWindow*)window)->LockLooperWithTimeout(B_INFINITE_TIMEOUT) != B_OK) {
    return;
  }

  ((BWindow*)window)->SetTitle(title);

  ((BWindow*)window)->UnlockLooper();
}

void *window_get_orphan(void *window, int idx) {
  if (((MLWindow*)window)->uiwindow->num_children == 0) {
    void *child = ((BWindow*)window)->ChildAt(idx);

    return child;
  } else {
    return NULL;
  }
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
  }
  ef_conv_t *utf8_conv = ui_get_selection_conv(1);

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
  BBitmap *bitmap;

  if ((bitmap = new BBitmap(BRect(0, 0, width - 1, height - 1), 0, B_RGBA32))) {
    bitmap->SetBits(data, len, 0, B_RGBA32);
  }

  return bitmap;
}

void beos_destroy_image(void *bitmap) {
  delete (BBitmap*)bitmap;
}

void *beos_load_image(const char *path, u_int *width, u_int *height) {
  BBitmap *bitmap;

  if ((bitmap = BTranslationUtils::GetBitmap(path))) {
    BRect rect = bitmap->Bounds();
    *width = rect.right + 1;
    *height = rect.bottom + 1;
  }

  return bitmap;
}

void *beos_resize_image(void *bitmap, u_int width, u_int height) {
  BRect rect(0, 0, width - 1, height - 1);
  BBitmap *new_bitmap = new BBitmap(rect, B_RGBA32, true);
  BView *view = new BView(rect, "", B_FOLLOW_ALL_SIDES, 0);
  new_bitmap->AddChild(view);
  new_bitmap->Lock();
  view->DrawBitmap((BBitmap*)bitmap, ((BBitmap*)bitmap)->Bounds(), rect);
  new_bitmap->Unlock();

  new_bitmap->RemoveChild(view);
  delete view;

  delete (BBitmap*)bitmap;

  return new_bitmap;
}

u_char *beos_get_bits(void *bitmap) {
  return (u_char*)((BBitmap*)bitmap)->Bits();
}

ConnectDialog::ConnectDialog()
  : BWindow(BRect(0, 0, 200, 100), "Password", B_MODAL_WINDOW,
            B_NOT_RESIZABLE|B_NOT_CLOSABLE|B_NOT_ZOOMABLE|B_NOT_MINIMIZABLE|B_NOT_MOVABLE, 0) {
  BButton *ok = new BButton(BRect(0, 0, 100, 100), "OkButton", "OK", new BMessage(MSG_OK));
  BButton *cancel = new BButton(BRect(0, 0, 100, 100), "CancelButton", "Cancel",
                                new BMessage(MSG_CANCEL));

  text = new BTextControl("", "Password", "", NULL);
  text->TextView()->HideTyping(true);

  BLayoutBuilder::Group<>(this, B_VERTICAL)
    .Add(text)
    .AddGroup(B_HORIZONTAL)
      .AddGlue()
      .Add(ok)
      .Add(cancel)
    .End()
    .SetInsets(B_USE_DEFAULT_SPACING);

  SetDefaultButton(ok);
  button = -1;
  text->MakeFocus();
}

bool ConnectDialog::Go() {
  Show();

  while (button == -1) {
    usleep(1000);
  }

  return (button == 0);
}

void ConnectDialog::Position(BWindow *parent) {
  BRect rect = (parent ? parent->Frame() : BScreen(this).Frame());

  MoveTo((rect.right + rect.left) / 2 - 100, (rect.bottom + rect.top) / 2 - 50);
}

void ConnectDialog::MessageReceived(BMessage *message) {
  switch(message->what) {
  case MSG_OK:
    button = 0;
    break;

  case MSG_CANCEL:
    button = 1;
    break;

  default:
    return;
  }

  PostMessage(B_QUIT_REQUESTED);
}

char *beos_dialog_password(const char *msg) {
  ConnectDialog *dialog = new ConnectDialog();
  dialog->Position(dynamic_cast<BWindow*>(BLooper::LooperForThread(find_thread(NULL))));

  if (dialog->Go()) {
    return strdup(dialog->text->Text());
  } else {
    return NULL;
  }
}

int beos_dialog_okcancel(const char *msg) {
  BAlert *alert = new BAlert("Dialog", msg, "OK", "Cencel");

  if (alert->Go() == 0) {
    return 1;
  } else {
    return 0;
  }
}

int beos_dialog_alert(const char *msg) {
  BAlert *alert = new BAlert("Alert", msg, "OK");

  alert->Go();

  return 1;
}

void beos_lock(void) {
  pthread_mutex_lock(&mutex);
}

void beos_unlock(void) {
  pthread_mutex_unlock(&mutex);
}
