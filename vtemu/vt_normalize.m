/*
 *	$Id$
 */

#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>

/* --- global functions --- */

int vt_normalize(UniChar* str, int num) {
  static CFMutableStringRef mutable_str;

  if (!mutable_str) {
    mutable_str = CFStringCreateMutable(kCFAllocatorDefault, 0);
  }

  CFStringAppendCharacters(mutable_str, str, num);
  CFStringNormalize(mutable_str, kCFStringNormalizationFormC);

  if ((num = CFStringGetLength(mutable_str)) == 1) {
    /* Normalized */
    CFStringGetCharacters(mutable_str, CFRangeMake(0, 1), str);
  }

  CFStringDelete(mutable_str, CFRangeMake(0, num));

  return num;
}

#endif
