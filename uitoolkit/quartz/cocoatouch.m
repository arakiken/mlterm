/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#import <UIKit/UIKit.h>

#include "ui_window.h"
#include "ui_scrollbar.h"

#include <pobl/bl_privilege.h>
#include <pobl/bl_unistd.h> /* bl_getuid/bl_getgid */
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h>  /* bl_compare_str */
#include "../ui_screen_manager.h"
#include "../ui_event_source.h"
#include "../ui_selection_encoding.h"

@interface Application : UIApplication
@end

@interface AppDelegate : NSObject <UIApplicationDelegate> {
  UIWindow *window;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@end

@interface TextPosition : UITextPosition {
  int position;
}
@property (nonatomic) int position;
@end

@interface TextRange : UITextRange {
  TextPosition *start;
  TextPosition *end;
}
@property (nonatomic, readonly) UITextPosition *start;
@property (nonatomic, readonly) UITextPosition *end;
@end

@interface MLTermView : UIView<UITextInput> {
  ui_window_t *uiwindow;
  CGContextRef ctx;
  CGLayerRef layer;
  int forceExpose; /* 2 = visual bell */

  BOOL ignoreKeyDown;
  NSString *markedText;
  TextRange *selectedTextRange;
  TextRange *markedTextRange;

  int cand_x;
  int cand_y;

  NSArray *cmds;
}

@property (readwrite, copy) UITextRange *selectedTextRange;
@property (nonatomic, readonly) UITextRange *markedTextRange;

- (void)drawString:(ui_font_t *)font
                  :(ui_color_t *)fg_color
                  :(int)x
                  :(int)y
                  :(u_char *)str
                  :(size_t)len;
- (void)drawString16:(ui_font_t *)font
                    :(ui_color_t *)fg_color
                    :(int)x
                    :(int)y
                    :(XChar2b *)str
                    :(size_t)len;
- (void)fillWith:(ui_color_t *)color:(int)x:(int)y:(u_int)width:(u_int)height;
- (void)drawRectFrame:(ui_color_t *)color:(int)x1:(int)y1:(int)x2:(int)y2;
- (void)copyArea:(Pixmap)src
                :(int)src_x
                :(int)src_y
                :(u_int)width
                :(u_int)height
                :(int)dst_x
                :(int)dst_y;
#if 0
- (void)scroll:(int)src_x:(int)src_y:(u_int)width:(u_int)height:(int)dst_x:(int)dst_y;
#endif
- (void)setClip:(int)x:(int)y:(u_int)width:(u_int)height;
- (void)unsetClip;
- (void)update:(int)flag;
- (void)bgColorChanged;
@end

/* --- static variables --- */

static ui_window_t *uiwindow_for_mlterm_view;
static int keyboard_margin;

static u_int key_code;
static u_int key_mod;

/* --- static functions --- */

#define set_fill_color(color)                                            \
  CGContextSetRGBFillColor(ctx, (((color)->pixel >> 16) & 0xff) / 255.0, \
                           (((color)->pixel >> 8) & 0xff) / 255.0,       \
                           ((color)->pixel & 0xff) / 255.0, 1.0);

#if 0
#define IS_OPAQUE                                          \
  ((uiwindow->bg_color.pixel & 0xff000000) == 0xff000000 || \
   ui_window_has_wall_picture(uiwindow))
#else
#define IS_OPAQUE 1
#endif

#ifdef DEBUG
int main_loop_final(void);
#endif

static void exit_program(void) {
#ifdef DEBUG
  main_loop_final();
  bl_alloca_garbage_collect();
  bl_mem_free_all();
  bl_dl_close_all();
#endif
}

static void monitor_pty(void) {
#if 0
  /* normal user (Don't call before UIApplicationMain()) */
  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());
#endif

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      ui_event_source_process();
    });
}

/* Undocumented */
bool CGFontGetGlyphsForUnichars(CGFontRef, unichar[], CGGlyph[], size_t);

static void drawUnistr(CGContextRef ctx, ui_font_t *font, unichar *str,
                       u_int len, int x, int y) {
  CGGlyph glyphs_buf[len];
  CGGlyph *glyphs;

#ifdef USE_OT_LAYOUT
  if (font->use_ot_layout /* && font->otf */) {
    glyphs = str;
  } else
#endif
  {
    glyphs = memset(glyphs_buf, 0, sizeof(CGGlyph) * len);
    CGFontGetGlyphsForUnichars(font->xfont->cg_font, str, glyphs_buf, len);

    for (; len > 0 && glyphs[len - 1] == 0; len--) ;
  }

  CGContextSetFont(ctx, font->xfont->cg_font);

  CGAffineTransform t;

  if (font->xfont->is_italic) {
    CGFloat f = -tanf(-12.0 * acosf(0) / 90);
    t = CGAffineTransformMake(1.0, 0.0, f, 1.0, -y * f, 0.0);
  } else {
    t = CGAffineTransformIdentity;
  }

  u_int width = font->width;

  u_int fontsize = font->xfont->size;
  switch (font->size_attr) {
    case DOUBLE_WIDTH:
      width /= 2;
      x = (x + 1) / 2;
      t = CGAffineTransformScale(t, 2.0, 1.0);
      break;

    case DOUBLE_HEIGHT_TOP:
    case DOUBLE_HEIGHT_BOTTOM:
      fontsize *= 2;
      break;
  }

  CGContextSetTextMatrix(ctx, t);

  CGPoint points[len];

  if (font->is_proportional) {
    int advances[len];
    if (!CGFontGetGlyphAdvances(font->xfont->cg_font, glyphs, len, advances)) {
      return;
    }

    int units = CGFontGetUnitsPerEm(font->xfont->cg_font);
    int cur_x = x;
    u_int count;
    for (count = 0; count < len; count++) {
      points[count] = CGPointMake(cur_x, y);

      if (advances[count] > 0) {
        cur_x += (advances[count] * fontsize / units);
      }
    }
  } else {
    u_int count;

    x += font->x_off;

    for (count = 0; count < len; count++) {
      points[count] = CGPointMake((x + width * count), y);
    }
  }

  CGContextSetFontSize(ctx, fontsize);

  CGContextShowGlyphsAtPositions(ctx, glyphs, points, len);

  if (font->double_draw_gap) {
    int gap = font->double_draw_gap;

    font->double_draw_gap = 0;
    drawUnistr(ctx, font, str, len, x + font->double_draw_gap, y);
    font->double_draw_gap = gap;
  }
}

