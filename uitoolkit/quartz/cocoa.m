/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#import <Cocoa/Cocoa.h>

#include "ui_window.h"
#include "ui_scrollbar.h"

#include <pobl/bl_privilege.h>
#include <pobl/bl_unistd.h> /* bl_getuid/bl_getgid */
#include <pobl/bl_mem.h>
#include <pobl/bl_file.h> /* bl_file_set_cloexec */
#include <pobl/bl_str.h>  /* bl_compare_str */
#include <sys/select.h>
#include <vt_term_manager.h>
#include "../ui_screen_manager.h"
#include "../ui_layout.h"
#include "../ui_selection_encoding.h"

@interface MLTermView : NSView<NSTextInputClient> {
  ui_window_t *uiwindow;
  CGContextRef ctx;
  int forceExpose; /* 2 = visual bell */

  BOOL ignoreKeyDown;
  NSString *markedText;
  int currentShiftMask;
  NSRange markedRange;
  NSRange selectedRange;
}

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

@interface MLSecureTextField : NSSecureTextField
@end

/* --- static variables --- */

static struct {
  int fd;
  void (*handler)(void);

} * additional_fds;

static u_int num_of_additional_fds;

static ui_window_t *uiwindow_for_mlterm_view;

static NSMenuItem *configMenuItem;
static NSMenuItem *pasteMenuItem;

char *global_args;

/* --- static functions --- */

#define set_fill_color(color)                                            \
  CGContextSetRGBFillColor(ctx, (((color)->pixel >> 16) & 0xff) / 255.0, \
                           (((color)->pixel >> 8) & 0xff) / 255.0,       \
                           ((color)->pixel & 0xff) / 255.0, 1.0);

#define IS_OPAQUE                                          \
  ((uiwindow->bg_color.pixel & 0xff000000) == 0xff000000 || \
   ui_window_has_wall_picture(uiwindow))

static void exit_program(void) {
  /* This function is called twice from willClose() and monitor_pty() */
  static int exited;

  if (!exited) {
    exited = 1;

#ifdef DEBUG
    main_loop_final();
    bl_alloca_garbage_collect();
    bl_mem_free_all();
    bl_dl_close_all();
#endif

    [[NSApplication sharedApplication] terminate:nil];
  }
}

static void monitor_pty(void) {
#if 0
  /* normal user (Don't call before NSApplicationMain()) */
  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());
#endif

  dispatch_async(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        static int ret;
        static fd_set read_fds;
        static int maxfd;
        static int is_cont_read;

        for (;;) {
          dispatch_sync(dispatch_get_main_queue(), ^{
            vt_close_dead_terms();

            vt_term_t **terms;
            u_int num_of_terms = vt_get_all_terms(&terms);

            int count;
            int ptyfd;
            if (ret > 0) {
              for (count = 0; count < num_of_additional_fds; count++) {
                if (additional_fds[count].fd < 0) {
                  (*additional_fds[count].handler)();
                }
              }

              for (count = 0; count < num_of_terms; count++) {
                if ((ptyfd = vt_term_get_master_fd(terms[count])) >= 0 &&
                    FD_ISSET(ptyfd, &read_fds)) {
                  vt_term_parse_vt100_sequence(terms[count]);
                }
              }

              if (++is_cont_read >= 2) {
                [[NSRunLoop currentRunLoop]
                    runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
              }
            } else if (ret == 0) {
              ui_display_idling(NULL);
              is_cont_read = 0;
            }

            FD_ZERO(&read_fds);
            maxfd = -1;

            for (count = 0; count < num_of_terms; count++) {
              if ((ptyfd = vt_term_get_master_fd(terms[count])) >= 0) {
                FD_SET(ptyfd, &read_fds);

                if (ptyfd > maxfd) {
                  maxfd = ptyfd;
                }
              }
            }

            for (count = 0; count < num_of_additional_fds; count++) {
              if (additional_fds[count].fd >= 0) {
                FD_SET(additional_fds[count].fd, &read_fds);

                if (additional_fds[count].fd > maxfd) {
                  maxfd = additional_fds[count].fd;
                }
              }
            }
          });

          struct timeval tval;
          tval.tv_usec = 100000; /* 0.1 sec */
          tval.tv_sec = 0;

          if (maxfd == -1 ||
              (ret = select(maxfd + 1, &read_fds, NULL, NULL, &tval)) < 0) {
            dispatch_sync(dispatch_get_main_queue(), ^{
              exit_program();
            });

            break;
          }
        }
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

    for (; len > 0 && glyphs[len - 1] == 0; len--)
      ;
  }

  CGContextSetFont(ctx, font->xfont->cg_font);

  CGAffineTransform t = CGAffineTransformIdentity;
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
  ef_parser_t *utf8_parser;

  if (!(utf8_parser = ui_get_selection_parser(1))) {
    return;
  }

  (*utf8_parser->init)(utf8_parser);

  vt_term_t *term = ((ui_screen_t *)uiwindow)->term;

  vt_term_set_config(term, "use_local_echo", "false");

  if (*preedit_text == '\0') {
    preedit_text = NULL;
  } else {
    if (bl_compare_str(preedit_text, cur_preedit_text) == 0) {
      return;
    }

    vt_term_parse_vt100_sequence(term);
    vt_term_set_config(term, "use_local_echo", "true");

    (*utf8_parser->set_str)(utf8_parser, preedit_text, strlen(preedit_text));

    u_char buf[128];
    size_t len;
    while (!utf8_parser->is_eos &&
           (len = vt_term_convert_to(term, buf, sizeof(buf), utf8_parser)) >
               0) {
      vt_term_preedit(term, buf, len);
    }
  }

  ui_window_update(uiwindow, 3); /* UPDATE_SCREEN|UPDATE_CURSOR */
}

