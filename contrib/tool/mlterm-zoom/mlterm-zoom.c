/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>

#define BUTTON_4_COMMAND "\x1b]5379;fontsize=larger\x07"
#define BUTTON_5_COMMAND "\x1b]5379;fontsize=smaller\x07"

void event_loop(Display* display);

int main(int argc, char* argv[]) {
  Display* display;
  Window window;
  XSetWindowAttributes attrs;
  char* display_name = NULL;
  int screen_no;
  int x, y, w, h, i;

  x = 0;
  y = 0;
  i = 1;
  while (i < argc) {
    if (strcmp(argv[i], "--display") == 0) {
      i++;
      display_name = argv[i];
    } else if (strcmp(argv[i], "--geometry") == 0) {
      i++;
      XParseGeometry(argv[i], &x, &y, &w, &h);
    }
    i++;
  }
  w = h = 32;

  display = XOpenDisplay(display_name);
  screen_no = DefaultScreen(display);

  attrs.override_redirect = True;
  attrs.event_mask = ButtonPressMask | KeyPressMask | LeaveWindowMask;
  attrs.background_pixel = WhitePixel(display, screen_no);
  attrs.border_pixel = BlackPixel(display, screen_no);

  window = XCreateWindow(display, RootWindow(display, screen_no), x - (w / 2), y - (h / 2), w, h, 2,
                         DefaultDepth(display, screen_no), InputOutput,
                         DefaultVisual(display, screen_no),
                         CWOverrideRedirect | CWEventMask | CWBackPixel | CWBorderPixel, &attrs);

  XMapWindow(display, window);

  event_loop(display);
  return 0;
}

void event_loop(Display* display) {
  XEvent event;

  while (1) {
    XNextEvent(display, &event);
    switch (event.type) {
      case ButtonPress:
        if (event.xbutton.button == 4) {
          fprintf(stdout, BUTTON_4_COMMAND);
        } else if (event.xbutton.button == 5) {
          fprintf(stdout, BUTTON_5_COMMAND);
        } else {
          return;
        }
        fflush(stdout);
        break;
      case LeaveNotify:
        return;
        break;
      default:
        break;
    }
  }
  return;
}