static void update_ime_text(ui_window_t *uiwindow, const char *preedit_text,
                            const char *cur_preedit_text) {
  (*uiwindow->preedit)(uiwindow, preedit_text, cur_preedit_text);
}

static void show_dialog(const char *msg) {
  if (![NSThread isMainThread]) {
    return;
  }

  NSString *ns_msg = [NSString stringWithCString:msg encoding:NSUTF8StringEncoding];

  UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"Alert"
                                                  message:ns_msg
                                                 delegate:nil
                                        cancelButtonTitle:nil
                                        otherButtonTitles:@"OK", nil];
  [alert autorelease];
  [alert show]; /* XXX This doesn't stop. */
}

static ui_window_t *search_focused_window(ui_window_t *win) {
  u_int count;
  ui_window_t *focused;

  /*
   * *parent* - *child*
   *            ^^^^^^^ => Hit this window instead of the parent window.
   *          - child
   *          - child
   * (**: is_focused == 1)
   */
  for (count = 0; count < win->num_children; count++) {
    if ((focused = search_focused_window(win->children[count]))) {
      return focused;
    }
  }

  if (win->is_focused) {
    return win;
  }

  return NULL;
}

/* --- class --- */

int cocoa_dialog_alert(const char *msg);

@implementation Application

- (void)sendEvent:(UIEvent *)event {
  [super sendEvent:event];

  if ([event respondsToSelector:@selector(_gsEvent)]) {
    u_int32_t *buf = [event performSelector:@selector(_gsEvent)];

    if (buf && buf[2] == 10 /* Event type */) {
      u_int num;
      ui_display_t **disps = ui_get_opened_displays(&num);
      ui_window_t *win = search_focused_window(disps[0]->roots[0]);

      if (win) {
        MLTermView *view = win->my_window;

        if (![view hasText]) {
          key_mod = buf[12];
          key_code = (buf[15] >> 16) & 0xffff;
          [self sendAction:@selector(keyEvent) to:view from:nil forEvent:nil];
        }
      }
    }
  }
}

@end

@implementation AppDelegate

@synthesize window;

#pragma mark -
#pragma mark Application lifecycle

- (BOOL)application:(UIApplication *)application
                   didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  [self.window makeKeyAndVisible];

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(keyboardDidShow:)
                                               name:UIKeyboardDidShowNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(keyboardDidHide:)
                                               name:UIKeyboardDidHideNotification
                                             object:nil];

  CGRect r = [self.window screen].applicationFrame;
	MLTermView *view = [[MLTermView alloc] initWithFrame:CGRectMake(0, 0,
                                                                  r.size.width, r.size.height)];
	[self.window addSubview:view];
	[self.window makeKeyAndVisible];

	return YES;
}

