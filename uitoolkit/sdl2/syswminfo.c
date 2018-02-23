/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <SDL_syswm.h>

static SDL_SYSWM_TYPE subsystem = SDL_SYSWM_UNKNOWN;

void syswminfo_init(void *window) {
  SDL_SysWMinfo info;

  SDL_VERSION(&info.version);

  if (SDL_GetWindowWMInfo(window, &info)) {
    subsystem = info.subsystem;
  }
}

int syswminfo_is_thread_safe(void) {
  return (subsystem == SDL_SYSWM_WINDOWS || subsystem == SDL_SYSWM_X11 ||
          subsystem == SDL_SYSWM_DIRECTFB || subsystem == SDL_SYSWM_COCOA);
}
