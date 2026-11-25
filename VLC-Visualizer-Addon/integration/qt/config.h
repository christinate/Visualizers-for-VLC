#pragma once
#define PACKAGE_NAME "VLC media player"
#define PACKAGE "vlc"
#define VERSION "3.0.24"
#define VERSION_MESSAGE "3.0.24 Vetinari — Visualizers integration"
#define COPYRIGHT_MESSAGE "Copyright (C) 1996-2026 the VideoLAN team"
#define VLC_COMPILE_BY "VLC Visualizers contributors"
#define VLC_COMPILE_HOST "native"
#define HAVE_CXX11 1
#define HAVE_SETENV 1
#define HAVE_STRUCT_POLLFD 1
#define VLC_WINSTORE_APP 0
#define ENABLE_VLM 1
#define N_(str) (str)
#define _(str) vlc_gettext(str)
#ifdef __cplusplus
#define restrict __restrict
#endif
#ifdef _WIN32
#define HAVE_WIN32 1
#define WIN32_LEAN_AND_MEAN 1
#include <winsock2.h>
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
struct pollfd;
int poll(struct pollfd *, unsigned, int);
int us_vasprintf(char **, const char *, va_list);
#define vasprintf us_vasprintf
#ifdef __cplusplus
}
#endif
#else
#define HAVE_POSIX_MEMALIGN 1
#define HAVE_UNISTD_H 1
#define HAVE_POLL_H 1
#endif
