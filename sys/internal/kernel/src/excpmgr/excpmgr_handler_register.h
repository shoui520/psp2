/* Shared exception register definitions. */
#pragma once

int sceKernelRegisterExceptionHandler(unsigned int code, unsigned int priority,
				      SceExcpmgrHandler *handler)
{
	SceExcpmgrHandler *link;
	unsigned int *words;
	int flags, i;

	if (handler == NULL || ((unsigned int)handler & 2))
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (SceSysrootForKernel_D7198963()) {
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

	i = priority - 1;
	flags = SceCpuForDriver_D32ACE9E(&g_excpHandlers.lock);
	for (; i >= 0; i--)
		if (g_excpHandlers.first[code][i] != NULL)
			break;
	if (i < 0) {
		/* nothing of higher priority: the handler becomes the head */
		link = g_excpHandlers.head[code];
		if (link == NULL)
			link = g_excpHandlers.defaultHandler;
		excpPokeWord((unsigned int)handler, (unsigned int)link);
		g_excpHandlers.head[code] = handler;
		excpSetVector(code, handler);
	} else {
		/* insert after the last handler of the next higher priority */
		link = g_excpHandlers.first[code][i];
		excpPokeWord((unsigned int)handler,
			*(unsigned int *)((unsigned int)link & ~3U));
		excpPokeWord((unsigned int)link, (unsigned int)handler);
	}
	if (g_excpHandlers.first[code][priority] == NULL)
		g_excpHandlers.first[code][priority] = handler;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_excpHandlers.lock, flags);
	return 0;
}

int sceKernelRegisterDefaultExceptionHandler(SceExcpmgrHandler *handler)
{
	SceExcpmgrHandler *(*first)[SCE_EXCP_PRIORITY_MAX];
	SceExcpmgrHandler **entry, *link;
	unsigned int code, i;
	int flags;

	if (SceSysrootForKernel_D7198963()) {
		if (handler->next != NULL || handler->reserved != 0) {
			SCE_DBG_ASSERT_FAIL(242);
			return SCE_KERNEL_ERROR_ILLEGAL_EXCPHANDLER;
		}
	}

	flags = SceCpuForDriver_D32ACE9E(&g_excpHandlers.lock);
	excpPokeWord((unsigned int)handler, (unsigned int)g_excpHandlers.defaultHandler);
	g_excpHandlers.defaultHandler = handler;
	first = g_excpHandlers.first;
	for (code = 0; code < SCE_EXCP_MAX; code++, first++) {
		entry = *first + SCE_EXCP_PRIORITY_MAX;
		i = SCE_EXCP_PRIORITY_MAX - 1;
		do {
			link = *--entry;
			if (link != NULL)
				break;
		} while (i-- != 0);
		if (link == NULL) {
			g_excpHandlers.head[code] = handler;
			excpSetVector(code, handler);
		} else {
			excpPokeWord((unsigned int)link, (unsigned int)handler);
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_excpHandlers.lock, flags);
	return 0;
}

