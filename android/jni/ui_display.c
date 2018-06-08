/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_display.h"

#include <unistd.h> /* STDIN_FILENO */
#include <stdlib.h> /* getenv */
#include <linux/kd.h>
#include <linux/keyboard.h>

#include <pobl/bl_debug.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h> /* bl_str_alloca_dup */
#include <pobl/bl_path.h>
#include <pobl/bl_dialog.h>

#include "../ui_window.h"
#include "../../common/c_animgif.c"

#define DISP_IS_INITED (_disp.display)

/* --- static functions --- */

static ui_display_t _disp;
static Display _display;
static int locked;

/* --- static functions --- */

static int display_lock(void) {
  if (locked < 0) {
    return 0;
  } else if (!locked) {
    ANativeWindow_Buffer buf;

    if (ANativeWindow_lock(_display.app->window, &buf, NULL) != 0) {
      return 0;
    }

    if (_display.buf.bits != buf.bits) {
      if (_display.buf.bits && _display.buf.height == buf.height &&
          _display.buf.stride == buf.stride) {
        memcpy(buf.bits, _display.buf.bits,
               _display.buf.height * _display.buf.stride * _display.bytes_per_pixel);
      }

      _display.buf = buf;
    }

    locked = 1;
  }

  return 1;
}

static inline u_char *get_fb(int x, int y) {
  return ((u_char *)_display.buf.bits) +
         ((_display.yoffset + y) * _display.buf.stride + x) * _display.bytes_per_pixel;
}

static int kcode_to_ksym(int kcode, int state) {
  /* US Keyboard */

  if (AKEYCODE_0 <= kcode && kcode <= AKEYCODE_9) {
    if (state & ShiftMask) {
      char *num_key_shift = ")!@#$%^&*(";

      return num_key_shift[kcode - AKEYCODE_0];
    } else {
      return kcode + 41;
    }
  } else if (AKEYCODE_A <= kcode && kcode <= AKEYCODE_Z) {
    kcode += 68;

    if (state & ShiftMask) {
      kcode -= 0x20;
    }

    return kcode;
  } else {
    if (state & ShiftMask) {
      switch (kcode) {
        case AKEYCODE_COMMA:
          return '<';

        case AKEYCODE_PERIOD:
          return '>';

        case AKEYCODE_SPACE:
          return ' ';

        case AKEYCODE_GRAVE:
          return '~';

        case AKEYCODE_MINUS:
          return '_';

        case AKEYCODE_EQUALS:
          return '+';

        case AKEYCODE_LEFT_BRACKET:
          return '{';

        case AKEYCODE_RIGHT_BRACKET:
          return '}';

        case AKEYCODE_BACKSLASH:
          return '|';

        case AKEYCODE_SEMICOLON:
          return ':';

        case AKEYCODE_APOSTROPHE:
          return '\"';

        case AKEYCODE_SLASH:
          return '?';
      }
    }

    switch (kcode) {
      case AKEYCODE_ENTER:
        return 0x0d;

      case AKEYCODE_DEL:
        return 0x08;

      case AKEYCODE_COMMA:
        return ',';

      case AKEYCODE_PERIOD:
        return '.';

      case AKEYCODE_TAB:
        return '\t';

      case AKEYCODE_SPACE:
        return ' ';

      case AKEYCODE_GRAVE:
        return '`';

      case AKEYCODE_MINUS:
        return '-';

      case AKEYCODE_EQUALS:
        return '=';

      case AKEYCODE_BACKSLASH:
        return '\\';

      case AKEYCODE_LEFT_BRACKET:
        return '[';

      case AKEYCODE_RIGHT_BRACKET:
        return ']';

      case AKEYCODE_SEMICOLON:
        return ';';

      case AKEYCODE_APOSTROPHE:
        return '\'';

      case AKEYCODE_SLASH:
        return '/';

      case AKEYCODE_AT:
        return '@';

      case AKEYCODE_ESCAPE:
        return '\x1b';

      default:
        if (kcode == -0x3ed) {
          /* XXX for Nihongo Full Keyboard */
          return XK_Henkan_Mode;
        } else if (kcode == -0x3ec) {
          /* XXX for Nihongo Full Keyboard */
          return XK_Muhenkan;
        } else if (kcode < 0) {
          return 0;
        } else {
          return kcode + 0x100;
        }
    }
  }
}