- (void)keyboardDidShow:(NSNotification *)note {
  CGRect r = [[[note userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
  keyboard_margin = r.size.height;
  self.window.frame = self.window.frame; /* call observeValueForKeyPath */
}

- (void)keyboardDidHide:(NSNotification *)note {
  keyboard_margin = 0;
  self.window.frame = self.window.frame;
}

- (void)applicationWillResignActive:(UIApplication *)application {
}


- (void)applicationDidEnterBackground:(UIApplication *)application {
}


- (void)applicationWillEnterForeground:(UIApplication *)application {
}


- (void)applicationDidBecomeActive:(UIApplication *)application {
}


- (void)applicationWillTerminate:(UIApplication *)application {
}

#pragma mark -
#pragma mark Memory management

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application {
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  exit_program();
  [super dealloc];
}

@end

@implementation TextPosition
@synthesize position;
@end

@implementation TextRange
@synthesize start;
@synthesize end;

- (id)init {
  [super init];

  start = [TextPosition alloc];
  end = [TextPosition alloc];

  return self;
}

- (void)dealloc {
  [super dealloc];

  [start release];
  [end release];
}
@end

@implementation MLTermView

@synthesize selectedTextRange;
@synthesize markedTextRange;
@synthesize tokenizer;
@synthesize inputDelegate;
@synthesize endOfDocument;
@synthesize beginningOfDocument;
@synthesize markedTextStyle;

- (id)initWithFrame:(CGRect)frame {
  if (uiwindow_for_mlterm_view) {
    uiwindow = uiwindow_for_mlterm_view;
  } else {
    char args[] = "mlclient";
    ui_mlclient(args, NULL);

    ui_screen_t **screens;
    u_int num = ui_get_all_screens(&screens);
    if (num == 0) {
      cocoa_dialog_alert("Failed to open screen");
      exit(1);
    }

    uiwindow = &screens[num - 1]->window;
  }

  uiwindow->my_window = (UIView *)self;
  forceExpose = 1;

  self.clearsContextBeforeDrawing = NO;

  [super initWithFrame:frame];

  ignoreKeyDown = FALSE;
  markedText = nil;
  markedTextRange = [UITextRange alloc];
  selectedTextRange = [UITextRange alloc];

  UILongPressGestureRecognizer *longpress =
    [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(longPress:)];
  longpress.minimumPressDuration = 2.0;
  [longpress autorelease];
  [self addGestureRecognizer:longpress];

  if (uiwindow_for_mlterm_view) {
    uiwindow_for_mlterm_view = NULL;
  }

  static int app_init;
  if (!app_init) {
    app_init = 1;
    monitor_pty();
  }

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self.window removeObserver:self forKeyPath:@"frame"];

  [markedTextRange release];
  [selectedTextRange release];

  if (layer) {
    CGLayerRelease(layer);
    layer = nil;
  }

  if (cmds) {
    [cmds release];
  }

  [super dealloc];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(configMenu:) || action == @selector(pasteMenu:) ||
      action == @selector(keyEvent:)) {
    return YES;
  } else {
    return NO;
  }
}

- (void)configMenu:(id)sender {
  cocoa_dialog_alert("Configuration menu is not supported.");
}

- (void)pasteMenu:(id)sender {
  /* if by any change */
  if (((ui_screen_t *)uiwindow)->term) {
    ui_screen_exec_cmd((ui_screen_t *)uiwindow, "paste");
  }
}

- (void)setFrame:(CGRect)r {
  if (layer) {
    CGLayerRelease(layer);
    layer = nil;
  }

  CGRect sr = [self.window screen].applicationFrame;

  r.origin.x += sr.origin.x;
  r.origin.y += sr.origin.y;

#if 0
  NSLog(@"setFrame %@ %f %f %f %f", self, r.origin.x, r.origin.y, r.size.width, r.size.height);
#endif

  [super setFrame:r];
}

- (void)windowResized {
  if (!uiwindow->parent || !((ui_screen_t *)uiwindow)->term) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  CGRect sr = [self.window screen].applicationFrame;

  uiwindow->parent->width = sr.size.width - uiwindow->parent->hmargin * 2;
  uiwindow->parent->height = sr.size.height - uiwindow->parent->vmargin * 2 - keyboard_margin;

  (*uiwindow->parent->window_resized)(uiwindow->parent);

  u_int count;
  for (count = 0; count < uiwindow->num_children; count++) {
    ui_window_t *child = uiwindow->children[count];

    if (child->my_window) {
      [((UIView *)child->my_window) setFrame:CGRectMake(child->x, child->y,
                                                        ACTUAL_WIDTH(child), ACTUAL_HEIGHT(child))];
    }
  }
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context {
  [self windowResized];
}

- (void)didMoveToWindow {
  if ([self window] == nil) {
    /* just before being deallocated */
    return;
  }

#if 0
  if (!uiwindow->parent->my_window) {
    [[self window] orderOut:self];

    if (uiwindow->event_mask & PointerMotionMask) {
      [self window].acceptsMouseMovedEvents = YES;
    }
  }
#endif

  [self.window addObserver:self forKeyPath:@"frame" options:NSKeyValueObservingOptionNew
               context:nil];

  /* Change view size */

  if (!uiwindow->parent->my_window) {
    CGRect sr = [self.window screen].applicationFrame;

    uiwindow->disp->width = sr.size.width;
    uiwindow->disp->height = sr.size.height;

#if 0
    [[self window] useOptimizedDrawing:YES];
#endif

    uiwindow->parent->my_window = [self window];

    if (!IS_OPAQUE) {
      [self bgColorChanged];
    }

    /* XXX TODO: Support color change */
    u_long pixel = uiwindow->fg_color.pixel; /* See window_exposed() in ui_layout.c */
    self.window.backgroundColor = [UIColor colorWithRed:((pixel >> 16) & 0xff) / 255.0
                                                  green:((pixel >> 8) & 0xff) / 255.0
                                                   blue:(pixel & 0xff) / 255.0
                                                  alpha:((pixel >> 24) & 0xff) / 255.0];
  }

  [self windowResized];
  [self becomeFirstResponder];
}

- (void)drawRect:(CGRect)rect {
  if (!uiwindow->parent || !((ui_screen_t *)uiwindow)->term) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  XExposeEvent ev;

  CGContextRef screen_ctx = UIGraphicsGetCurrentContext();
  CGContextSaveGState(screen_ctx);
  CGContextTranslateCTM(screen_ctx, 0.0, self.bounds.size.height);
  CGContextScaleCTM(screen_ctx, 1.0, -1.0);
  CGContextSetBlendMode(screen_ctx, kCGBlendModeCopy);

#if 0
  CGAffineTransform t = CGContextGetCTM(ctx);
  bl_debug_printf("%f %f %f %f %f %f\n", t.a, t.b, t.c, t.d, t.tx, t.ty);
#endif

  if (!layer) {
    layer = CGLayerCreateWithContext(screen_ctx, self.bounds.size, NULL);
    ctx = CGLayerGetContext(layer);
    CGContextSetBlendMode(ctx, kCGBlendModeCopy);

    if (uiwindow->update_window_flag == 0) {
      uiwindow->update_window_flag = 3; /* UPDATE_SCREEN|UPDATE_CURSOR (ui_screen.c) */
    }
    forceExpose = 1;
  }

  CGPoint p = CGPointMake(0, 0);

  if (forceExpose & 2) {
    /* Visual bell */
    [self fillWith:&uiwindow->fg_color
                  :uiwindow->hmargin
                  :uiwindow->vmargin
                  :uiwindow->width
                  :uiwindow->height];
    CGContextFlush(ctx);
    CGContextDrawLayerAtPoint(screen_ctx, p, layer);

    [[NSRunLoop currentRunLoop]
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

    forceExpose &= ~2;
    uiwindow->update_window_flag = 0;
  }

  ev.type = UI_EXPOSE;
  ev.x = rect.origin.x;
  ev.width = rect.size.width;
  ev.height = rect.size.height;
  ev.y = ACTUAL_HEIGHT(uiwindow) - rect.origin.y - ev.height;
  ev.force_expose = forceExpose;

  ui_window_receive_event(uiwindow, (XEvent *)&ev);

  forceExpose = 0;
  uiwindow->update_window_flag = 0;

  CGContextDrawLayerAtPoint(screen_ctx, p, layer);

  CGContextRestoreGState(screen_ctx);
}

- (BOOL)isOpaque {
  return IS_OPAQUE ? YES : NO;
}

- (BOOL)wantsDefaultClipping {
  return IS_OPAQUE ? YES : NO;
}

static ui_window_t *get_current_window(ui_window_t *win) {
  u_int count;

  if (win->inputtable > 0) {
    return win;
  }

  for (count = 0; count < win->num_children; count++) {
    ui_window_t *hit;

    if ((hit = get_current_window(win->children[count]))) {
      return hit;
    }
  }

  return NULL;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)becomeFirstResponder {
  if ([super becomeFirstResponder] != YES) {
    return NO;
  }

  [self.window bringSubviewToFront:self];

  XEvent ev;
  ev.type = UI_KEY_FOCUS_IN;

  ui_window_receive_event(uiwindow, &ev);

  return YES;
}

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
  UITouch *touch = [touches anyObject];
  CGPoint loc = [touch locationInView:self];

  XButtonEvent bev;
  bev.type = UI_BUTTON_PRESS;
  bev.time = touch.timestamp * 1000;
  bev.x = loc.x;
  bev.y = loc.y;
  bev.state = 0;
  bev.button = 1;
  bev.click_count = touch.tapCount;

  ui_window_receive_event(uiwindow, (XEvent *)&bev);

  if (!uiwindow->is_focused || keyboard_margin == 0) {
    [self becomeFirstResponder];
  }
}

- (void)longPress:(id)sender {
  UIMenuController *menuctl = [UIMenuController sharedMenuController];
  CGPoint loc = [sender locationOfTouch:0 inView:self];
  [menuctl setTargetRect:CGRectMake(loc.x, loc.y, 0, 0) inView:self];
  menuctl.arrowDirection = UIMenuControllerArrowDown;

  NSMutableArray *items = [NSMutableArray array];
  UIMenuItem *item;
  item = [[[UIMenuItem alloc] initWithTitle:@"Paste" action:@selector(pasteMenu:)] autorelease];
  [items addObject:item];
  item = [[[UIMenuItem alloc] initWithTitle:@"Config" action:@selector(configMenu:)] autorelease];
  [items addObject:item];
  menuctl.menuItems = items;
  [menuctl setMenuVisible:NO animated:NO];
  [menuctl setMenuVisible:YES animated:YES];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
  UITouch *touch = [touches anyObject];
  CGPoint loc = [touch locationInView:self];

  XButtonEvent bev;
  bev.type = UI_BUTTON_RELEASE;
  bev.time = touch.timestamp * 1000;
  bev.x = loc.x;
  bev.y = loc.y;
  bev.state = 0;
  bev.button = 1;
  bev.click_count = touch.tapCount;

  ui_window_receive_event(uiwindow, (XEvent *)&bev);
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event {
  UITouch *touch = [touches anyObject];
  CGPoint loc = [touch locationInView:self];

  XMotionEvent mev;
  mev.type = UI_BUTTON_MOTION;
  mev.time = touch.timestamp * 1000;
  mev.x = loc.x;
  mev.y = loc.y;
  mev.state = Button1Mask;

  ui_window_receive_event(uiwindow, (XEvent *)&mev);
}

#if 0
- (void)scrollWheel:(NSEvent *)event {
  NSPoint loc = [event locationInWindow];
  XButtonEvent bevPress;
  XButtonEvent bevRelease;

  bevPress.type = UI_BUTTON_PRESS;
  bevRelease.type = UI_BUTTON_RELEASE;

  bevPress.time = event.timestamp * 1000;
  bevRelease.time = (event.timestamp * 1000) + 1;

  bevPress.x = loc.x - self.frame.origin.x;
  bevRelease.x = loc.x - self.frame.origin.x;

  bevPress.y =
      ACTUAL_HEIGHT(uiwindow->parent) - loc.y - /* self.frame.origin.y - */ 1;
  bevRelease.y =
      ACTUAL_HEIGHT(uiwindow->parent) - loc.y - /* self.frame.origin.y - */ 1;

  bevPress.state = event.modifierFlags &
                   (NSShiftKeyMask | NSControlKeyMask | NSAlternateKeyMask);
  bevRelease.state = event.modifierFlags &
                     (NSShiftKeyMask | NSControlKeyMask | NSAlternateKeyMask);

  bevPress.click_count = 1;

  if (event.deltaY > 1) {
    bevPress.button = 4;
    bevRelease.button = 4;
  }

  if (event.deltaY < -1) {
    bevPress.button = 5;
    bevRelease.button = 5;
  }

  if (event.deltaY < -1 || event.deltaY > 1) {
    ui_window_receive_event(uiwindow, (XEvent *)&bevPress);
    ui_window_receive_event(uiwindow, (XEvent *)&bevRelease);
  }
}
#endif

- (void)keyEvent {
  XKeyEvent kev;

#if 0
  NSLog(@"Key event: mod %x keycode %x", key_mod, key_code);
#endif

  if (0xf700 <= key_code && key_code <= 0xf8ff) {
    /* do nothing (Function keys) */
  } else if (key_code == 0x1b) {
    /* do nothing */
  } else if ((key_mod & NSControlKeyMask) || (key_mod & NSCommandKeyMask)) {
    if ('a' <= key_code && key_code <= 'z') {
      key_code -= 0x60;
    } else if (0x40 <= key_code && key_code < 0x60) {
      key_code -= 0x40;
    } else {
      return;
    }
  } else {
    return;
  }

  kev.type = UI_KEY_PRESS;
  kev.state = key_mod;
  kev.keysym = key_code;
  kev.utf8 = NULL;
  ui_window_receive_event(uiwindow, (XEvent *)&kev);
}

#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 70000 /* iOS 7.0 or later */
- (NSArray *)keyCommands {
  if (!cmds) {
    UIKeyModifierFlags flags[] = {
      0,
      UIKeyModifierShift,
      UIKeyModifierShift | UIKeyModifierControl,
      UIKeyModifierShift | UIKeyModifierAlternate,
      UIKeyModifierShift | UIKeyModifierCommand,
      UIKeyModifierShift | UIKeyModifierControl | UIKeyModifierAlternate,
      UIKeyModifierShift | UIKeyModifierControl | UIKeyModifierAlternate | UIKeyModifierCommand,
      UIKeyModifierShift | UIKeyModifierControl | UIKeyModifierCommand,
      UIKeyModifierShift | UIKeyModifierAlternate | UIKeyModifierCommand,
      UIKeyModifierShift | UIKeyModifierCommand,
      UIKeyModifierControl,
      UIKeyModifierControl | UIKeyModifierAlternate,
      UIKeyModifierControl | UIKeyModifierAlternate | UIKeyModifierCommand,
      UIKeyModifierControl | UIKeyModifierCommand,
      UIKeyModifierAlternate,
      UIKeyModifierAlternate | UIKeyModifierCommand,
      UIKeyModifierCommand };
    NSMutableArray *mutable = [[NSMutableArray alloc] init];
    u_char c[] = "\x60";
    size_t count;

    for (; c[0] < 0x80; c[0]++) {
      [mutable addObject:[UIKeyCommand keyCommandWithInput:[NSString stringWithUTF8String:c]
                                             modifierFlags:UIKeyModifierControl
                                                    action:@selector(keyEvent:)]];
    }

    for (count = 0; count < sizeof(flags) / sizeof(flags[0]); count++) {
      [mutable addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow
                                             modifierFlags:flags[count]
                                                    action:@selector(keyEvent:)]];
      [mutable addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow
                                             modifierFlags:flags[count]
                                                    action:@selector(keyEvent:)]];
      [mutable addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow
                                             modifierFlags:flags[count]
                                                    action:@selector(keyEvent:)]];
      [mutable addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow
                                             modifierFlags:flags[count]
                                                    action:@selector(keyEvent:)]];
      [mutable addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputEscape
                                             modifierFlags:flags[count]
                                                    action:@selector(keyEvent:)]];

      [mutable addObject:[UIKeyCommand keyCommandWithInput:@"\x01"
                                             modifierFlags:flags[count]
                                                    action:@selector(keyEvent:)]];
      [mutable addObject:[UIKeyCommand keyCommandWithInput:@"\x04"
                                             modifierFlags:flags[count]
                                                    action:@selector(keyEvent:)]];
      [mutable addObject:[UIKeyCommand keyCommandWithInput:@"\x0b"
                                             modifierFlags:flags[count]
                                                    action:@selector(keyEvent:)]];
      [mutable addObject:[UIKeyCommand keyCommandWithInput:@"\x0c"
                                             modifierFlags:flags[count]
                                                    action:@selector(keyEvent:)]];
      [mutable addObject:[UIKeyCommand keyCommandWithInput:@"\x10"
                                             modifierFlags:flags[count]
                                                    action:@selector(keyEvent:)]];
     }

    cmds = mutable;
  }

  return cmds;
}

