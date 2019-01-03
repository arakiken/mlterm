#include <terminaldisplay.h>

using namespace Terminal;

bool Display::ti_flag(const char *capname) {
	return true;
}

int Display::ti_num(const char *capname) {
	return 0;
}

const char *Display::ti_str(const char *capname) {
	return "";
}

Display::Display(bool use_environment)
  : has_ech(true), has_bce(true) /* XXX */, has_title(true),
		/*
		 * smcup and rmcup are used in Display::open() and Display::close().
		 * It is unnecessary for mlterm to use alternate screen for mosh,
		 * so both smcup and rmcup are NULL.
		 */
		smcup(NULL), rmcup(NULL) {
}
