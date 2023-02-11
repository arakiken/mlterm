/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <terminaldisplay.h>

using namespace Terminal;

/* XXX See configure.in */
#ifdef MOSH_VERSION
/* 1.3.2 or before */
#if MOSH_VERSION <= 10302
bool Display::ti_flag(const char *capname) {
  return true;
}

int Display::ti_num(const char *capname) {
  return 0;
}

const char *Display::ti_str(const char *capname) {
  return "";
}
#endif
#endif

Display::Display(bool use_environment)
  : has_ech(true), has_bce(true) /* XXX */, has_title(true),
    /*
     * smcup and rmcup are used in Display::open() and Display::close().
     * It is unnecessary for mlterm to use alternate screen for mosh,
     * so both smcup and rmcup are NULL.
     */
    smcup(NULL), rmcup(NULL) {
}
