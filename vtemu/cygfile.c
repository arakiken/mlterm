/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#if defined(__CYGWIN__) || defined(__MSYS__)

#include <windows.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_path.h> /* bl_conv_to_win32_path */

/* --- global functions --- */

HANDLE
cygfopen(const char* path, const char* mode) {
  char winpath[MAX_PATH];
  HANDLE file;

  if (bl_conv_to_win32_path(path, winpath, sizeof(winpath)) < 0) {
    return NULL;
  }

  if (*mode == 'a') {
    file =
        CreateFileA(winpath, FILE_APPEND_DATA, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  } else /* if( *mode == 'w') */
  {
    file = CreateFileA(winpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  }

  if (file == INVALID_HANDLE_VALUE) {
    return NULL;
  } else {
    return file;
  }
}

int cygfclose(HANDLE file) {
  CloseHandle(file);

  return 0;
}

size_t cygfwrite(const void* ptr, size_t size, size_t nmemb, HANDLE file) {
  DWORD written;

  if (WriteFile(file, ptr, size * nmemb, &written, NULL)) {
    return written;
  } else {
    return 0;
  }
}

#ifdef __DEBUG
void main(void) {
  HANDLE file = cygfopen("test.log", "w");
  cygfwrite("aaa\n", 1, 4, file);
  cygfclose(file);

  file = cygfopen("test.log", "a");
  cygfwrite("bbb\n", 1, 4, file);
  cygfclose(file);
}
#endif

#endif