static void remove_all_observers(ui_window_t *uiwindow) {
  u_int count;

  for (count = 0; count < uiwindow->num_of_children; count++) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:uiwindow->children[count]->my_window];
    remove_all_observers(uiwindow->children[count]);
  }
}

static NSAlert *create_dialog(const char *msg, int has_cancel) {
  NSAlert *alert = [[NSAlert alloc] init];
  [alert autorelease];

  NSString *ns_msg = [NSString stringWithCString:msg encoding:NSUTF8StringEncoding];
  [alert setMessageText:ns_msg];
  [alert addButtonWithTitle:@"OK"]; /* First */
  if (has_cancel) {
    [alert addButtonWithTitle:@"Cancel"]; /* Second */
  }

  return alert;
}

/* --- class --- */

int cocoa_dialog_alert(const char *msg);

@implementation MLTermView

- (id)initWithFrame:(NSRect)frame {
  if (uiwindow_for_mlterm_view) {
    uiwindow = uiwindow_for_mlterm_view;
  } else {
    if (global_args) {
      char *args = bl_str_alloca_dup(global_args);
      if (args) {
        ui_mlclient(args, NULL);
      }
    } else {
      char args[] = "mlclient";
      ui_mlclient(args, NULL);
    }

    ui_screen_t **screens;
    u_int num = ui_get_all_screens(&screens);
    if (num == 0) {
      cocoa_dialog_alert("Failed to open screen");
      exit(1);
    }

    uiwindow = &screens[num - 1]->window;
  }

  uiwindow->my_window = (NSView *)self;
  forceExpose = 1;

  [super initWithFrame:frame];

  [self registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType,
                                                          nil]];

  ignoreKeyDown = FALSE;
  markedText = nil;
  markedRange = NSMakeRange(NSNotFound, 0);
  selectedRange = NSMakeRange(NSNotFound, 0);

  if (uiwindow_for_mlterm_view) {
    uiwindow_for_mlterm_view = NULL;
  }

  if (!configMenuItem) {
    monitor_pty();

#if 1
    [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask
                                          handler:^(NSEvent *event) {
                                            switch (event.keyCode) {
                                              case 0x66: /* Eisu */
                                              case 0x68: /* Kana */
                                                ignoreKeyDown = TRUE;
                                            }

                                            [event.window.firstResponder
                                                keyDown:event];

                                            return (NSEvent *)nil;
                                          }];
#endif

    /* for mlconfig */
    setenv("PANGO_LIBDIR", [[[NSBundle mainBundle] bundlePath] UTF8String], 1);

    NSMenu *appmenu = [[NSMenu alloc] initWithTitle:@""];
    configMenuItem = [appmenu addItemWithTitle:@"Config"
                                        action:@selector(configMenu:)
                                 keyEquivalent:@"C"];
    pasteMenuItem = [appmenu addItemWithTitle:@"Paste"
                                       action:@selector(pasteMenu:)
                                keyEquivalent:@"P"];
    [configMenuItem setTarget:self];
    [pasteMenuItem setTarget:self];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    [[menu addItemWithTitle:@"" action:nil keyEquivalent:@""]
        setSubmenu:appmenu];
    [[NSApplication sharedApplication] setMainMenu:menu];
  }

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)configMenu:(id)sender {
  /* if by any change */
  if (((ui_screen_t *)uiwindow)->term) {
    ui_screen_exec_cmd((ui_screen_t *)uiwindow, "mlconfig");
  }
}

- (void)pasteMenu:(id)sender {
  /* if by any change */
  if (((ui_screen_t *)uiwindow)->term) {
    ui_screen_exec_cmd((ui_screen_t *)uiwindow, "paste");
  }
}