static int process_key_event(int action, int code) {
  if (code == AKEYCODE_BACK) {
    return 0;
  }

  if (action == AKEY_EVENT_ACTION_DOWN) {
    if (code == AKEYCODE_SHIFT_RIGHT || code == AKEYCODE_SHIFT_LEFT) {
      _display.key_state |= ShiftMask;
    }
#if 0
    else if (code == KEY_CAPSLOCK) {
      if (_display.key_state & ShiftMask) {
        _display.key_state &= ~ShiftMask;
      } else {
        _display.key_state |= ShiftMask;
      }
    }
#endif
    else if (code == AKEYCODE_CONTROL_RIGHT || code == AKEYCODE_CONTROL_LEFT) {
      _display.key_state |= ControlMask;
    } else if (code == AKEYCODE_ALT_RIGHT || code == AKEYCODE_ALT_LEFT) {
      _display.key_state |= Mod1Mask;
    }
#if 0
    else if (code == KEY_NUMLOCK) {
      _display.lock_state ^= NLKED;
    }
#endif
    else {
      XKeyEvent xev;

      xev.type = KeyPress;
      xev.ksym = kcode_to_ksym(code, _display.key_state);
      xev.keycode = code;
      xev.state = _display.button_state | _display.key_state;

      ui_window_receive_event(_disp.roots[0], &xev);
    }
  } else if (action == AKEY_EVENT_ACTION_MULTIPLE) {
    XKeyEvent xev;

    xev.type = KeyPress;
    xev.ksym = 0;
    xev.keycode = 0;
    xev.state = 0;

    ui_window_receive_event(_disp.roots[0], &xev);
  } else /* if( action == AKEY_EVENT_ACTION_UP) */
  {
    if (code == AKEYCODE_SHIFT_RIGHT || code == AKEYCODE_SHIFT_LEFT) {
      _display.key_state &= ~ShiftMask;
    } else if (code == AKEYCODE_CONTROL_RIGHT || code == AKEYCODE_CONTROL_LEFT) {
      _display.key_state &= ~ControlMask;
    } else if (code == AKEYCODE_ALT_RIGHT || code == AKEYCODE_ALT_LEFT) {
      _display.key_state &= ~Mod1Mask;
    }
  }

  return 1;
}

static JNIEnv *get_jni_env(JavaVM *vm) {
  JNIEnv *env;
  if ((*vm)->GetEnv(vm, &env, JNI_VERSION_1_6) == JNI_OK) {
    return env;
  } else {
    return NULL;
  }
}

static int dialog_cb(bl_dialog_style_t style, const char *msg) {
  JNIEnv *env;

  if (style == BL_DIALOG_ALERT &&
      (env = get_jni_env(_display.app->activity->vm))) {
    jobject this = _display.app->activity->clazz;
    (*env)->CallVoidMethod(env, this,
                           (*env)->GetMethodID(env, (*env)->GetObjectClass(env, this),
                                               "showMessage",
                                               "(Ljava/lang/String;)V"),
                           (*env)->NewStringUTF(env, msg));

    return 1;
  } else {
    return -1;
  }
}

static void show_soft_input(JavaVM *vm) {
  JNIEnv *env;

  if ((env = get_jni_env(vm))) {
    jobject this = _display.app->activity->clazz;
    (*env)->CallVoidMethod(env, this, (*env)->GetMethodID(env, (*env)->GetObjectClass(env, this),
                                                        "showSoftInput", "()V"));
  }
}

/* 
 * _disp.roots[1] is ignored.
 */
static inline ui_window_t *get_window(int x, /* X in display */
                                      int y  /* Y in display */
                                      ) {
  ui_window_t *win;
  u_int count;

  for (count = 0; count < _disp.roots[0]->num_children; count++) {
    if ((win = _disp.roots[0]->children[count])->is_mapped) {
      if (win->x <= x && x < win->x + ACTUAL_WIDTH(win) && win->y <= y &&
          y < win->y + ACTUAL_HEIGHT(win)) {
        return win;
      }
    }
  }

  return _disp.roots[0];
}

static int process_mouse_event(int source, int action, int64_t time, int x, int y) {
  if (source & AINPUT_SOURCE_MOUSE) {
    static XButtonEvent prev_xev;
    XButtonEvent xev;
    ui_window_t *win;

    switch (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) {
      case 0x100:
        _display.button_state = Button1Mask;
        xev.button = Button1;
        break;

      case 0x200:
        xev.button = Button2;
        _display.button_state = Button2Mask;
        break;

      case 0x300:
        xev.button = Button3;
        _display.button_state = Button3Mask;
        break;

      case 0x0:
      default:
        xev.button = 0;
        break;
    }

    switch (action & AMOTION_EVENT_ACTION_MASK) {
      case AMOTION_EVENT_ACTION_DOWN:
        xev.type = ButtonPress;
        if (xev.button == 0) {
          xev.button = Button1;
          _display.button_state = Button1Mask;
        }
        break;

      case AMOTION_EVENT_ACTION_UP:
        xev.type = ButtonRelease;
        /* Reset button_state in releasing button. */
        _display.button_state = 0;
        if (xev.button == 0) {
          xev.button = Button1;
        }
        break;

      case 7 /* AMOTION_EVENT_ACTION_HOVER_MOVE */:
      case AMOTION_EVENT_ACTION_MOVE:
        xev.type = MotionNotify;
        if (_display.long_press_counter > 0) {
          xev.button = Button1;
          _display.button_state = Button1Mask;
        }
        break;

      default:
        return 1;
    }

    xev.time = time / 1000000;
    xev.x = x;
    xev.y = y;
    xev.state = _display.key_state;

    if (xev.type == ButtonPress) {
      static int click_num;

      /* 800x600 display => (800+600) / 64 = 21 */
      if (xev.x + xev.y + (_disp.width + _disp.height) / 64 >= _disp.width + _disp.height) {
        if (click_num == 0) {
          click_num = 1;
        } else /* if( click_num == 1) */ {
          click_num = 0;
#if 0
          /* This doesn't work on Android 3.x and 4.x. */
          ANativeActivity_showSoftInput(_display.app->activity,
                                        ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);
#else
          show_soft_input(_display.app->activity->vm);
#endif
        }
      } else {
        click_num = 0;
      }

      memset(&prev_xev, 0, sizeof(prev_xev));
    } else if (xev.type == MotionNotify) {
      if (prev_xev.type == MotionNotify && prev_xev.x == xev.x && prev_xev.y == xev.y) {
        /* Long click on touch panel sends same MotionNotify events continuously. */
#if 0
        bl_debug_printf("Same event is ignored.\n");
#endif
        if (xev.time >= prev_xev.time + 100) {
          /* 0.1 sec */
          prev_xev = xev;
          ui_display_idling(NULL);
        }

        return 1;
      }
      prev_xev = xev;
    }

#if 0
    bl_debug_printf(
        BL_DEBUG_TAG "Button is %s x %d y %d btn %d time %d\n",
        xev.type == ButtonPress ? "pressed" : (xev.type == MotionNotify ? "motion" : "released"),
        xev.x, xev.y, xev.button, xev.time);
#endif

    win = get_window(xev.x, xev.y);
    xev.x -= win->x;
    xev.y -= win->y;

    if (xev.type != ButtonRelease && xev.button == Button1 &&
        /* XXX excludes scrollbar (see x_window_init() called in ui_scrollbar.c) */
        win->width > win->min_width &&
        win->hmargin < xev.x && xev.x < win->hmargin + win->width &&
        win->vmargin < xev.y && xev.y < win->vmargin + win->height) {
      /* Start long pressing */
      _display.long_press_counter = 1;
    } else {
      _display.long_press_counter = 0;
    }

    ui_window_receive_event(win, &xev);
  }

  return 1;
}