- (void)keyEvent:(UIKeyCommand *)keyCommand {
  key_mod = keyCommand.modifierFlags;

#if 0
  NSLog(@"KeyCmd: %x %x\n", key_mod, [keyCommand.input characterAtIndex:0]);
  NSLog(keyCommand.input);
#endif

  if ([keyCommand.input compare:UIKeyInputUpArrow] == NSOrderedSame) {
    key_code = NSUpArrowFunctionKey;
  } else if ([keyCommand.input compare:UIKeyInputDownArrow] == NSOrderedSame) {
    key_code = NSDownArrowFunctionKey;
  } else if ([keyCommand.input compare:UIKeyInputLeftArrow] == NSOrderedSame) {
    key_code = NSLeftArrowFunctionKey;
  } else if ([keyCommand.input compare:UIKeyInputRightArrow] == NSOrderedSame) {
    key_code = NSRightArrowFunctionKey;
  } else if ([keyCommand.input compare:UIKeyInputEscape] == NSOrderedSame) {
    key_code = 0x1b;
  } else {
    key_code = [keyCommand.input characterAtIndex:0];

    switch (key_code) {
    case 0x1:
      key_code = NSHomeFunctionKey;
      break;

    case 0x4:
      key_code = NSEndFunctionKey;
      break;

    case 0xb:
      key_code = NSPageUpFunctionKey;
      break;

    case 0xc:
      key_code = NSPageDownFunctionKey;
      break;

    case 0x10:
      /* iOS sends 0x10 for all function keys. It's impossible to distinguish them. */
      key_code = NSF1FunctionKey;
      break;

    default:
      break;
    }
  }

  [self keyEvent];
}
#endif