static void reset_position(ui_window_t *uiwindow) {
  u_int count;

  for (count = 0; count < uiwindow->num_of_children; count++) {
    ui_window_t *child = uiwindow->children[count];

    [((NSView *)child->my_window)
        setFrame:NSMakeRect(child->x,
                            ((int)ACTUAL_HEIGHT(uiwindow)) -
                                ((int)ACTUAL_HEIGHT(child)) - child->y,
                            ACTUAL_WIDTH(child), ACTUAL_HEIGHT(child))];

    reset_position(child);
  }
}

- (void)resized:(NSNotification *)note {
  if (!uiwindow->parent || !((ui_screen_t *)uiwindow)->term) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  uiwindow->parent->width =
      ((NSView *)[self window].contentView).frame.size.width -
      uiwindow->parent->hmargin * 2;
  uiwindow->parent->height =
      ((NSView *)[self window].contentView).frame.size.height -
      uiwindow->parent->vmargin * 2;

  (*uiwindow->parent->window_resized)(uiwindow->parent);
  reset_position(uiwindow->parent);
}

- (void)viewDidMoveToWindow {
  if ([self window] == nil) {
    /* just before being deallocated */
    return;
  }

  struct terminal *term = ((ui_screen_t *)uiwindow)->screen_scroll_listener->self;

  if (!uiwindow->parent->my_window) {
    [[self window] orderOut:self];

    if (uiwindow->event_mask & PointerMotionMask) {
      [self window].acceptsMouseMovedEvents = YES;
    }
  }

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(resized:)
             name:NSViewFrameDidChangeNotification
           object:[self window].contentView];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(focused:)
             name:NSWindowDidBecomeMainNotification
           object:[self window]];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(unfocused:)
             name:NSWindowDidResignMainNotification
           object:[self window]];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(willClose:)
             name:NSWindowWillCloseNotification
           object:[self window]];

  int diff_x = [self window].frame.size.width - self.frame.size.width;
  int diff_y = [self window].frame.size.height - self.frame.size.height;
  u_int sb_width = [NSScroller scrollerWidth];
  int y = ACTUAL_HEIGHT(uiwindow->parent) - ACTUAL_HEIGHT(uiwindow) - uiwindow->y;

  /* Change view size */

  self.frame =
      CGRectMake(uiwindow->x, y, ACTUAL_WIDTH(uiwindow), ACTUAL_HEIGHT(uiwindow));

  /* Creating scrollbar */

  CGRect r =
      CGRectMake(term->scrollbar.window.x, y, sb_width, ACTUAL_HEIGHT(uiwindow));
  NSScroller *scroller = [[NSScroller alloc] initWithFrame:r];
  [scroller setEnabled:YES];
  [scroller setTarget:self];
  [scroller setAction:@selector(scrollerAction:)];
  [scroller setFloatValue:0.0 knobProportion:1.0];
#if 0
  [scroller setArrowsPosition:NSScrollerArrowsMaxEnd];
#endif
  [[self window].contentView addSubview:scroller];

  term->scrollbar.window.my_window = (NSView *)scroller;

  if (term->sb_mode != SBM_NONE) {
    uiwindow->parent->width =
        uiwindow->parent->width + sb_width - term->scrollbar.window.width;
  } else {
    [scroller setHidden:YES];
  }
  term->scrollbar.window.width = sb_width;

  /* Change window size */

  if (!uiwindow->parent->my_window) {
    [[self window] useOptimizedDrawing:YES];

    uiwindow->parent->my_window = [self window];

    r = [self window].frame;
    r.size.width = ACTUAL_WIDTH(uiwindow->parent) + diff_x;
    float new_height = ACTUAL_HEIGHT(uiwindow->parent) + diff_y;
    r.origin.y += (r.size.height - new_height);
    r.size.height = new_height;

    [[self window] setResizeIncrements:NSMakeSize(uiwindow->width_inc,
                                                  uiwindow->height_inc)];
    [[self window] setFrame:r display:NO];

    if (!IS_OPAQUE) {
      [self bgColorChanged];
    }

    [[self window] makeKeyAndOrderFront:self];

    /*
     * Adjust by uiwindow->parent->{x,y} after [window setFrame:r] above adjusted
     * window position.
     */
    if (uiwindow->parent->x > 0 || uiwindow->parent->y > 0) {
      r = [self window].frame;
      r.origin.x += uiwindow->parent->x;
      r.origin.y -= uiwindow->parent->y;

      [[self window] setFrame:r display:NO];
    }
  }

  NSRect sr = [[[self window] screen] visibleFrame];
  uiwindow->disp->width = sr.size.width;
  uiwindow->disp->height = sr.size.height;

  /* Adjust scroller position */
  [scroller setFrameOrigin:NSMakePoint(term->scrollbar.window.x, y)];
}