static int32_t on_input_event(struct android_app *app, AInputEvent *event) {
  switch (AInputEvent_getType(event)) {
    case AINPUT_EVENT_TYPE_MOTION:
#if 0
      bl_debug_printf("MOTION %d %d x %d y %d\n", AInputEvent_getSource(event),
                      AMotionEvent_getAction(event), AMotionEvent_getX(event, 0),
                      AMotionEvent_getY(event, 0) - _display.yoffset);
#endif
      return process_mouse_event(AInputEvent_getSource(event), AMotionEvent_getAction(event),
                                 AMotionEvent_getEventTime(event), AMotionEvent_getX(event, 0),
                                 AMotionEvent_getY(event, 0) - _display.yoffset);

    case AINPUT_EVENT_TYPE_KEY:
#if 0
      bl_debug_printf("KEY %d %d\n", AKeyEvent_getScanCode(event), AKeyEvent_getKeyCode(event));
#endif
      return process_key_event(AKeyEvent_getAction(event), AKeyEvent_getKeyCode(event));

    default:
      return 0;
  }
}

static void update_window(ui_window_t *win) {
  u_int count;

  ui_window_clear_margin_area(win);

  if (win->window_exposed) {
    (*win->window_exposed)(win, 0, 0, win->width, win->height);
  }

  for (count = 0; count < win->num_children; count++) {
    update_window(win->children[count]);
  }
}

static void init_window(ANativeWindow *window) {
  struct rgb_info rgbinfo = {3, 2, 3, 11, 5, 0};

  if (_disp.width == 0) {
    _disp.width = ANativeWindow_getWidth(window);
    _disp.height = ANativeWindow_getHeight(window);
  } else {
    /* Changed in visibleFrameChanged. */
  }

  _disp.depth = 16;
  _display.bytes_per_pixel = 2;
  _display.rgbinfo = rgbinfo;

  ANativeWindow_setBuffersGeometry(window, 0, 0, WINDOW_FORMAT_RGB_565);

  if (_display.buf.bits) {
    /* mlterm restarted */
    u_int count;

    _display.buf.bits = NULL;

    /* In case of locked is 1 here. */
    locked = 0;

    for (count = 0; count < _disp.num_roots; count++) {
      ui_window_set_mapped_flag(_disp.roots[count], 1); /* In case of restarting */
      update_window(_disp.roots[count]);
    }
  }
}

static void on_app_cmd(struct android_app *app, int32_t cmd) {
  switch (cmd) {
    case APP_CMD_SAVE_STATE:
#ifdef DEBUG
      bl_debug_printf("SAVE_STATE\n");
#endif
      break;

    case APP_CMD_INIT_WINDOW:
#ifdef DEBUG
      bl_debug_printf("INIT_WINDOW\n");
#endif
      init_window(app->window);
      break;

    case APP_CMD_WINDOW_RESIZED:
#ifdef DEBUG
      bl_debug_printf("WINDOW_RESIZED\n");
#endif
      break;

    case APP_CMD_TERM_WINDOW:
#ifdef DEBUG
      bl_debug_printf("TERM_WINDOW\n");
#endif
      {
        u_int count;
        for (count = 0; count < _disp.num_roots; count++) {
          ui_window_set_mapped_flag(_disp.roots[count], 0);
        }
      }
      locked = -1; /* Don't lock until APP_CMD_INIT_WINDOW after restart. */

      break;

    case APP_CMD_WINDOW_REDRAW_NEEDED:
#ifdef DEBUG
      bl_debug_printf("WINDOW_REDRAW_NEEDED\n");
#endif
      break;

    case APP_CMD_CONFIG_CHANGED:
#ifdef DEBUG
      bl_debug_printf("CONFIG_CHANGED\n");
#endif

    case APP_CMD_GAINED_FOCUS:
#ifdef DEBUG
      bl_debug_printf("GAINED_FOCUS\n");
#endif
      /* When our app gains focus, we start monitoring the accelerometer. */
      if (_display.accel_sensor) {
        ASensorEventQueue_enableSensor(_display.sensor_evqueue, _display.accel_sensor);
        /* We'd like to get 60 events per second (in us). */
        ASensorEventQueue_setEventRate(_display.sensor_evqueue, _display.accel_sensor,
                                       (1000L / 60) * 1000);
      }

      break;

    case APP_CMD_LOST_FOCUS:
#ifdef DEBUG
      bl_debug_printf("LOST_FOCUS\n");
#endif
      /*
       * When our app loses focus, we stop monitoring the accelerometer.
       * This is to avoid consuming battery while not being used.
       */
      if (_display.accel_sensor) {
        ASensorEventQueue_disableSensor(_display.sensor_evqueue, _display.accel_sensor);
      }

      break;
  }
}