- (UITextRange *)characterRangeAtPoint:(CGPoint)point {
  return nil;
}

- (UITextPosition *)closestPositionToPoint:(CGPoint)point {
  return nil;
}

- (UITextPosition *)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange *)range {
  return nil;
}

- (CGRect)firstRectForRange:(UITextRange *)range {
#if 0
  int x = cand_x + uiwindow->x + uiwindow->hmargin;
  int y = ACTUAL_HEIGHT(uiwindow->parent) - (cand_y + uiwindow->y + uiwindow->vmargin);

  if (vt_term_get_vertical_mode(((ui_screen_t*)uiwindow)->term)) {
    /*
     * XXX Adjust candidate window position.
     *
     * +-+-+------
     * | |1|ABCDE
     *
     * <-->^
     *  25 x
     */
    x += 25;
  }

  CGRect r = CGRectMake(x, y, ui_col_width((ui_screen_t *)uiwindow),
                        ui_line_height((ui_screen_t *)uiwindow));
#endif

  return CGRectMake(0, 0, 1, 1);
}

- (CGRect)caretRectForPosition:(UITextPosition *)position {
  return CGRectMake(0, 0, 1, 1);
}

- (void)setBaseWritingDirection:(UITextWritingDirection)writingDirection
                       forRange:(UITextRange *)range {
}

- (UITextWritingDirection)baseWritingDirectionForPosition:(UITextPosition *)position
                                              inDirection:(UITextStorageDirection)direction {
  return UITextWritingDirectionLeftToRight;
}

- (UITextRange *)characterRangeByExtendingPosition:(UITextPosition *)position
                                       inDirection:(UITextLayoutDirection)direction {
  return nil;
}

- (UITextPosition *)positionWithinRange:(UITextRange *)range
                    farthestInDirection:(UITextLayoutDirection)direction {
  return nil;
}

- (NSInteger)offsetFromPosition:(UITextPosition *)from
                     toPosition:(UITextPosition *)toPosition {
  return ((TextPosition*)toPosition).position - ((TextPosition*)from).position;
}

- (NSComparisonResult)comparePosition:(UITextPosition *)position
                           toPosition:(UITextPosition *)other {
  int p = ((TextPosition *)position).position;
  int o = ((TextPosition *)other).position;

  if (p < o) {
    return NSOrderedAscending;
  } else if (p > 0) {
    return NSOrderedDescending;
  } else {
    return NSOrderedSame;
  }
}

- (UITextPosition *)positionFromPosition:(UITextPosition *)position
                                  offset:(NSInteger)offset {
  TextPosition *pos = [[TextPosition alloc] autorelease];
  pos.position = ((TextPosition *)position).position + offset;

  return pos;
}

- (UITextPosition *)positionFromPosition:(UITextPosition *)position
                             inDirection:(UITextLayoutDirection)direction
                                  offset:(NSInteger)offset {
  return nil;
}

- (UITextRange *)textRangeFromPosition:(UITextPosition *)fromPosition
                            toPosition:(UITextPosition *)toPosition {
  TextRange *range = [[TextRange alloc] autorelease];
  ((TextPosition *)range.start).position = ((TextPosition *)fromPosition).position;
  ((TextPosition *)range.end).position = ((TextPosition *)toPosition).position;

  return range;
}