- (void)scrollerAction:(id)sender {
  struct terminal *term = ((ui_screen_t *)uiwindow)->screen_scroll_listener->self;
  ui_scrollbar_t *sb = &term->scrollbar;
  float pos = [sender floatValue];

  switch ([sender hitPart]) {
    case NSScrollerKnob:
    case NSScrollerKnobSlot:
      ui_scrollbar_is_moved(sb, pos);
      break;
    case NSScrollerDecrementLine:
      ui_scrollbar_move_upward(sb, 1);
      (sb->sb_listener->screen_scroll_downward)(sb->sb_listener->self, 1);
      break;
    case NSScrollerDecrementPage:
      ui_scrollbar_move_upward(sb, 10);
      (sb->sb_listener->screen_scroll_downward)(sb->sb_listener->self, 10);
      break;
    case NSScrollerIncrementLine:
      ui_scrollbar_move_downward(sb, 1);
      (sb->sb_listener->screen_scroll_upward)(sb->sb_listener->self, 1);
      break;
    case NSScrollerIncrementPage:
      ui_scrollbar_move_downward(sb, 10);
      (sb->sb_listener->screen_scroll_upward)(sb->sb_listener->self, 10);
      break;
    case NSScrollerNoPart:
      break;
  }
}

- (void)drawRect:(NSRect)rect {
  if (!uiwindow->parent || !((ui_screen_t *)uiwindow)->term) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  XExposeEvent ev;

  ctx = [[NSGraphicsContext currentContext] graphicsPort];
#if 0
  CGAffineTransform t = CGContextGetCTM(ctx);
  bl_debug_printf("%f %f %f %f %f %f\n", t.a, t.b, t.c, t.d, t.tx, t.ty);
#endif

  if (forceExpose & 2) {
    [self fillWith:&uiwindow->fg_color
                  :uiwindow->hmargin
                  :uiwindow->vmargin
                  :uiwindow->width
                  :uiwindow->height];
    CGContextFlush(ctx);

    [[NSRunLoop currentRunLoop]
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

    forceExpose = 1;
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

  for (count = 0; count < win->num_of_children; count++) {
    ui_window_t *hit;

    if ((hit = get_current_window(win->children[count]))) {
      return hit;
    }
  }

  return NULL;
}

- (void)focused:(NSNotification *)note {
  if (!uiwindow->parent || !((ui_screen_t *)uiwindow)->term) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  XEvent ev;

  ev.type = UI_FOCUS_IN;

  ui_window_receive_event(ui_get_root_window(uiwindow), &ev);

  ui_window_t *focused;

  if ((focused = get_current_window(ui_get_root_window(uiwindow)))) {
    [configMenuItem setTarget:focused->my_window];
    [pasteMenuItem setTarget:focused->my_window];
  }
}

- (void)unfocused:(NSNotification *)note {
  if (!uiwindow->parent || !((ui_screen_t *)uiwindow)->term) {
    /* It has been already removed from ui_layout or term has been detached. */
    return;
  }

  XEvent ev;

  ev.type = UI_FOCUS_OUT;

  ui_window_receive_event(ui_get_root_window(uiwindow), &ev);
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)becomeFirstResponder {
  XEvent ev;

  ev.type = UI_KEY_FOCUS_IN;

  ui_window_receive_event(uiwindow, &ev);

  [configMenuItem setTarget:uiwindow->my_window];
  [pasteMenuItem setTarget:uiwindow->my_window];

  return YES;
}

- (void)willClose:(NSNotification *)note {
  if (ui_get_all_screens(NULL) == 0) {
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    return;
  }

  ui_window_t *root = ui_get_root_window(uiwindow);

  if (root->num_of_children == 0) {
    /*
     * This function can be called if root->num_of_children == 0
     * by window_dealloc() in ui_window.c.
     */
    return;
  }

  remove_all_observers(root);

  XEvent ev;

  ev.type = UI_CLOSE_WINDOW;

  ui_window_receive_event(root, &ev);
  ui_close_dead_screens();
  if (ui_get_all_screens(NULL) == 0) {
    exit_program();
  }
}

- (void)mouseDown:(NSEvent *)event {
  NSPoint loc = [event locationInWindow];
  XButtonEvent bev;

  bev.type = UI_BUTTON_PRESS;
  bev.time = event.timestamp * 1000;
  bev.x = loc.x - self.frame.origin.x;
  bev.y =
      ACTUAL_HEIGHT(uiwindow->parent) - loc.y - /* self.frame.origin.y - */ 1;
  bev.state = event.modifierFlags &
              (NSShiftKeyMask | NSControlKeyMask | NSAlternateKeyMask);
  if (event.type == NSLeftMouseDown) {
    bev.button = 1;
  } else {
    bev.button = 3;
  }
  bev.click_count = event.clickCount;

  ui_window_receive_event(uiwindow, (XEvent *)&bev);
}

- (void)mouseUp:(NSEvent *)event {
  NSPoint loc = [event locationInWindow];
  XButtonEvent bev;

  bev.type = UI_BUTTON_RELEASE;
  bev.time = event.timestamp * 1000;
  bev.x = loc.x - self.frame.origin.x;
  bev.y =
      ACTUAL_HEIGHT(uiwindow->parent) - loc.y - /* self.frame.origin.y - */ 1;
  bev.state = event.modifierFlags &
              (NSShiftKeyMask | NSControlKeyMask | NSAlternateKeyMask);
  if (event.type == NSLeftMouseUp) {
    bev.button = 1;
  } else {
    bev.button = 3;
  }

  ui_window_receive_event(uiwindow, (XEvent *)&bev);
}

- (void)rightMouseUp:(NSEvent *)event {
  NSPoint loc = [event locationInWindow];
  XButtonEvent bev;

  bev.type = UI_BUTTON_RELEASE;
  bev.time = event.timestamp * 1000;
  bev.x = loc.x - self.frame.origin.x;
  bev.y =
      ACTUAL_HEIGHT(uiwindow->parent) - loc.y - /* self.frame.origin.y - */ 1;
  bev.state = event.modifierFlags &
              (NSShiftKeyMask | NSControlKeyMask | NSAlternateKeyMask);
  bev.button = 3;

  ui_window_receive_event(uiwindow, (XEvent *)&bev);
}

- (NSMenu *)menuForEvent:(NSEvent *)event {
  NSPoint loc = [event locationInWindow];
  XButtonEvent bev;

  bev.type = UI_BUTTON_PRESS;
  bev.time = event.timestamp * 1000;
  bev.x = loc.x - self.frame.origin.x;
  bev.y =
      ACTUAL_HEIGHT(uiwindow->parent) - loc.y - /* self.frame.origin.y - */ 1;
  bev.state = event.modifierFlags &
              (NSShiftKeyMask | NSControlKeyMask | NSAlternateKeyMask);
  bev.button = 3;
  bev.click_count = event.clickCount;

  ui_window_receive_event(uiwindow, (XEvent *)&bev);

  return (NSMenu *)nil;
}

- (void)mouseDragged:(NSEvent *)event {
  NSPoint loc = [event locationInWindow];
  XMotionEvent mev;

  mev.type = UI_BUTTON_MOTION;
  mev.time = event.timestamp * 1000;
  mev.x = loc.x - self.frame.origin.x;
  mev.y = ACTUAL_HEIGHT(uiwindow->parent) - loc.y - /* self.frame.origin.y */ -1;
  if (event.type == NSLeftMouseDragged) {
    mev.state = Button1Mask;
  } else {
    mev.state = Button3Mask;
  }

  ui_window_receive_event(uiwindow, (XEvent *)&mev);
}

- (void)rightMouseDragged:(NSEvent *)event {
  NSPoint loc = [event locationInWindow];
  XMotionEvent mev;

  mev.type = UI_BUTTON_MOTION;
  mev.time = event.timestamp * 1000;
  mev.x = loc.x - self.frame.origin.x;
  mev.y = ACTUAL_HEIGHT(uiwindow->parent) - loc.y - /* self.frame.origin.y */ -1;
  mev.state = Button3Mask;

  ui_window_receive_event(uiwindow, (XEvent *)&mev);
}

- (void)mouseMoved:(NSEvent *)event {
  NSPoint loc = [event locationInWindow];
  XMotionEvent mev;

  mev.type = UI_POINTER_MOTION;
  mev.time = event.timestamp * 1000;
  mev.x = loc.x - self.frame.origin.x;
  mev.y = ACTUAL_HEIGHT(uiwindow->parent) - loc.y - /* self.frame.origin.y */ -1;
  mev.state = 0;

  ui_window_receive_event(uiwindow, (XEvent *)&mev);
}

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

- (void)keyDown:(NSEvent *)event {
  if ([event type] == NSFlagsChanged) {
    return;
  }

  u_int flags = event.modifierFlags;

  /* ShiftMask is not set without this. (see insertText()) */
  currentShiftMask = flags & NSShiftKeyMask;

  /* Alt+x isn't interpreted unless preediting. */
  if (markedText || !(flags & NSAlternateKeyMask) ||
      (flags & NSControlKeyMask)) {
    [self interpretKeyEvents:[NSArray arrayWithObject:event]];
  }

  if (ignoreKeyDown) {
    ignoreKeyDown = FALSE;
  } else if (!markedText) {
    XKeyEvent kev;

    kev.type = UI_KEY_PRESS;
    kev.state = flags & (NSShiftKeyMask | NSControlKeyMask |
                         NSAlternateKeyMask | NSCommandKeyMask);

    if (kev.state & NSControlKeyMask) {
      kev.keysym = [[event charactersIgnoringModifiers] characterAtIndex:0];
      kev.utf8 = [event characters].UTF8String;
    } else if (kev.state & NSAlternateKeyMask) {
      kev.keysym = [[event charactersIgnoringModifiers] characterAtIndex:0];
      kev.utf8 = NULL;
    } else {
      kev.keysym = [[event characters] characterAtIndex:0];
      kev.utf8 = [event characters].UTF8String;
    }

    if ((kev.state & NSShiftKeyMask) && 'A' <= kev.keysym &&
        kev.keysym <= 'Z') {
      kev.keysym += 0x20;
    }

    ui_window_receive_event(uiwindow, (XEvent *)&kev);
  }
}

#if 0
- (BOOL)_wantsKeyDownForEvent:(id)event {
  return YES;
}
#endif

- (void)pasteboard:(NSPasteboard *)sender provideDataForType:(NSString *)type {
  /*
   * If this view exits with owning pasteboard, this method is called
   * after ui_screen_t::term is deleted.
   */
  if (((ui_screen_t *)uiwindow)->term) {
    XSelectionRequestEvent ev;

    ev.type = UI_SELECTION_REQUESTED;
    ev.sender = sender;

    ui_window_receive_event(uiwindow, (XEvent *)&ev);
  }
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  NSPasteboard *pboard = [sender draggingPasteboard];
  NSDragOperation sourceMask = [sender draggingSourceOperationMask];

  if ([[pboard types] containsObject:NSFilenamesPboardType]) {
    if (sourceMask & NSDragOperationLink) {
      return NSDragOperationLink;
    } else if (sourceMask & NSDragOperationCopy) {
      return NSDragOperationCopy;
    }
  }

  return NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  NSPasteboard *pboard = [sender draggingPasteboard];

  if ([[pboard types] containsObject:NSFilenamesPboardType]) {
    NSArray *files = [pboard propertyListForType:NSFilenamesPboardType];
    int count;
    XSelectionNotifyEvent ev;

    ev.type = UI_SELECTION_NOTIFIED;
    for (count = 0; count < [files count]; count++) {
      ev.data = [[files objectAtIndex:count] UTF8String];
      ev.len = strlen(ev.data);

      ui_window_receive_event(uiwindow, (XEvent *)&ev);
    }
  }

  return YES;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
  return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actualRange {
  int x;
  int y;

  if (!uiwindow->xim_listener ||
      !(*uiwindow->xim_listener->get_spot)(uiwindow->xim_listener->self, &x,
                                          &y)) {
    x = y = 0;
  }

  x += (uiwindow->x + uiwindow->hmargin);
  y += (uiwindow->y + uiwindow->vmargin);

  NSRect r = NSMakeRect(x, ACTUAL_HEIGHT(uiwindow->parent) - y,
                        ui_col_width((ui_screen_t *)uiwindow),
                        ui_line_height((ui_screen_t *)uiwindow));
  r.origin = [[self window] convertBaseToScreen:r.origin];

  return r;
}

- (NSArray *)validAttributesForMarkedText {
  return nil;
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)
                                                                actualRange {
  return nil;
}

- (BOOL)hasMarkedText {
  if (markedText) {
    return YES;
  } else {
    return NO;
  }
}

- (NSRange)markedRange {
  return markedRange;
}

- (NSRange)selectedRange {
  return selectedRange;
}

- (void)unmarkText {
  [markedText release];
  markedText = nil;
  update_ime_text(uiwindow, "", NULL);
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selected
     replacementRange:(NSRange)replacement {
  if ([string isKindOfClass:[NSAttributedString class]]) {
    string = [string string];
  }

  selectedRange = selected;

  if ([string length] > 0) {
    char *p;

    if (!(p = alloca(strlen([string UTF8String]) + 10))) {
      return;
    }
    *p = '\0';

    if (selectedRange.location > 0) {
      strcpy(p,
             [[string substringWithRange:
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

    update_ime_text(uiwindow, p, markedText ? markedText.UTF8String : NULL);
  } else if (markedText) {
    update_ime_text(uiwindow, "", markedText.UTF8String);
  }

  if (markedText) {
    [markedText release];
    markedText = nil;
  }

  if ([string length] > 0) {
    markedText = [string copy];
    markedRange = NSMakeRange(0, [string length]);
  } else {
    markedRange = NSMakeRange(NSNotFound, 0);
  }
}

- (void)doCommandBySelector:(SEL)selector {
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
  [self unmarkText];

  if ([string length] > 0) {
    XKeyEvent kev;

    kev.type = UI_KEY_PRESS;
    kev.state = currentShiftMask;
    kev.keysym = 0;
    kev.utf8 = [string UTF8String];

    ui_window_receive_event(uiwindow, (XEvent *)&kev);

    ignoreKeyDown = TRUE;
  }
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
  CGContextSelectFont(ctx, "Menlo", 1, kCGEncodingMacRoman);
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
    [[NSColor colorWithDeviceRed:((color->pixel >> 16) & 0xff) / 255.0
                           green:((color->pixel >> 8) & 0xff) / 255.0
                            blue:(color->pixel & 0xff) / 255.0
                           alpha:((color->pixel >> 24) & 0xff) / 255.0] set];

    NSRectFillUsingOperation(
        NSMakeRect(x, ACTUAL_HEIGHT(uiwindow) - y - height, width, height),
        NSCompositeCopy);
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
  NSRect src_r = NSMakeRect(src_x, ACTUAL_HEIGHT(uiwindow) - src_y - height,
			width, height);
  NSBitmapImageRep *bir = [self bitmapImageRepForCachingDisplayInRect:src_r];
  [self cacheDisplayInRect:src_r toBitmapImageRep:bir];
  CGImageRef image = bir.CGImage;

  CGRect dst_r = CGRectMake(dst_x, ACTUAL_HEIGHT(uiwindow) - dst_y - height,
				width, height);
  CGContextDrawImage(ctx, dst_r, image);

  static int i;
  if (i == 0) {
    CFURLRef url = [NSURL fileURLWithPath:@"/Users/ken/kallist/log.png"];
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(
				   url, kUTTypePNG, 1, NULL);
    CGImageDestinationAddImage(dest, image, nil);
    CGImageDestinationFinalize(dest);
    CFRelease( dest);
    i++;
  }
  else if (i == 1) {
    NSImage *  nsimage = [[[NSImage alloc]initWithSize:src_r.size] autorelease];
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
    [self setNeedsDisplay:YES];
  } else {
    int x;
    int y;

    if (!uiwindow->xim_listener ||
        !(*uiwindow->xim_listener->get_spot)(uiwindow->xim_listener->self, &x,
                                            &y)) {
      x = y = 0;
    }

    x += (uiwindow->hmargin);
    y += (uiwindow->vmargin);

    [self
        setNeedsDisplayInRect:NSMakeRect(x, ACTUAL_HEIGHT(uiwindow) - y, 1, 1)];
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
    [[self window] setBackgroundColor:[NSColor whiteColor]];
    [[self window] setOpaque:YES];
  } else {
    [[self window] setBackgroundColor:[NSColor clearColor]];
    [[self window] setOpaque:NO];
  }
}

@end

@implementation MLSecureTextField

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  [self becomeFirstResponder];
}

@end

/* --- global functions --- */

void view_alloc(ui_window_t *uiwindow) {
  uiwindow_for_mlterm_view = uiwindow;

  MLTermView *view =
      [[MLTermView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)];
  [((NSWindow *)uiwindow->parent->my_window).contentView addSubview:view];
  [view release];
}

void view_dealloc(NSView *view) {
  /* view is also released by this. */
  [view removeFromSuperviewWithoutNeedingDisplay];
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

void view_set_input_focus(NSView *view) {
  [[view window] makeFirstResponder:view];
}

void view_set_rect(NSView *view, int x, int y, /* The origin is left-botom. */
                   u_int width, u_int height) {
#if 0
  /* Scrollbar position is corrupt in spliting screen vertically. */
  [view setFrame:NSMakeRect(x,y,width,height)];
#else
  [view setFrameSize:NSMakeSize(width, height)];
  [view setFrameOrigin:NSMakePoint(x, y)];
#endif
}

void view_set_hidden(NSView *view, int flag) { [view setHidden:flag]; }

void window_alloc(ui_window_t *root) {
  uiwindow_for_mlterm_view = root->children[1];

  NSNib *nib = [[NSNib alloc] initWithNibNamed:@"Main" bundle:nil];
  [nib instantiateNibWithOwner:nil topLevelObjects:nil];
  [nib release];
}

void window_dealloc(NSWindow *window) { [window release]; }

void window_resize(NSWindow *window, int width, int height) {
  CGRect wr = window.frame;
  CGSize vs = ((NSView *)window.contentView).frame.size;
  int diff_x = wr.size.width - vs.width;
  int diff_y = wr.size.height - vs.height;

  wr.origin.y += (vs.height - height);
  wr.size.width = width + diff_x;
  wr.size.height = height + diff_y;
  [window setFrame:wr display:YES];
}

void window_accepts_mouse_moved_events(NSWindow *window, int accept) {
  window.acceptsMouseMovedEvents = (accept ? YES : NO);
}

void window_set_normal_hints(NSWindow *window, u_int width_inc,
                             u_int height_inc) {
  [window setResizeIncrements:NSMakeSize(width_inc, height_inc)];
}

void window_get_position(NSWindow *window, int *x, int *y) {
  *x = window.frame.origin.x;
  *y = [[window screen] visibleFrame].size.height - window.frame.origin.y -
       [window.contentView frame].size.height;
}

void window_set_title(NSWindow *window, const char *title /* utf8 */) {
  NSString *ns_title = [NSString stringWithCString:title encoding:NSUTF8StringEncoding];
  [window setTitle:ns_title];
}

void app_urgent_bell(int on) {
  if (on) {
    [[NSApplication sharedApplication] requestUserAttention:NSCriticalRequest];
  } else {
    [[NSApplication sharedApplication]
        cancelUserAttentionRequest:NSCriticalRequest];
  }
}

void scroller_update(NSScroller *scroller, float pos, float knob) {
  [scroller setFloatValue:pos knobProportion:knob];
}

CGFontRef cocoa_create_font(const char *font_family) {
  NSString *ns_font_family =
      [NSString stringWithCString:font_family encoding:NSUTF8StringEncoding];

  return CGFontCreateWithFontName(ns_font_family);
}

#ifdef USE_OT_LAYOUT
char *cocoa_get_font_path(CGFontRef cg_font) {
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

int cocoa_clipboard_own(MLTermView *view) {
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

  [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString]
                     owner:view];

  return 1;
}

void cocoa_clipboard_set(const u_char *utf8, size_t len) {
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  NSString *str = [[NSString alloc] initWithBytes:utf8
                                           length:len
                                         encoding:NSUTF8StringEncoding];

  [pasteboard setString:str forType:NSPasteboardTypeString];
  [str release];
}

const char *cocoa_clipboard_get(void) {
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  NSString *available;

  available = [pasteboard
      availableTypeFromArray:[NSArray arrayWithObject:NSPasteboardTypeString]];
  if ([available isEqualToString:NSPasteboardTypeString]) {
    NSString *str = [pasteboard stringForType:NSPasteboardTypeString];
    if (str != nil) {
      return [str UTF8String];
    }
  }

  return NULL;
}

void cocoa_beep(void) { NSBeep(); }

CGImageRef cocoa_load_image(const char *path, u_int *width, u_int *height) {
  NSString *nspath =
      [NSString stringWithCString:path encoding:NSUTF8StringEncoding];
  NSImage *nsimg = [[NSImage alloc] initWithContentsOfFile:nspath];
  if (!nsimg) {
    return nil;
  }

  CGImageRef cgimg = [nsimg CGImageForProposedRect:NULL context:nil hints:nil];

  CGImageRetain(cgimg);

  *width = nsimg.size.width;
  *height = nsimg.size.height;
  [nsimg release];

  return cgimg;
}

/*
 * fd >= 0  -> Normal file descriptor. handler is invoked if fd is ready.
 * fd < 0 -> Special ID. handler is invoked at interval of 0.1 sec.
 */
int cocoa_add_fd(int fd, void (*handler)(void)) {
  void *p;

  if (!handler) {
    return 0;
  }

  if ((p = realloc(additional_fds, sizeof(*additional_fds) *
                                       (num_of_additional_fds + 1))) == NULL) {
    return 0;
  }

  additional_fds = p;
  additional_fds[num_of_additional_fds].fd = fd;
  additional_fds[num_of_additional_fds++].handler = handler;
  if (fd >= 0) {
    bl_file_set_cloexec(fd);
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %d is added to additional fds.\n", fd);
#endif

  return 1;
}

int cocoa_remove_fd(int fd) {
  u_int count;

  for (count = 0; count < num_of_additional_fds; count++) {
    if (additional_fds[count].fd == fd) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Additional fd %d is removed.\n", fd);
#endif

      additional_fds[count] = additional_fds[--num_of_additional_fds];

      return 1;
    }
  }

  return 0;
}

const char *cocoa_get_bundle_path(void) {
  return [[[NSBundle mainBundle] bundlePath] UTF8String];
}

char *cocoa_dialog_password(const char *msg) {
  NSAlert *alert = create_dialog(msg, 1);

  NSTextField *text = [[MLSecureTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 20)];
  [text autorelease];
  [alert setAccessoryView:text];

  if ([alert runModal] == NSAlertFirstButtonReturn) {
    return strdup([[text stringValue] UTF8String]);
  } else {
    return NULL;
  }
}

int cocoa_dialog_okcancel(const char *msg) {
  NSAlert *alert = create_dialog(msg, 1);

  if ([alert runModal] == NSAlertFirstButtonReturn) {
    return 1;
  } else {
    return 0;
  }
}

int cocoa_dialog_alert(const char *msg) {
  NSAlert *alert = create_dialog(msg, 0);

  if ([alert runModal] == NSAlertFirstButtonReturn) {
    return 1;
  } else {
    return 0;
  }
}