/* --- global functions --- */

ui_display_t *ui_display_open(char *disp_name, u_int depth) {
  if (!_disp.name && !(_disp.name = getenv("DISPLAY"))) {
    _disp.name = ":0.0";

    /* Callback should be set before bl_dialog() is called. */
    bl_dialog_set_callback(dialog_cb);
  }
  _disp.display = &_display;

  return &_disp;
}

void ui_display_close(ui_display_t *disp) {
  if (disp == &_disp) {
    ui_display_close_all();
  }
}

void ui_display_close_all(void) {
  if (DISP_IS_INITED) {
    u_int count;

    for (count = 0; count < _disp.num_roots; count++) {
#if 0
      ui_window_unmap(_disp.roots[count]);
#endif
      ui_window_final(_disp.roots[count]);
    }

    free(_disp.roots);
    _disp.roots = NULL;

    /* DISP_IS_INITED is false from here. */
    _disp.display = NULL;
  }
}

int ui_display_show_root(ui_display_t *disp, ui_window_t *root, int x, int y, int hint,
                         char *app_name, Window parent_window /* Ignored */
                         ) {
  void *p;

  if ((p = realloc(disp->roots, sizeof(ui_window_t *) * (disp->num_roots + 1))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " realloc failed.\n");
#endif

    return 0;
  }

  disp->roots = p;

  root->disp = disp;
  root->parent = NULL;
  root->parent_window = disp->my_window;
  root->gc = disp->gc;
  root->x = x;
  root->y = y;

  if (app_name) {
    root->app_name = app_name;
  }

  disp->roots[disp->num_roots++] = root;

  /* Cursor is drawn internally by calling ui_display_put_image(). */
  return ui_window_show(root, hint);
}

int ui_display_remove_root(ui_display_t *disp, ui_window_t *root) {
  u_int count;

  for (count = 0; count < disp->num_roots; count++) {
    if (disp->roots[count] == root) {
/* XXX ui_window_unmap resize all windows internally. */
#if 0
      ui_window_unmap(root);
#endif
      ui_window_final(root);

      disp->num_roots--;

      if (count == disp->num_roots) {
        disp->roots[count] = NULL;
      } else {
        disp->roots[count] = disp->roots[disp->num_roots];
      }

      return 1;
    }
  }

  return 0;
}

static void perform_long_click(JavaVM *vm) {
  JNIEnv *env;

  if ((env = get_jni_env(vm))) {
    jobject this = _display.app->activity->clazz;

    (*env)->CallVoidMethod(env, this, (*env)->GetMethodID(env, (*env)->GetObjectClass(env, this),
                                                        "performLongClick", "()V"));
  }
}

void ui_display_idling(ui_display_t *disp /* ignored */
                       ) {
  u_int count;

  if (_display.long_press_counter > 0) {
    if (_display.long_press_counter++ == 10) {
      _display.long_press_counter = 0;
      perform_long_click(_display.app->activity->vm);
    }
  }

  for (count = 0; count < _disp.num_roots; count++) {
    ui_window_idling(_disp.roots[count]);
  }
}

/*
 * Folloing functions called from ui_window.c
 */

int ui_display_own_selection(ui_display_t *disp, ui_window_t *win) {
  if (_disp.selection_owner) {
    ui_display_clear_selection(&_disp, _disp.selection_owner);
  }

  _disp.selection_owner = win;

  return 1;
}

int ui_display_clear_selection(ui_display_t *disp /* can be NULL */, ui_window_t *win) {
  if (_disp.selection_owner == NULL || _disp.selection_owner != win) {
    return 0;
  }

  if (_disp.selection_owner->selection_cleared) {
    (*_disp.selection_owner->selection_cleared)(_disp.selection_owner);
  }

  _disp.selection_owner = NULL;

  return 1;
}

XModifierKeymap *ui_display_get_modifier_mapping(ui_display_t *disp) { return disp->modmap.map; }