- (void)showMarkedText:(id)string
         selectedRange:(NSRange)selectedRange {
  if ([string length] > 0) {
    char *p;

    if (!(p = alloca(strlen([string UTF8String]) + 10))) {
      return;
    }
    *p = '\0';

    if (selectedRange.location > 0) {
      strcpy(p, [[string substringWithRange:
                           NSMakeRange(0, selectedRange.location)] UTF8String]);
    }

    if (selectedRange.length > 0) {
      sprintf(p + strlen(p), "\x1b[7m%s\x1b[27m",
              [[string substringWithRange:selectedRange] UTF8String]);
    }

    if (selectedRange.location + selectedRange.length < [string length]) {
      strcat(p, [[string substringWithRange:
                             NSMakeRange(
                                 selectedRange.location + selectedRange.length,
                                 [string length] - selectedRange.location -
                                     selectedRange.length)] UTF8String]);
    }

    if (!markedText) {
      if (!uiwindow->xim_listener ||
          !(*uiwindow->xim_listener->get_spot)(uiwindow->xim_listener->self, &cand_x, &cand_y)) {
        cand_x = cand_y = 0;
      }
    }

    update_ime_text(uiwindow, p, markedText ? markedText.UTF8String : NULL);
  } else if (markedText) {
    update_ime_text(uiwindow, "", markedText.UTF8String);
  }
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange {
  if ([string isKindOfClass:[NSAttributedString class]]) {
    string = [string string];
  }

  ((TextPosition *)selectedTextRange.start).position = 0;
  ((TextPosition *)selectedTextRange.end).position = selectedRange.location +
                                                     selectedRange.length;

  [self showMarkedText:string selectedRange:selectedRange];

  if (markedText) {
    [markedText release];
    markedText = nil;
  }

  if ([string length] > 0) {
    markedText = [string copy];
    ((TextPosition *)markedTextRange.end).position = [string length];
  } else {
    ((TextPosition *)markedTextRange.end).position = 0;
  }
  ((TextPosition *)markedTextRange.start).position = 0;
}

- (void)unmarkText {
  if (markedText) {
    update_ime_text(uiwindow, "", NULL);
    [self insertText:markedText];
    [markedText release];
    markedText = nil;
  }
}

- (void)replaceRange:(UITextRange *)range withText:(NSString *)text {
  if (!markedText) {
    NSRange range = NSMakeRange(0, [text length]);
    [self setMarkedText:text selectedRange:range];

    return;
  }

  int start = ((TextPosition *)range.start).position;
  int length = ((TextPosition *)range.end).position - start;
  NSMutableString *nsstr = [NSMutableString stringWithString:markedText];
  [nsstr replaceCharactersInRange:NSMakeRange(start, length) withString:text];
  [markedText release];
  markedText = [NSString stringWithString:nsstr];
  [nsstr release];
  ((TextPosition *)selectedTextRange.start).position = start;
  ((TextPosition *)selectedTextRange.start).position = start + [text length];

  [self showMarkedText:markedText selectedRange:NSMakeRange(start, [text length])];
}

- (NSString *)textInRange:(UITextRange *)range {
  return markedText;
}

- (BOOL)hasText {
  if (markedText) {
    return YES;
  } else {
    return NO;
  }
}

- (void)insertText:(NSString *)string {
  if ([string length] > 0) {
    unichar c = [string characterAtIndex:0];

    if (0xf700 <= c && c <= 0xf8ff) {
      /* Function keys (See keyEvent or keyEvent:) */
      return;
    }

    XKeyEvent kev;
    kev.type = UI_KEY_PRESS;
    kev.state = 0;
    kev.keysym = c;
    kev.utf8 = [string UTF8String];

    ui_window_receive_event(uiwindow, (XEvent *)&kev);

    ignoreKeyDown = TRUE;
  }
}

- (void)deleteBackward {
    XKeyEvent kev;

    kev.type = UI_KEY_PRESS;
    kev.state = 0;
    kev.keysym = 0x08;
    kev.utf8 = "\x08";

    ui_window_receive_event(uiwindow, (XEvent *)&kev);
}

- (void)drawString:(ui_font_t *)font
                  :(ui_color_t *)fg_color
                  :(int)x
                  :(int)y
                  :(u_char *)str
                  :(size_t)len {
  set_fill_color(fg_color);

#if 0
  u_char *p = alloca(len + 1);
  memcpy(p, str, len);
  p[len] = '\0';
  bl_debug_printf("%d %d %s %x\n", x, y, p, p[len - 1]);
#endif

#if 0
  CGContextSelectFont(ctx, "Menlo", 16, kCGEncodingMacRoman);
  CGContextShowTextAtPoint(ctx, x, ACTUAL_HEIGHT(uiwindow) - y - 1, str, len);
#else
  unichar ustr[len];
  int count;
  for (count = 0; count < len; count++) {
    ustr[count] = str[count];
  }

  drawUnistr(ctx, font, ustr, len, x, ACTUAL_HEIGHT(uiwindow) - y - 1);
#endif
}

- (void)drawString16:(ui_font_t *)font
                    :(ui_color_t *)fg_color
                    :(int)x
                    :(int)y
                    :(XChar2b *)str
                    :(size_t)len {
  set_fill_color(fg_color);

  drawUnistr(ctx, font, (unichar *)str, len, x, ACTUAL_HEIGHT(uiwindow) - y - 1);
}

- (void)fillWith:(ui_color_t *)color:(int)x:(int)y:(u_int)width:(u_int)height {
#if 0
  static int count = 0;
  color->pixel += (0x10 * (count++));
#endif

  if (IS_OPAQUE) {
    set_fill_color(color);

    CGRect rect =
        CGRectMake(x, ACTUAL_HEIGHT(uiwindow) - y - height, width, height);
    CGContextAddRect(ctx, rect);
    CGContextFillPath(ctx);
  } else {
    [[UIColor colorWithRed:((color->pixel >> 16) & 0xff) / 255.0
                     green:((color->pixel >> 8) & 0xff) / 255.0
                      blue:(color->pixel & 0xff) / 255.0
                     alpha:((color->pixel >> 24) & 0xff) / 255.0] set];

#if 0
    NSRectFillUsingOperation(
        NSMakeRect(x, ACTUAL_HEIGHT(uiwindow) - y - height, width, height),
        NSCompositeCopy);
#endif
  }
}

- (void)drawRectFrame:(ui_color_t *)color:(int)x1:(int)y1:(int)x2:(int)y2 {
  set_fill_color(color);

  CGRect rect = CGRectMake(x1, ACTUAL_HEIGHT(uiwindow) - y2 - 1, 1, y2 - y1);
  CGContextAddRect(ctx, rect);
  rect = CGRectMake(x1, ACTUAL_HEIGHT(uiwindow) - y2 - 1, x2 - x1, 1);
  CGContextAddRect(ctx, rect);
  rect = CGRectMake(x2, ACTUAL_HEIGHT(uiwindow) - y2 - 1, 1, y2 - y1);
  CGContextAddRect(ctx, rect);
  rect = CGRectMake(x1, ACTUAL_HEIGHT(uiwindow) - y1 - 1, x2 - x1 + 1, 1);
  CGContextAddRect(ctx, rect);
  CGContextFillPath(ctx);
}

- (void)copyArea:(Pixmap)src
                :(int)src_x
                :(int)src_y
                :(u_int)width
                :(u_int)height
                :(int)dst_x
                :(int)dst_y {
  CGImageRef clipped = CGImageCreateWithImageInRect(
      src, CGRectMake(src_x, src_y, width, height));
  CGContextDrawImage(
      ctx,
      CGRectMake(dst_x, ACTUAL_HEIGHT(uiwindow) - dst_y - height, width, height),
      clipped);
  CGImageRelease(clipped);
}

#if 0
- (void)scroll:(int)src_x:(int)src_y:(u_int)width:(u_int)height:(int)dst_x:(int)dst_y {
  CGContextFlush(ctx);

  /* Don't release this CGImage */
  CGRect src_r = CGRectMake(src_x, ACTUAL_HEIGHT(uiwindow) - src_y - height,
			width, height);
  UIBitmapImageRep *bir = [self bitmapImageRepForCachingDisplayInRect:src_r];
  [self cacheDisplayInRect:src_r toBitmapImageRep:bir];
  CGImageRef image = bir.CGImage;

  CGRect dst_r = CGRectMake(dst_x, ACTUAL_HEIGHT(uiwindow) - dst_y - height,
				width, height);
  CGContextDrawImage(ctx, dst_r, image);

  static int i;
  if (i == 0) {
    CFURLRef url = [UIURL fileURLWithPath:@"/Users/ken/kallist/log.png"];
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(
				   url, kUTTypePNG, 1, NULL);
    CGImageDestinationAddImage(dest, image, nil);
    CGImageDestinationFinalize(dest);
    CFRelease( dest);
    i++;
  }
  else if (i == 1) {
    UIImage *  nsimage = [[[UIImage alloc]initWithSize:src_r.size] autorelease];
    [nsimage addRepresentation:bir];
    [[nsimage TIFFRepresentation] writeToFile:@"/Users/ken/kallist/log.tiff"
                                  atomically:YES];

    i++;
  }
}
#endif

- (void)update:(int)flag {
  forceExpose |= flag;

  if (IS_OPAQUE || forceExpose) {
    [self setNeedsDisplay];
  } else {
    int x;
    int y;

    if (!uiwindow->xim_listener ||
        !(*uiwindow->xim_listener->get_spot)(uiwindow->xim_listener->self, &x, &y)) {
      x = y = 0;
    }

    x += (uiwindow->hmargin);
    y += (uiwindow->vmargin);

    [self setNeedsDisplayInRect:CGRectMake(x, ACTUAL_HEIGHT(uiwindow) - y, 1, 1)];
  }
}

- (void)setClip:(int)x:(int)y:(u_int)width:(u_int)height {
  CGContextSaveGState(ctx);

  y = ACTUAL_HEIGHT(uiwindow) - y;

  CGContextBeginPath(ctx);
  CGContextMoveToPoint(ctx, x, y);
  CGContextAddLineToPoint(ctx, x + width, y);
  CGContextAddLineToPoint(ctx, x + width, y - height);
  CGContextAddLineToPoint(ctx, x, y - height);
  CGContextClosePath(ctx);
  CGContextClip(ctx);
}

- (void)unsetClip {
  CGContextRestoreGState(ctx);
}

- (void)bgColorChanged {
  if (IS_OPAQUE) {
    [[self window] setBackgroundColor:[UIColor whiteColor]];
    [[self window] setOpaque:YES];
  } else {
    [[self window] setBackgroundColor:[UIColor clearColor]];
    [[self window] setOpaque:NO];
  }
}
@end

/* --- global functions --- */

void view_alloc(ui_window_t *uiwindow) {
  uiwindow_for_mlterm_view = uiwindow;

  MLTermView *view =
      [[MLTermView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)];
  [((UIWindow *)uiwindow->parent->my_window) addSubview:view];
}

void view_dealloc(UIView *view) {
  /* removeFromSuperview() can hide keyboard and change the frame of UIWindow. */
  [view.window removeObserver:view forKeyPath:@"frame"];
  [view removeFromSuperview];
  [view release];
}

void view_update(MLTermView *view, int flag) { [view update:flag]; }

void view_set_clip(MLTermView *view, int x, int y, u_int width, u_int height) {
  [view setClip:x:y:width:height];
}

void view_unset_clip(MLTermView *view) { [view unsetClip]; }

void view_draw_string(MLTermView *view, ui_font_t *font, ui_color_t *fg_color,
                      int x, int y, char *str, size_t len) {
  [view drawString:font:fg_color:x:y:str:len];
}

void view_draw_string16(MLTermView *view, ui_font_t *font, ui_color_t *fg_color,
                        int x, int y, XChar2b *str, size_t len) {
  [view drawString16:font:fg_color:x:y:str:len];
}

void view_fill_with(MLTermView *view, ui_color_t *color, int x, int y,
                    u_int width, u_int height) {
  [view fillWith:color:x:y:width:height];
}

void view_draw_rect_frame(MLTermView *view, ui_color_t *color, int x1, int y1,
                          int x2, int y2) {
  [view drawRectFrame:color:x1:y1:x2:y2];
}

void view_copy_area(MLTermView *view, Pixmap src, int src_x, int src_y,
                    u_int width, u_int height, int dst_x, int dst_y) {
  [view copyArea:src:src_x:src_y:width:height:dst_x:dst_y];
}

void view_scroll(MLTermView *view, int src_x, int src_y, u_int width,
                 u_int height, int dst_x, int dst_y) {
#if 0
  [view scroll:src_x:src_y:width:height:dst_x:dst_y];
#endif
}

void view_bg_color_changed(MLTermView *view) { [view bgColorChanged]; }

void view_visual_bell(MLTermView *view) { [view update:2]; }

void view_set_input_focus(UIView *view) {
  [view becomeFirstResponder];
}

void view_set_rect(UIView *view, int x, int y, /* The origin is left-botom. */
                   u_int width, u_int height) {
  [view setFrame:CGRectMake(x,y,width,height)];
}

void view_set_hidden(UIView *view, int flag) { [view setHidden:flag]; }

void window_alloc(ui_window_t *root) {
  uiwindow_for_mlterm_view = root->children[1];

  UINib *nib = [[UINib alloc] nibWithNibName:@"Main" bundle:nil];
  [nib instantiateWithOwner:nil options:nil];
  [nib release];
}

void window_dealloc(UIWindow *window) { [window release]; }

void window_resize(UIWindow *window, int width, int height) {}

void window_move_resize(UIWindow *window, int x, int y, int width, int height) {}

void window_accepts_mouse_moved_events(UIWindow *window, int accept) {
#if 0
  window.acceptsMouseMovedEvents = (accept ? YES : NO);
#endif
}

void window_set_normal_hints(UIWindow *window, u_int width_inc,
                             u_int height_inc) {
#if 0
  [window setResizeIncrements:NSMakeSize(width_inc, height_inc)];
#endif
}

void window_get_position(UIWindow *window, int *x, int *y) {
  *x = 0;
  *y = 0;
}

void window_set_title(UIWindow *window, const char *title /* utf8 */) {
#if 0
  NSString *ns_title = [NSString stringWithCString:title encoding:NSUTF8StringEncoding];
  [window setTitle:ns_title];
#endif
}

void app_urgent_bell(int on) {
#if 0
  if (on) {
    [[NSApplication sharedApplication] requestUserAttention:NSCriticalRequest];
  } else {
    [[NSApplication sharedApplication]
        cancelUserAttentionRequest:NSCriticalRequest];
  }
#endif
}

void scroller_update(UIScrollView *scroller, float pos, float knob) {
#if 0
#if 1
  [scroller setFloatValue:pos knobProportion:knob]; /* Deprecated since 10.6 */
#else
  scroller.knobProportion = knob;
  scroller.doubleValue = pos;
#endif
#endif
}

CGFontRef cocoa_create_font(const char *font_family) {
  NSString *ns_font_family =
      [NSString stringWithCString:font_family encoding:NSUTF8StringEncoding];

  return CGFontCreateWithFontName(ns_font_family);
}

#ifdef USE_OT_LAYOUT
char *cocoa_get_font_path(CGFontRef cg_font) {
#if 0
  CTFontDescriptorRef desc =
      CTFontDescriptorCreateWithNameAndSize(CGFontCopyFullName(cg_font), 14);
  CFURLRef url =
      (CFURLRef)CTFontDescriptorCopyAttribute(desc, kCTFontURLAttribute);
  const char *urlstr = [((NSString *)CFURLGetString(url))UTF8String];
  if (strncmp(urlstr, "file://localhost", 16) == 0) {
    urlstr += 16;
  }

  char *path = strdup(urlstr);

  CFRelease(url);
  CFRelease(desc);

  return path;
#else
  return NULL;
#endif
}
#endif /* USE_OT_LAYOUT */

void cocoa_release_font(CGFontRef cg_font) { CFRelease(cg_font); }

u_int cocoa_font_get_advance(CGFontRef cg_font, u_int fontsize, int size_attr,
                             unichar *utf16, u_int len, CGGlyph glyph) {
  if (utf16) {
    CGFontGetGlyphsForUnichars(cg_font, &utf16, &glyph, 1);
  }

  int advance;
  if (!CGFontGetGlyphAdvances(cg_font, &glyph, 1, &advance) || advance < 0) {
    return 0;
  }

  if (size_attr >= DOUBLE_HEIGHT_TOP) {
    fontsize *= 2;
  }

  return advance * fontsize / CGFontGetUnitsPerEm(cg_font);
}

void cocoa_clipboard_own(MLTermView *view) {
#if 0
  UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
  [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString]
                     owner:view];
#endif
}

