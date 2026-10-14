/* Shared exception report definitions. */
#pragma once

#ifndef EXCP_REPORT_LINKAGE
#define EXCP_REPORT_LINKAGE static
#endif

#include "sce/excpmgr_report_data.h"
#include "sce/compiler.h"

static inline unsigned int faultStatus(unsigned int fsr)
{
	return (fsr & 15) | (fsr >> 10 & 1) << 4;
}

EXCP_REPORT_LINKAGE void __attribute__((unused)) printIFSR(SceExcpmgrContext *context)
{
#if FW_DEBUG
	unsigned int ifsr = context->ifsr;

	SCE_DBG_PRINTF(0, 849,
		EXCP_REPORT_TEXT(ifsrFormat, "IFSR           0x%08X [ %s ]\n"), ifsr,
		ifsrNames[faultStatus(ifsr)]);
	switch (ifsr & 0x140f) {
	case 2:
		SCE_DBG_PRINTF(0, 852,
			EXCP_REPORT_TEXT(debugStatusFormat, "DBGDSCR        0x%08X [ %s ]\n"), context->dbgdscr,
			debugEventNames[context->dbgdscr >> 2 & 15]);
		break;
	}
	SCE_DBG_PRINTF(0, 856,
		EXCP_REPORT_TEXT(ifarFormat, "IFAR           0x%08X\n"), context->ifar);
	SCE_DBG_PRINTF(0, 857, EXCP_REPORT_TEXT(separator, "\n"));
#endif
}

EXCP_REPORT_LINKAGE void __attribute__((unused)) printDFSR(SceExcpmgrContext *context)
{
#if FW_DEBUG
	unsigned int dfsr = context->dfsr;
	const ExcpFaultStatus *status = &dfsrStatus[faultStatus(dfsr)];
	unsigned int domain;

	SCE_DBG_PRINTF(0, 918,
		EXCP_REPORT_TEXT(dfsrFormat, "DFSR           0x%08X [ %s ]\n"), dfsr, status->name);
	if (status->domainValid) {
		domain = dfsr >> 4 & 15;
		SCE_DBG_PRINTF(0, 931,
			EXCP_REPORT_TEXT(domainFormat, "DFSR DOMAIN    0x%X [ %s ]\n"), domain,
			domainNames[context->dacr >> (domain * 2) & 3]);
	} else {
		SCE_DBG_PRINTF(0, 933,
			EXCP_REPORT_TEXT(invalidDomainFormat, "DFSR DOMAIN    [ invalid ]\n"));
	}
	SCE_DBG_PRINTF(0, 938,
		EXCP_REPORT_TEXT(dfarFormat, "DFAR           0x%08X [ %s ]\n"), context->dfar,
		status->farValid ? EXCP_REPORT_TEXT(valid, "valid") :
		EXCP_REPORT_TEXT(invalid, "invalid"));
	SCE_DBG_PRINTF(0, 940, EXCP_REPORT_TEXT(separator, "\n"));
#endif
}

#define PUT(c)	(*out++ = (c))