void ui_display_update_modifier_mapping(ui_display_t *disp, u_int serial) { /* dummy */ }

XID ui_display_get_group_leader(ui_display_t *disp) { return None; }

void ui_display_rotate(int rotate) {}

int ui_display_init(struct android_app *app) {
  int ret;
  int ident;
  int events;
  struct android_poll_source *source;

  /* Make sure glue isn't stripped. */
  app_dummy();

  ret = _display.app ? 0 : 1;
  _display.app = app;

  app->onAppCmd = on_app_cmd;

  do {
    if ((ident = ALooper_pollAll(-1 /* block forever waiting for events */, NULL, &events,
                                 (void **)&source)) >= 0) {
      /* Process this event. */
      if (source) {
        (*source->process)(app, source);
      }
    }
  } while (!app->window);

  app->onInputEvent = on_input_event;

  /* Prepare to monitor accelerometer. */
  _display.sensor_man = ASensorManager_getInstance();
  _display.accel_sensor =
      ASensorManager_getDefaultSensor(_display.sensor_man, ASENSOR_TYPE_ACCELEROMETER);
  _display.sensor_evqueue =
      ASensorManager_createEventQueue(_display.sensor_man, app->looper, LOOPER_ID_USER, NULL, NULL);

  return ret;
}

void ui_display_final(void) {
  if (locked >= 0) {
    ASensorManager_destroyEventQueue(_display.sensor_man, _display.sensor_evqueue);
    _display.accel_sensor = NULL;

    locked = -1; /* Don't lock until APP_CMD_INIT_WINDOW after restart. */
    ANativeActivity_finish(_display.app->activity);
  }
}

int ui_display_process_event(struct android_poll_source *source, int ident) {
  /* Process this event. */
  if (source) {
    (*source->process)(_display.app, source);
  }

  /* If a sensor has data, process it now. */
  if (ident == LOOPER_ID_USER) {
    if (_display.accel_sensor) {
      ASensorEvent event;

      while (ASensorEventQueue_getEvents(_display.sensor_evqueue, &event, 1) > 0) {
#if 0
        bl_debug_printf("Accelerometer: x=%f y=%f z=%f", event.acceleration.x, event.acceleration.y,
                        event.acceleration.z);
#endif
      }
    }
  }

  /* Check if we are exiting. */
  if (_display.app->destroyRequested) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " destroy requested.\n");
#endif

    locked = -1; /* Don't lock until APP_CMD_INIT_WINDOW after restart. */

    return 0;
  }

  return 1;
}

void ui_display_unlock(void) {
  if (locked > 0) {
    ANativeWindow_unlockAndPost(_display.app->window);
    locked = 0;
  }
}

size_t ui_display_get_str(u_char *seq, size_t seq_len) {
  JNIEnv *env;
  jobject this;
  jstring jstr_key;
  size_t len;

  if (!(env = get_jni_env(_display.app->activity->vm))) {
    return 0;
  }

  this = _display.app->activity->clazz;

  jstr_key =
      (*env)->GetObjectField(env, this, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, this),
                                                           "keyString", "Ljava/lang/String;"));

  if (jstr_key) {
    const char *key;

    key = (*env)->GetStringUTFChars(env, jstr_key, NULL);

    if ((len = strlen(key)) > seq_len) {
      len = 0;
    } else {
      memcpy(seq, key, len);
    }

    (*env)->ReleaseStringUTFChars(env, jstr_key, key);
  } else {
    len = 0;
  }

  return len;
}

u_long ui_display_get_pixel(ui_display_t *disp, int x, int y) {
  u_char *fb;

  fb = get_fb(x, y);

  if (_display.bytes_per_pixel == 4) {
    return *((u_int32_t *)fb);
  } else /* if( _display.bytes_per_pixel == 2) */
  {
    return *((u_int16_t *)fb);
  }
}

void ui_display_put_image(ui_display_t *disp, int x, int y, u_char *image, size_t size,
                          int need_fb_pixel) {
  if (display_lock()) {
    memcpy(get_fb(x, y), image, size);
  }
}

