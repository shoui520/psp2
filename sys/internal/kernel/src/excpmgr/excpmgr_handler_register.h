/* Shared exception register definitions. */
#pragma once

int sceKernelRegisterExceptionHandler(unsigned int code, unsigned int priority,
				      SceExcpmgrHandler *handler)
{
	SceExcpmgrHandler *link;
	unsigned int *words;
	unsigned int n;
	int flags;

	if (handler == NULL || ((unsigned int)handler & 2))
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (sceKernelIsColdBoot()) {
		words = (unsigned int *)((unsigned int)handler & ~3U);
		if (words[0] != 0 || words[1] != 0) {
			SCE_DBG_ASSERT_FAIL(189);
			return SCE_KERNEL_ERROR_ILLEGAL_EXCPHANDLER;
		}
	}
	if (code <= 7) {
		if (priority > 7) {
			SCE_DBG_ASSERT_FAIL(200);
			return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
		}
	} else {
		SCE_DBG_ASSERT_FAIL(195);
		return SCE_KERNEL_ERROR_ILLEGAL_EXCPCODE;
	}

	flags = sceKernelSpinlockLowLockCpuSuspendIntr(&g_excpHandlers.lock);
	for (n = priority; n != 0; n--) {
		if (g_excpHandlers.first[code][n - 1] != NULL) {
			excpPokeWord((unsigned int)handler,
				*(unsigned int *)((unsigned int)g_excpHandlers.first[code][n - 1] & ~3U));
			excpPokeWord((unsigned int)g_excpHandlers.first[code][n - 1], (unsigned int)handler);
			goto registered;
		}
	}
	/* nothing of higher priority: the handler becomes the head */
	link = g_excpHandlers.head[code];
	if (link == NULL)
		link = g_excpHandlers.defaultHandler;
	excpPokeWord((unsigned int)handler, (unsigned int)link);
	g_excpHandlers.head[code] = handler;
	excpSetVector(code, handler);

registered:
	if (g_excpHandlers.first[code][priority] == NULL)
		g_excpHandlers.first[code][priority] = handler;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_excpHandlers.lock, flags);
	return 0;
}

int sceKernelRegisterDefaultExceptionHandler(SceExcpmgrHandler *handler)
{
	SceExcpmgrHandler **end, **entry;
	SceExcpmgrHandler *link;
	unsigned int code, i;
	int flags;

	if (sceKernelIsColdBoot()) {
		if (handler->next != NULL || handler->reserved != 0) {
			SCE_DBG_ASSERT_FAIL(242);
			return SCE_KERNEL_ERROR_ILLEGAL_EXCPHANDLER;
		}
	}

	flags = sceKernelSpinlockLowLockCpuSuspendIntr(&g_excpHandlers.lock);
	excpPokeWord((unsigned int)handler, (unsigned int)g_excpHandlers.defaultHandler);
	g_excpHandlers.defaultHandler = handler;
	end = &g_excpHandlers.first[0][SCE_EXCP_PRIORITY_MAX];
	for (code = 0; code != SCE_EXCP_MAX; code++, end += SCE_EXCP_PRIORITY_MAX) {
		entry = end;
		for (i = 7; ; i--) {
			link = *--entry;
			if (__builtin_expect(link != NULL, 1)) {
				excpPokeWord((unsigned int)link, (unsigned int)handler);
				goto next;
			}
			if (i == 0)
				break;
		}
		g_excpHandlers.head[code] = handler;
		excpSetVector(code, handler);
next:
		;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_excpHandlers.lock, flags);
	return 0;
}