void cocoa_clipboard_set(const u_char *utf8, size_t len) {
  UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
  NSString *str = [[NSString alloc] initWithBytes:utf8
                                           length:len
                                         encoding:NSUTF8StringEncoding];
  pasteboard.string = str;
  [str release];
}

const char *cocoa_clipboard_get(void) {
  UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];

  NSString *str = pasteboard.string;
  if (str != nil) {
    return [str UTF8String];
  }

  return NULL;
}

void cocoa_beep(void) {}

CGImageRef cocoa_load_image(const char *path, u_int *width, u_int *height) {
  NSString *nspath =
      [NSString stringWithCString:path encoding:NSUTF8StringEncoding];
  UIImage *uiimg = [[UIImage alloc] initWithContentsOfFile:nspath];
  if (!uiimg) {
    return nil;
  }

  CGImageRef cgimg = uiimg.CGImage;

  CGImageRetain(cgimg);

  *width = uiimg.size.width;
  *height = uiimg.size.height;
  [uiimg release];

  return cgimg;
}

const char *cocoa_get_bundle_path(void) {
  return [[[NSBundle mainBundle] bundlePath] UTF8String];
}

char *cocoa_dialog_password(const char *msg) {
#if 0
  NSAlert *alert = create_dialog(msg, 1);
  if (alert == nil) {
    return NULL;
  }

  NSTextField *text = [[MLSecureTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 20)];
  [text autorelease];
  [alert setAccessoryView:text];

  if ([alert runModal] == NSAlertFirstButtonReturn) {
    return strdup([[text stringValue] UTF8String]);
  } else
#endif
  {
    return NULL;
  }
}

int cocoa_dialog_okcancel(const char *msg) {
  return -1;
}

int cocoa_dialog_alert(const char *msg) {
  show_dialog(msg);

  return 1;
}
