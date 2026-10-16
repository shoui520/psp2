/* Sony's fault-status text pool, shared by the IFSR and DFSR tables. */
#pragma once

#define EXCP_FAULT_TEXTS(X) \
	X(fault00, "0x00") \
	X(alignmentFault, "Alignment fault") \
	X(debugEvent, "Debug event") \
	X(sectionAccessFlag, "Section Access Flag fault") \
	X(instructionCacheMaintenance, "Instruction cache maintenance fault") \
	X(sectionTranslation, "Section Translation fault") \
	X(pageAccessFlag, "Page Access Flag fault") \
	X(pageTranslation, "Page Translation fault") \
	X(synchronousExternalAbort, "Synchronous external abort") \
	X(sectionDomain, "Section Domain fault") \
	X(fault0A, "0x0A") \
	X(pageDomain, "Page Domain fault") \
	X(firstTranslationExternalAbort, "1st level Translation table walk synchronous external abort") \
	X(sectionPermission, "Section Permission fault") \
	X(secondTranslationExternalAbort, "2nd level Translation table walk synchronous external abort") \
	X(pagePermission, "Page Permission fault") \
	X(fault10, "0x10") \
	X(fault11, "0x11") \
	X(fault12, "0x12") \
	X(fault13, "0x13") \
	X(lockdown, "Lockdown") \
	X(fault15, "0x15") \
	X(asynchronousExternalAbort, "Asynchronous external abort") \
	X(fault17, "0x17") \
	X(fault18, "0x18") \
	X(memoryParity, "Memory access synchronous parity error") \
	X(coprocessorAbort, "Coprocessor abort") \
	X(fault1B, "0x1B") \
	X(firstTranslationParity, "1st level Translation table walk synchronous parity error") \
	X(fault1D, "0x1D") \
	X(secondTranslationParity, "2nd level Translation table walk synchronous parity error") \
	X(fault1F, "0x1F")

#define EXCP_FAULT_TEXT_FIELD(_name, _text) \
	char _name[sizeof(_text)] __attribute__((aligned(4)));
typedef struct ExcpFaultText {
	EXCP_FAULT_TEXTS(EXCP_FAULT_TEXT_FIELD)
} ExcpFaultText;
#undef EXCP_FAULT_TEXT_FIELD
extern const ExcpFaultText g_faultText;