void ui_display_copy_lines(ui_display_t *disp, int src_x, int src_y, int dst_x, int dst_y,
                           u_int width, u_int height) {
  if (display_lock()) {
    u_char *src;
    u_char *dst;
    u_int copy_len;
    u_int count;
    size_t line_length;

    copy_len = width * _display.bytes_per_pixel;
    line_length = _display.buf.stride * _display.bytes_per_pixel;

    if (src_y <= dst_y) {
      src = get_fb(src_x, src_y + height - 1);
      dst = get_fb(dst_x, dst_y + height - 1);

      if (src_y == dst_y) {
        for (count = 0; count < height; count++) {
          memmove(dst, src, copy_len);
          dst -= line_length;
          src -= line_length;
        }
      } else {
        for (count = 0; count < height; count++) {
          memcpy(dst, src, copy_len);
          dst -= line_length;
          src -= line_length;
        }
      }
    } else {
      src = get_fb(src_x, src_y);
      dst = get_fb(dst_x, dst_y);

      for (count = 0; count < height; count++) {
        memcpy(dst, src, copy_len);
        dst += line_length;
        src += line_length;
      }
    }
  }
}

u_char *ui_display_get_bitmap(char *path, u_int *width, u_int *height) {
  JNIEnv *env;
  jobject this;
  jintArray jarray;
  char *image;

  if (!(env = get_jni_env(_display.app->activity->vm))) {
    return NULL;
  }

  this = _display.app->activity->clazz;

  image = NULL;
  if ((jarray = (*env)->CallObjectMethod(
           env, this, (*env)->GetMethodID(env, (*env)->GetObjectClass(env, this), "getBitmap",
                                          "(Ljava/lang/String;II)[I"),
           (*env)->NewStringUTF(env, path), *width, *height))) {
    jint len;

    len = (*env)->GetArrayLength(env, jarray);

    if ((image = malloc((len - 2) * sizeof(u_int32_t)))) {
      jint *elems;

      elems = (*env)->GetIntArrayElements(env, jarray, NULL);

      *width = elems[len - 2];
      *height = elems[len - 1];
      memcpy(image, elems, (len - 2) * sizeof(u_int32_t));

      (*env)->ReleaseIntArrayElements(env, jarray, elems, 0);
    }
  }

  return image;
}

void ui_display_request_text_selection(void) {
  JNIEnv *env;
  jobject this;

  if (!(env = get_jni_env(_display.app->activity->vm))) {
    return;
  }

  this = _display.app->activity->clazz;
  (*env)->CallVoidMethod(env, this, (*env)->GetMethodID(env, (*env)->GetObjectClass(env, this),
                                                        "getTextFromClipboard", "()V"));
}

void ui_display_send_text_selection(u_char *sel_data, size_t sel_len) {
  u_char *p;
  JNIEnv *env;
  jobject this;
  jstring str;

  if (!(p = alloca(sel_len + 1))) {
    return;
  }

  sel_data = memcpy(p, sel_data, sel_len);
  sel_data[sel_len] = '\0';

  if (!(env = get_jni_env(_display.app->activity->vm))) {
    return;
  }

  this = _display.app->activity->clazz;
  (*env)->CallVoidMethod(env, this,
                         (*env)->GetMethodID(env, (*env)->GetObjectClass(env, this),
                                             "setTextToClipboard", "(Ljava/lang/String;)V"),
                         (*env)->NewStringUTF(env, sel_data));
}

void ui_display_show_dialog(char *server, char *privkey) {
  char *user;
  char *host;
  char *port;
  char *encoding;
  JNIEnv *env;
  jobject this;

  if (!(env = get_jni_env(_display.app->activity->vm))) {
    return;
  }

  if (server == NULL ||
      !bl_parse_uri(NULL, &user, &host, &port, NULL, &encoding, bl_str_alloca_dup(server))) {
    user = host = port = encoding = NULL;
  }

  if (!privkey) {
    char *home;

    if ((home = bl_get_home_dir()) && (privkey = alloca(strlen(home) + 15 + 1))) {
      sprintf(privkey, "%s/.mlterm/id_rsa", home);
    }
  }

  this = _display.app->activity->clazz;
  (*env)->CallVoidMethod(env, this,
                         (*env)->GetMethodID(env, (*env)->GetObjectClass(env, this),
                                             "showConnectDialog",
                                             "(Ljava/lang/String;Ljava/lang/String;"
                                             "Ljava/lang/String;Ljava/lang/String;"
                                             "Ljava/lang/String;)V"),
                         user ? (*env)->NewStringUTF(env, user) : NULL,
                         host ? (*env)->NewStringUTF(env, host) : NULL,
                         port ? (*env)->NewStringUTF(env, port) : NULL,
                         encoding ? (*env)->NewStringUTF(env, encoding) : NULL,
                         privkey ? (*env)->NewStringUTF(env, privkey) : NULL);
}

