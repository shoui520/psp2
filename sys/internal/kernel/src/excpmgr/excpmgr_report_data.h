/* Exception diagnostic tables, emitted once in each owner's native order. */
#pragma once

#ifndef EXCP_REPORT_TEXT
#define EXCP_REPORT_TEXT(_name, _text) _text
#endif

#include "excpmgr_it_names.h"
#include "excpmgr_debug_events.h"

#if FW_DEBUG
typedef struct ExcpFaultStatus {
	const char *name;
	unsigned short farValid;
	unsigned short domainValid;
} ExcpFaultStatus;

#ifndef EXCP_IT_TEXT
#define EXCP_IT_TEXT(_index) SCE_EXCP_IT_TEXT_##_index
#endif
#ifndef EXCP_DEBUG_EVENT_ENTRY
#define EXCP_DEBUG_EVENT_ENTRY(_index, _text) _text,
#endif

#define EXCP_IFSR_NAMES { \
	EXCP_REPORT_TEXT(fault00, "0x00"), \
	EXCP_REPORT_TEXT(fault01, "0x01"), \
	EXCP_REPORT_TEXT(debugEvent, "Debug event"), \
	EXCP_REPORT_TEXT(sectionAccessFlag, "Section Access Flag fault"), \
	EXCP_REPORT_TEXT(fault04, "0x04"), \
	EXCP_REPORT_TEXT(sectionTranslation, "Section Translation fault"), \
	EXCP_REPORT_TEXT(pageAccessFlag, "Page Access Flag fault"), \
	EXCP_REPORT_TEXT(pageTranslation, "Page Translation fault"), \
	EXCP_REPORT_TEXT(synchronousExternalAbort, "Synchronous external abort"), \
	EXCP_REPORT_TEXT(sectionDomain, "Section Domain fault"), \
	EXCP_REPORT_TEXT(fault0A, "0x0A"), \
	EXCP_REPORT_TEXT(pageDomain, "Page Domain fault"), \
	EXCP_REPORT_TEXT(firstTranslationExternalAbort, \
		"1st level Translation table walk synchronous external abort"), \
	EXCP_REPORT_TEXT(sectionPermission, "Section Permission fault"), \
	EXCP_REPORT_TEXT(secondTranslationExternalAbort, \
		"2nd level Translation table walk synchronous external abort"), \
	EXCP_REPORT_TEXT(pagePermission, "Page Permission fault"), \
	EXCP_REPORT_TEXT(fault10, "0x10"), \
	EXCP_REPORT_TEXT(fault11, "0x11"), \
	EXCP_REPORT_TEXT(fault12, "0x12"), \
	EXCP_REPORT_TEXT(fault13, "0x13"), \
	EXCP_REPORT_TEXT(lockdown, "Lockdown"), \
	EXCP_REPORT_TEXT(fault15, "0x15"), \
	EXCP_REPORT_TEXT(fault16, "0x16"), \
	EXCP_REPORT_TEXT(fault17, "0x17"), \
	EXCP_REPORT_TEXT(fault18, "0x18"), \
	EXCP_REPORT_TEXT(fault18, "0x18"), \
	EXCP_REPORT_TEXT(memoryParity, "Memory access synchronous parity error"), \
	EXCP_REPORT_TEXT(coprocessorAbort, "Coprocessor abort"), \
	EXCP_REPORT_TEXT(fault1B, "0x1B"), \
	EXCP_REPORT_TEXT(firstTranslationParity, \
		"1st level Translation table walk synchronous parity error"), \
	EXCP_REPORT_TEXT(fault1D, "0x1D"), \
	EXCP_REPORT_TEXT(secondTranslationParity, \
		"2nd level Translation table walk synchronous parity error"), \
	EXCP_REPORT_TEXT(fault1F, "0x1F"), \
}

