/*
 * Standard pointer and size definitions for the Marvell 88W8787 ("Robin") firmware.
 */
#pragma once

typedef unsigned int size_t;
typedef int ptrdiff_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif
