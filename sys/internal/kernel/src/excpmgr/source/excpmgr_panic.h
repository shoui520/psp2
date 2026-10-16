/* Common C policy for terminal exception handlers. */
#pragma once
#include "excpmgr.h"

void excpPanicLeave(SceExcpmgrContext *context) __attribute__((noreturn));

static inline void __attribute__((noreturn, always_inline))
excpPanicDispatch(SceExcpmgrContext *context, unsigned int arg,
		  void (*panic)(SceExcpmgrContext *context))
{
	int code = arg & 0xffff;

	if (code >= 5)
		for (;;)
			;
	switch (code) {
	case 0:
	case 1:
	case 2:
	case 4:
		excpPanicLeave(context);
	case 3:
		panic(context);
		for (;;)
			;
	}
	for (;;)
		;
}
