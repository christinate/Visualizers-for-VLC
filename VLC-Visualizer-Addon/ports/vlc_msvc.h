#pragma once
#include <winsock2.h>
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#define __attribute__(x)
#define restrict __restrict
#define __builtin_constant_p(x) 0
#define __builtin_expect(x,y) (x)
#define VLC_DEPRECATED
#define N_(x) x
#define _(x) x
inline int poll(struct pollfd*, unsigned long, int) { return -1; }