#define EXCP_DFSR_STATUS { \
	{ EXCP_REPORT_TEXT(fault00, "0x00"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(alignmentFault, "Alignment fault"), 1, 0 }, \
	{ EXCP_REPORT_TEXT(debugEvent, "Debug event"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(sectionAccessFlag, "Section Access Flag fault"), 1, 0 }, \
	{ EXCP_REPORT_TEXT(instructionCacheMaintenance, "Instruction cache maintenance fault"), 1, 0 }, \
	{ EXCP_REPORT_TEXT(sectionTranslation, "Section Translation fault"), 1, 0 }, \
	{ EXCP_REPORT_TEXT(pageAccessFlag, "Page Access Flag fault"), 1, 1 }, \
	{ EXCP_REPORT_TEXT(pageTranslation, "Page Translation fault"), 1, 1 }, \
	{ EXCP_REPORT_TEXT(synchronousExternalAbort, "Synchronous external abort"), 1, 0 }, \
	{ EXCP_REPORT_TEXT(sectionDomain, "Section Domain fault"), 1, 1 }, \
	{ EXCP_REPORT_TEXT(fault0A, "0x0A"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(pageDomain, "Page Domain fault"), 1, 1 }, \
	{ EXCP_REPORT_TEXT(firstTranslationExternalAbort, \
		"1st level Translation table walk synchronous external abort"), 1, 0 }, \
	{ EXCP_REPORT_TEXT(sectionPermission, "Section Permission fault"), 1, 1 }, \
	{ EXCP_REPORT_TEXT(secondTranslationExternalAbort, \
		"2nd level Translation table walk synchronous external abort"), 1, 1 }, \
	{ EXCP_REPORT_TEXT(pagePermission, "Page Permission fault"), 1, 1 }, \
	{ EXCP_REPORT_TEXT(fault10, "0x10"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(fault11, "0x11"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(fault12, "0x12"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(fault13, "0x13"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(lockdown, "Lockdown"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(fault15, "0x15"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(asynchronousExternalAbort, "Asynchronous external abort"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(fault17, "0x17"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(fault18, "0x18"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(fault18, "0x18"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(memoryParity, "Memory access synchronous parity error"), 1, 0 }, \
	{ EXCP_REPORT_TEXT(coprocessorAbort, "Coprocessor abort"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(fault1B, "0x1B"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(firstTranslationParity, \
		"1st level Translation table walk synchronous parity error"), 1, 0 }, \
	{ EXCP_REPORT_TEXT(fault1D, "0x1D"), 0, 0 }, \
	{ EXCP_REPORT_TEXT(secondTranslationParity, \
		"2nd level Translation table walk synchronous parity error"), 1, 1 }, \
	{ EXCP_REPORT_TEXT(fault1F, "0x1F"), 0, 0 }, \
}

#define EXCP_DEBUG_EVENT_NAMES { SCE_EXCP_DEBUG_EVENTS(EXCP_DEBUG_EVENT_ENTRY) }
#define EXCP_DOMAIN_NAMES { "No access", "Client", "Reserved", "Manager" }
#define EXCP_IT_THEN { \
	EXCP_IT_TEXT(0), EXCP_IT_TEXT(15), EXCP_IT_TEXT(14), EXCP_IT_TEXT(13), \
	EXCP_IT_TEXT(12), EXCP_IT_TEXT(11), EXCP_IT_TEXT(10), EXCP_IT_TEXT(9), \
	EXCP_IT_TEXT(8), EXCP_IT_TEXT(7), EXCP_IT_TEXT(6), EXCP_IT_TEXT(5), \
	EXCP_IT_TEXT(4), EXCP_IT_TEXT(3), EXCP_IT_TEXT(2), EXCP_IT_TEXT(1), \
}
#define EXCP_IT_ELSE { \
	EXCP_IT_TEXT(0), EXCP_IT_TEXT(1), EXCP_IT_TEXT(2), EXCP_IT_TEXT(3), \
	EXCP_IT_TEXT(4), EXCP_IT_TEXT(5), EXCP_IT_TEXT(6), EXCP_IT_TEXT(7), \
	EXCP_IT_TEXT(8), EXCP_IT_TEXT(9), EXCP_IT_TEXT(10), EXCP_IT_TEXT(11), \
	EXCP_IT_TEXT(12), EXCP_IT_TEXT(13), EXCP_IT_TEXT(14), EXCP_IT_TEXT(15), \
}
#define EXCP_CONDITION_NAMES { \
	"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", \
	"HI", "LS", "GE", "LT", "GT", "LE", "AL", "  ", \
}

#ifdef EXCP_REPORT_EXTERNAL_DATA
extern const char *const ifsrNames[33];
extern const ExcpFaultStatus dfsrStatus[33];
extern const char *const debugEventNames[16];
extern const char *const domainNames[4];
extern const char *const itThen[16];
extern const char *const itElse[16];
extern const char *const conditionNames[16];
#else
static const char *const ifsrNames[] = EXCP_IFSR_NAMES;
static const ExcpFaultStatus dfsrStatus[] = EXCP_DFSR_STATUS;
static const char *const debugEventNames[] = EXCP_DEBUG_EVENT_NAMES;
static const char *const domainNames[] = EXCP_DOMAIN_NAMES;
static const char *const itThen[] = EXCP_IT_THEN;
static const char *const itElse[] = EXCP_IT_ELSE;
static const char *const conditionNames[] = EXCP_CONDITION_NAMES;
#endif
#endif