/* Called in the main thread (not in the native activity thread) */

void ui_display_resize(int yoffset, int width, int height,
                       int (*need_resize)(u_int, u_int, u_int, u_int)) {
  u_int new_width;
  u_int new_height;

#ifdef DEBUG
  bl_debug_printf("Visible frame changed yoff %d w %d h %d => yoff %d w %d h %d\n",
                  _display.yoffset, _disp.width, _disp.height, yoffset, width, height);
#endif

  if (width < 50 || height < 50) {
    /* Don't resize because it may be impossible to show any characters. */
    return;
  }

  _display.yoffset = yoffset;

  if (_disp.num_roots == 0) {
    _disp.width = width;
    _disp.height = height;

    return;
  }

  new_width = width;
  new_height = height;

  if (need_resize && !(*need_resize)(_disp.width, _disp.height, new_width, new_height)) {
    return;
  }

  _disp.width = new_width;
  _disp.height = new_height;

  ui_window_resize_with_margin(_disp.roots[0], _disp.width, _disp.height, NOTIFY_TO_MYSELF);
}

/* Called in the main thread (not in the native activity thread) */

void ui_display_update_all(void) {
  if (_disp.num_roots > 0) {
    ui_window_update_all(_disp.roots[0]);
  }
}

void ui_window_set_mapped_flag(ui_window_t *win, int flag) {
  u_int count;

  if (flag == win->is_mapped) {
    return;
  }

  for (count = 0; count < win->num_children; count++) {
    ui_window_set_mapped_flag(win->children[count], flag);
  }

  if (!flag) {
    win->saved_mapped = win->is_mapped;
    win->is_mapped = 0;
  } else {
    win->is_mapped = win->saved_mapped;
  }
}

jstring Java_mlterm_native_1activity_MLActivity_convertToTmpPath(
    JNIEnv *env, jobject this,
    jstring jstr /* must be original URL. (Don't specify /sdcard/.mlterm/anim*) */
    ) {
  char *dst_path;
  char *dir;

  if (!(dir = bl_get_user_rc_path("mlterm/"))) {
    return NULL;
  }

  if (!(dst_path = alloca(strlen(dir) + 8 + 5 /* hash <= 65535 */ + 1))) {
    jstr = NULL;
  } else {
    const char *src_path;
    jint hash;

    src_path = (*env)->GetStringUTFChars(env, jstr, NULL);
    hash = hash_path(src_path);
    (*env)->ReleaseStringUTFChars(env, jstr, src_path);

    sprintf(dst_path, "%sanim%d.gif", dir, hash);
    jstr = ((*env)->NewStringUTF)(env, dst_path);
  }

  free(dir);

  return jstr;
}

void Java_mlterm_native_1activity_MLActivity_splitAnimationGif(
    JNIEnv *env, jobject this,
    jstring jstr /* must be original URL. (Don't specify /sdcard/.mlterm/anim*) */
    ) {
  const char *str;
  const char *path;
  char *dir;
  char *tmp;
  int hash;

  if (!(dir = bl_get_user_rc_path("mlterm/"))) {
    return;
  }

  path = str = (*env)->GetStringUTFChars(env, jstr, NULL);
  hash = hash_path(path);

  if (strstr(path, "://") && (tmp = alloca(strlen(dir) + 8 + 5 /* hash <= 65535 */ + 1))) {
    sprintf(tmp, "%sanim%d.gif", dir, hash);
    path = tmp;
  }

  split_animation_gif(path, dir, hash);

  free(dir);

  (*env)->ReleaseStringUTFChars(env, jstr, str);
}
