/* Shared exception default definitions. */
#pragma once

void sceKernelDefaultHandlerCfunc(void)
{
#if FW_DEBUG
	unsigned int psr;

	SCE_DBG_PRINTF(0xb, 508, EXCP_REPORT_TEXT(emptyFormat, "()\n"));
	__asm__ volatile("mrs %0, cpsr" : "=r" (psr));
	SCE_DBG_PRINTF(0xb, 509, EXCP_REPORT_TEXT(defaultCpsrFormat, "cpsr     0x%08x\n"), psr);
	__asm__ volatile("mrs %0, spsr" : "=r" (psr));
	SCE_DBG_PRINTF(0xb, 510, EXCP_REPORT_TEXT(defaultSpsrFormat, "spsr     0x%08x\n"), psr);
#endif
	for (;;)
		;
}

void sceKernelDefaultHandlerResetCfunc(void)
{
	SCE_DBG_PRINTF(0xb, 522, EXCP_REPORT_TEXT(emptyFormat, "()\n"));
	sceKernelDefaultHandlerCfunc();
}

void sceKernelDefaultHandlerSvcCfunc(void)
{
	SCE_DBG_PRINTF(0xb, 529, EXCP_REPORT_TEXT(emptyFormat, "()\n"));
	sceKernelDefaultHandlerCfunc();
}

void sceKernelDefaultHandlerReservedCfunc(void)
{
	SCE_DBG_PRINTF(0xb, 536, EXCP_REPORT_TEXT(emptyFormat, "()\n"));
	sceKernelDefaultHandlerCfunc();
}

void sceKernelDefaultHandlerIrqCfunc(void)
{
	SCE_DBG_PRINTF(0xb, 543, EXCP_REPORT_TEXT(emptyFormat, "()\n"));
	sceKernelDefaultHandlerCfunc();
}

#if defined(EXCPMGR_SKBL)
void sceKernelDefaultHandlerFiqCfunc(void)
{
	SCE_DBG_PRINTF(0xb, 550, EXCP_REPORT_TEXT(emptyFormat, "()\n"));
	sceKernelDefaultHandlerCfunc();
}
#endif

void sceKernelPanicHandlerUndefCfunc(SceExcpmgrContext *context)
{
	_sceKernelPanicHandlerCommon(context, 6, SCE_DBGINFO(1204), EXCP_REPORT_TEXT(undef, "UNDEF"));
}

void sceKernelPanicHandlerPabtCfunc(SceExcpmgrContext *context)
{
	_sceKernelPanicHandlerCommon(context, 4, SCE_DBGINFO(1212), EXCP_REPORT_TEXT(pabt, "PABT"));
}

void sceKernelPanicHandlerDabtCfunc(SceExcpmgrContext *context)
{
	_sceKernelPanicHandlerCommon(context, 1, SCE_DBGINFO(1220), EXCP_REPORT_TEXT(dabt, "DABT"));
}

void sceKernelPanicHandlerReturnFromNestedExceptionCfunc(SceExcpmgrContext *context)
{
	_sceKernelPanicHandlerCommon(context, 1, SCE_DBGINFO(1228), EXCP_REPORT_TEXT(nested, "NEST"));
}