EXCP_REPORT_LINKAGE void __attribute__((unused)) printExcpSavedRegisters(SceExcpmgrContext *context)
{
#if FW_DEBUG
	char text[64], *out;
	const char *s;
	unsigned int cpsr, it;

	if (context == NULL) {
		SCE_DBG_PRINTF(0xb, 681,
			EXCP_REPORT_TEXT(invalidContextFormat, "  illegal base addr\n"));
		return;
	}
	SCE_DBG_PRINTF(0, 689,
		EXCP_REPORT_TEXT(r0Format, "r0-r3 : 0x%08X 0x%08X 0x%08X 0x%08X\n"),
		context->r[0], context->r[1], context->r[2], context->r[3]);
	SCE_DBG_PRINTF(0, 694,
		EXCP_REPORT_TEXT(r4Format, "r4-r7 : 0x%08X 0x%08X 0x%08X 0x%08X\n"),
		context->r[4], context->r[5], context->r[6], context->r[7]);
	SCE_DBG_PRINTF(0, 699,
		EXCP_REPORT_TEXT(r8Format, "r8-r11: 0x%08X 0x%08X 0x%08X 0x%08X\n"),
		context->r[8], context->r[9], context->r[10], context->r[11]);
	SCE_DBG_PRINTF(0, 701, EXCP_REPORT_TEXT(r12Format, "r12   : 0x%08X\n"), context->r[12]);
	SCE_DBG_PRINTF(0, 703, EXCP_REPORT_TEXT(spFormat, "SP    : 0x%08X\n"), context->sp);
	SCE_DBG_PRINTF(0, 705, EXCP_REPORT_TEXT(lrFormat, "LR    : 0x%08X\n"), context->lr);
	SCE_DBG_PRINTF(0, 709, EXCP_REPORT_TEXT(pcFormat, "PC    : 0x%08X\n"), context->pc);

	cpsr = context->cpsr;
	out = text;
	if ((int)cpsr < 0)
		PUT('N');
	else
		PUT('n');
	if ((int)(cpsr << 1) < 0)
		PUT('Z');
	else
		PUT('z');
	if ((int)(cpsr << 2) < 0)
		PUT('C');
	else
		PUT('c');
	if ((int)(cpsr << 3) < 0)
		PUT('V');
	else
		PUT('v');
	if ((int)(cpsr << 4) < 0)
		PUT('Q');
	else
		PUT('q');
	PUT(' ');
	PUT('G');
	PUT('E');
	if ((int)(cpsr << 12) < 0)
		PUT('3');
	else
		PUT('_');
	if ((int)(cpsr << 13) < 0)
		PUT('2');
	else
		PUT('_');
	if ((int)(cpsr << 14) < 0)
		PUT('1');
	else
		PUT('_');
	if ((int)(cpsr << 15) < 0)
		PUT('0');
	else
		PUT('_');
	it = (cpsr >> 25 & 3) | (cpsr >> 10 & 3) << 2;
	if (unlikely(it != 0)) {
		unsigned int cond = cpsr >> 12 & 15;

		PUT(' ');
		if ((cond & 1) == 0)
			s = itThen[it];
		else
			s = itElse[it];
		PUT(*s++);
		PUT(*s++);
		PUT(*s++);
		PUT(*s++);
		PUT(*s++);
		PUT(' ');
		s = conditionNames[cond];
		PUT(*s++);
		PUT(*s++);
	}
	PUT(' ');
	if ((int)(cpsr << 22) < 0)
		PUT('E');
	else
		PUT('e');
	if ((int)(cpsr << 23) < 0)
		PUT('A');
	else
		PUT('a');
	if ((int)(cpsr << 24) < 0)
		PUT('I');
	else
		PUT('i');
	if ((int)(cpsr << 25) < 0)
		PUT('F');
	else
		PUT('f');
	PUT(' ');
	switch (cpsr & 0x1000020) {
	case 0:
		PUT('A'); PUT('R'); PUT('M');
		break;
	case 0x20:
		PUT('T'); PUT('h'); PUT('u'); PUT('m'); PUT('b');
		break;
	case 0x1000000:
		PUT('J'); PUT('a'); PUT('z'); PUT('e'); PUT('l'); PUT('l'); PUT('e');
		break;
	case 0x1000020:
		PUT('T'); PUT('h'); PUT('u'); PUT('m'); PUT('b'); PUT('E'); PUT('E');
		break;
	}
	PUT(' ');
	switch (cpsr & 0x1f) {
	case 0x10:
		PUT('U'); PUT('s'); PUT('r');
		break;
	case 0x11:
		PUT('F'); PUT('i'); PUT('q');
		break;
	case 0x12:
		PUT('I'); PUT('r'); PUT('q');
		break;
	case 0x13:
		PUT('S'); PUT('v'); PUT('c');
		break;
	case 0x16:
		PUT('M'); PUT('o'); PUT('n');
		break;
	case 0x17:
		PUT('A'); PUT('b'); PUT('t');
		break;
	case 0x1b:
		PUT('U'); PUT('n'); PUT('d');
		break;
	case 0x1f:
		PUT('S'); PUT('y'); PUT('s');
		break;
	default:
		PUT('?'); PUT('?'); PUT('?');
		break;
	}
	*out = '\0';
	SCE_DBG_PRINTF(0, 713,
		EXCP_REPORT_TEXT(cpsrFormat, "CPSR  : 0x%08X [%s]\n"), cpsr, text);
	SCE_DBG_PRINTF(0, 716,
		EXCP_REPORT_TEXT(contextIdFormat, "Context ID : 0x%08X\n"), context->contextId);
	SCE_DBG_PRINTF(0, 718,
		EXCP_REPORT_TEXT(dacrFormat, "DACR       : 0x%08X\n"), context->dacr);
	SCE_DBG_PRINTF(0, 720,
		EXCP_REPORT_TEXT(ttbr1Format, "TTBR1      : 0x%08X\n"), context->ttbr1);
	SCE_DBG_PRINTF(0, 723,
		EXCP_REPORT_TEXT(tpidrurwFormat, "TPIDRURW   : 0x%08X\n"), readTpidrurw());
	SCE_DBG_PRINTF(0, 725,
		EXCP_REPORT_TEXT(tpidruroFormat, "TPIDRURO   : 0x%08X\n"), context->tpidruro);
	SCE_DBG_PRINTF(0, 727,
		EXCP_REPORT_TEXT(tpidrprwFormat, "TPIDRPRW   : 0x%08X\n"), context->thread);
	SCE_DBG_PRINTF(0, 729, EXCP_REPORT_TEXT(separator, "\n"));
#endif
}

#undef PUT

