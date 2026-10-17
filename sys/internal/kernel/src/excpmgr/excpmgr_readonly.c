/* The first interleaved exception descriptor/table group in Sony's order. */
#define SCE_DBGINFO_SECTION ".rodata"
#include "excpmgr.h"
#include "excpmgr_readonly.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-macro-redefined"
#undef __FILE_NAME__
#define __FILE_NAME__ g_excpmgrFileName
#pragma GCC diagnostic pop

#if FW_DEBUG
#include "excpmgr_debug_events.h"
#define EXCP_REPORT_EXTERNAL_DATA
#define EXCP_IT_TEXT(_index) g_excpmgrITText##_index
#define EXCP_DEBUG_EVENT_ENTRY(_index, _text) g_debugEventText.text##_index,
#include "excpmgr_report_data.h"

/* GCC emits these explicit readonly definitions in reverse declaration order. */
const char *const ifsrNames[] = EXCP_IFSR_NAMES;
SCE_DBGINFO_DEFINE(, g_savedRegistersTpidrprwInfo, 725, g_savedRegistersName);
const char g_defaultSvcName[] __attribute__((aligned(4))) =
	"sceKernelDefaultHandlerSvcCfunc";
const ExcpFaultStatus dfsrStatus[] = EXCP_DFSR_STATUS;
const char *const conditionNames[] = {
	g_conditionText[0], g_conditionText[1], g_conditionText[2], g_conditionText[3],
	g_conditionText[4], g_conditionText[5], g_conditionText[6], g_conditionText[7],
	g_conditionText[8], g_conditionText[9], g_conditionText[10], g_conditionText[11],
	g_conditionText[12], g_conditionText[13], g_conditionText[14], g_conditionText[15],
};
SCE_DBGINFO_DEFINE(, g_registerHandlerPriorityInfo, 200, g_registerHandlerName);
SCE_DBGINFO_DEFINE(, g_registerHandlerCodeInfo, 195, g_registerHandlerName);
const ExcpExceptionText g_exceptionText = {
	.empty = "",
	.halting = "Halting Debug-mode",
	.secureMonitor = "Secure Monitor",
	.three = "3",
	.impreciseAbort = "Imprecise Data Abort",
	.t2eeTrap = "T2EE Trap",
	.reset = "Reset",
	.undefined = "Undefined",
	.svc = "SVC",
	.prefetchAbort = "Prefetch Abort",
	.preciseAbort = "Precise Data Abort",
	.softwareWatchpoint = "Software Watchpoint",
	.irq = "IRQ",
	.fiq = "FIQ",
};
SCE_DBGINFO_DEFINE(, g_registerHandlerNullInfo, 189, g_registerHandlerName);
const char g_registerDefaultName[] __attribute__((aligned(4))) =
	"sceKernelRegisterDefaultExceptionHandler";
SCE_DBGINFO_DEFINE(, g_panicUndefInfo, 1204, g_panicUndefName);
SCE_DBGINFO_DEFINE(, g_defaultCpsrInfo, 509, g_defaultHandlerName);
#define EXCP_FAULT_TEXT_INITIALIZER(_name, _text) ._name = _text,
const ExcpFaultText g_faultText = {
	EXCP_FAULT_TEXTS(EXCP_FAULT_TEXT_INITIALIZER)
};
#undef EXCP_FAULT_TEXT_INITIALIZER
SCE_DBGINFO_DEFINE(, g_defaultHandlerInfo, 508, g_defaultHandlerName);
SCE_DBGINFO_DEFINE(, g_savedRegistersR8Info, 699, g_savedRegistersName);
const char *const exceptionNames[] = {
	g_exceptionText.empty, g_exceptionText.halting, g_exceptionText.secureMonitor,
	g_exceptionText.three, g_exceptionText.impreciseAbort, g_exceptionText.t2eeTrap,
	g_waypointSix, g_waypointSeven, g_exceptionText.reset, g_exceptionText.undefined,
	g_exceptionText.svc, g_exceptionText.prefetchAbort, g_exceptionText.preciseAbort,
	g_exceptionText.softwareWatchpoint, g_exceptionText.irq, g_exceptionText.fiq,
};
SCE_DBGINFO_DEFINE(, g_releaseHandlerInfo, 276, g_releaseHandlerName);
const char g_defaultResetName[] __attribute__((aligned(4))) =
	"sceKernelDefaultHandlerResetCfunc";
const char g_excpmgrFileName[] __attribute__((aligned(4))) = "excpmgr.c";
SCE_DBGINFO_DEFINE(, g_initialDabtThreadNameErrorInfo, 2041, g_initialDabtName);
SCE_DBGINFO_DEFINE(, g_initialDabtProcessNameInfo, 2037, g_initialDabtName);
SCE_DBGINFO_DEFINE(, g_initialDabtProcessNameErrorInfo, 2035, g_initialDabtName);
SCE_DBGINFO_DEFINE(, g_panicCommonContextInfo, 1148, g_panicCommonName);
SCE_DBGINFO_DEFINE(, g_panicCommonThreadNameInfo, 1145, g_panicCommonName);
SCE_DBGINFO_DEFINE(, g_panicCommonThreadIdInfo, 1143, g_panicCommonName);
SCE_DBGINFO_DEFINE(, g_panicCommonProcessNameInfo, 1137, g_panicCommonName);
SCE_DBGINFO_DEFINE(, g_panicCommonProcessIdInfo, 1135, g_panicCommonName);
SCE_DBGINFO_DEFINE(, g_ifsrAddressInfo, 856, g_printIFSRName);
SCE_DBGINFO_DEFINE(, g_ifsrDebugStatusInfo, 852, g_printIFSRName);
SCE_DBGINFO_DEFINE(, g_ifsrInfo, 849, g_printIFSRName);
SCE_DBGINFO_DEFINE(, g_savedRegistersTpidruroInfo, 723, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_savedRegistersTpidrurwInfo, 720, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_savedRegistersTtbr1Info, 718, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_savedRegistersDacrInfo, 716, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_savedRegistersContextIdInfo, 713, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_savedRegistersPcInfo, 709, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_savedRegistersLrInfo, 705, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_savedRegistersSpInfo, 703, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_savedRegistersR12Info, 701, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_initialUndefProcessNameErrorInfo, 1861, g_initialUndefName);
SCE_DBGINFO_DEFINE(, g_releaseHandlerRestoreInfo, 295, g_releaseHandlerName);
SCE_DBGINFO_DEFINE(, g_releaseHandlerLinkInfo, 288, g_releaseHandlerName);
SCE_DBGINFO_DEFINE(, g_releaseHandlerNullInfo, 283, g_releaseHandlerName);
SCE_DBGINFO_DEFINE(, g_releaseHandlerCodeInfo, 278, g_releaseHandlerName);
SCE_DBGINFO_DEFINE(, g_initialDabtThreadNameInfo, 2043, g_initialDabtName);
const char g_panicCommonName[] __attribute__((aligned(4))) =
	"_sceKernelPanicHandlerCommon";
const char g_releaseHandlerName[] __attribute__((aligned(4))) =
	"sceKernelReleaseExceptionHandler";
SCE_DBGINFO_DEFINE(, g_initialDabtContextInfo, 2000, g_initialDabtName);
SCE_DBGINFO_DEFINE(, g_initialDabtThreadIdInfo, 1998, g_initialDabtName);
const char g_domainManagerName[] __attribute__((aligned(4))) = "Manager";
const char g_domainReservedName[] __attribute__((aligned(4))) = "Reserved";
const char g_domainClientName[] __attribute__((aligned(4))) = "Client";
const char g_domainNoAccessName[] __attribute__((aligned(4))) = "No access";
SCE_DBGINFO_DEFINE(, g_initialDabtProcessIdInfo, 1994, g_initialDabtName);
SCE_DBGINFO_DEFINE(, g_printWPTHeaderInfo, 998, g_printWPTName);
const char g_registerHandlerName[] __attribute__((aligned(4))) =
	"sceKernelRegisterExceptionHandler";
const char g_fault16[] __attribute__((aligned(4))) = "0x16";
const char g_fault04[] __attribute__((aligned(4))) = "0x04";
const char g_fault01[] __attribute__((aligned(4))) = "0x01";
const char g_initName[] __attribute__((aligned(4))) = "Init";
const char *const itThen[] = EXCP_IT_THEN;
#define EXCP_REPORT_TEXT_DEFINE(_name, _text) \
	const char g_excpmgrText##_name[] __attribute__((aligned(4))) = _text;
EXCP_REPORT_TEXTS(EXCP_REPORT_TEXT_DEFINE)
#undef EXCP_REPORT_TEXT_DEFINE
const unsigned int armNop = 0xe320f000;
const char g_savedRegistersName[] __attribute__((aligned(4))) =
	"printExcpSavedRegisters";
const char g_waypointSeven[] __attribute__((aligned(4))) = "7";
const char g_waypointSix[] __attribute__((aligned(4))) = "6";
const char g_waypointDebugExit[] __attribute__((aligned(4))) = "Debug exit";
const char g_waypointDebugEntry[] __attribute__((aligned(4))) = "Debug entry";
const char g_waypointBarrier[] __attribute__((aligned(4))) = "DMB/DSB/ISB";
const char g_waypointException[] __attribute__((aligned(4))) = "Exception";
const char g_waypointIndirectBranch[] __attribute__((aligned(4))) = "Indirect branch";
const char g_waypointDirectBranch[] __attribute__((aligned(4))) = "Direct   branch";
SCE_DBGINFO_DEFINE(, g_initialDabtExceptionInfo, 1982, g_initialDabtName);
SCE_DBGINFO_DEFINE(, g_dfsrSeparatorInfo, 940, g_printDFSRName);
SCE_DBGINFO_DEFINE(, g_initInfo, 1649, g_initName);
SCE_DBGINFO_DEFINE(, g_printVfpSeparatorInfo, 783, g_printVfpName);
SCE_DBGINFO_DEFINE(, g_printVfpFpscrInfo, 782, g_printVfpName);
SCE_DBGINFO_DEFINE(, g_printVfpFpexcInfo, 781, g_printVfpName);
SCE_DBGINFO_DEFINE(, g_initialUndefContextInfo, 1827, g_initialUndefName);
SCE_DBGINFO_DEFINE(, g_initialUndefThreadIdInfo, 1825, g_initialUndefName);
SCE_DBGINFO_DEFINE(, g_initialUndefProcessIdInfo, 1822, g_initialUndefName);
SCE_DBGINFO_DEFINE(, g_savedRegistersGeneralInfo, 681, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_printVfpPairsInfo, 778, g_printVfpName);
SCE_DBGINFO_DEFINE(, g_initialUndefExceptionInfo, 1813, g_initialUndefName);
const unsigned int thumbNop = 0xbf00bf00;
const char g_defaultIrqName[] __attribute__((aligned(4))) =
	"sceKernelDefaultHandlerIrqCfunc";
SCE_DBGINFO_DEFINE(, g_defaultIrqInfo, 543, g_defaultIrqName);
#define EXCP_DEBUG_EVENT_INITIALIZER(_index, _text) .text##_index = _text,
const ExcpDebugEventText g_debugEventText = {
	SCE_EXCP_DEBUG_EVENTS(EXCP_DEBUG_EVENT_INITIALIZER)
};
#undef EXCP_DEBUG_EVENT_INITIALIZER
SCE_DBGINFO_DEFINE(, g_panicNestedInfo, 1228, g_panicNestedName);
const char g_conditionText[16][4] = {
	"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
	"HI", "LS", "GE", "LT", "GT", "LE", "AL", "  ",
};
SCE_DBGINFO_DEFINE(, g_defaultReservedInfo, 536, g_defaultReservedName);
SCE_DBGINFO_DEFINE(, g_savedRegistersHeaderInfo, 689, g_savedRegistersName);
const char g_panicNestedName[] __attribute__((aligned(4))) =
	"sceKernelPanicHandlerReturnFromNestedExceptionCfunc";
const char g_defaultHandlerName[] __attribute__((aligned(4))) =
	"sceKernelDefaultHandlerCfunc";
const char g_initialDabtName[] __attribute__((aligned(4))) =
	"sceKernelInitialHandlerDebugDabtCfunc";
SCE_DBGINFO_DEFINE(, g_printWPTNoModuleInfo, 1078, g_printWPTName);
SCE_DBGINFO_DEFINE(, g_dfsrAddressInfo, 938, g_printDFSRName);
SCE_DBGINFO_DEFINE(, g_dfsrInvalidDomainInfo, 933, g_printDFSRName);
SCE_DBGINFO_DEFINE(, g_dfsrDomainInfo, 931, g_printDFSRName);
const char g_printIFSRName[] __attribute__((aligned(4))) = "printIFSR";
const char g_panicUndefName[] __attribute__((aligned(4))) =
	"sceKernelPanicHandlerUndefCfunc";
SCE_DBGINFO_DEFINE(, g_dfsrInfo, 918, g_printDFSRName);
SCE_DBGINFO_DEFINE(, g_initialPabtThreadNameInfo, 1952, g_initialPabtName);
SCE_DBGINFO_DEFINE(, g_initialPabtThreadNameErrorInfo, 1950, g_initialPabtName);
SCE_DBGINFO_DEFINE(, g_initialPabtProcessNameInfo, 1946, g_initialPabtName);
SCE_DBGINFO_DEFINE(, g_initialPabtProcessNameErrorInfo, 1944, g_initialPabtName);
SCE_DBGINFO_DEFINE(, g_initialPabtContextInfo, 1907, g_initialPabtName);
SCE_DBGINFO_DEFINE(, g_initialPabtThreadIdInfo, 1905, g_initialPabtName);
SCE_DBGINFO_DEFINE(, g_initialUndefThreadNameErrorInfo, 1867, g_initialUndefName);
SCE_DBGINFO_DEFINE(, g_initialUndefProcessNameInfo, 1863, g_initialUndefName);
const char *const waypointNames[] = {
	g_waypointDirectBranch, g_waypointIndirectBranch, g_waypointException,
	g_waypointBarrier, g_waypointDebugEntry, g_waypointDebugExit,
	g_waypointSix, g_waypointSeven,
};
const char g_initialPabtName[] __attribute__((aligned(4))) =
	"sceKernelInitialHandlerDebugPabtCfunc";
SCE_DBGINFO_DEFINE(, g_savedRegistersSeparatorInfo, 729, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_savedRegistersEncodingInfo, 727, g_savedRegistersName);
SCE_DBGINFO_DEFINE(, g_savedRegistersFlagsInfo, 694, g_savedRegistersName);
const char g_defaultReservedName[] __attribute__((aligned(4))) =
	"sceKernelDefaultHandlerReservedCfunc";
const char g_initialUndefName[] __attribute__((aligned(4))) =
	"sceKernelInitialHandlerDebugUndefCfunc";
SCE_DBGINFO_DEFINE(, g_registerDefaultInvalidHandlerInfo, 242, g_registerDefaultName);
const char *const domainNames[] = {
	g_domainNoAccessName, g_domainClientName, g_domainReservedName, g_domainManagerName,
};
SCE_DBGINFO_DEFINE(, g_defaultSpsrInfo, 510, g_defaultHandlerName);
const char g_printVfpName[] __attribute__((aligned(4))) = "printVfpRegisters";
SCE_DBGINFO_DEFINE(, g_panicDabtInfo, 1220, g_panicDabtName);
SCE_DBGINFO_DEFINE(, g_defaultSvcInfo, 529, g_defaultSvcName);
SCE_DBGINFO_DEFINE(, g_panicPabtInfo, 1212, g_panicPabtName);
SCE_DBGINFO_DEFINE(, g_defaultResetInfo, 522, g_defaultResetName);
const char g_panicDabtName[] __attribute__((aligned(4))) =
	"sceKernelPanicHandlerDabtCfunc";
const char *const itElse[] = EXCP_IT_ELSE;
SCE_DBGINFO_DEFINE(, g_printWPTModuleInfo, 1069, g_printWPTName);
const char g_panicPabtName[] __attribute__((aligned(4))) =
	"sceKernelPanicHandlerPabtCfunc";
SCE_DBGINFO_DEFINE(, g_printWPTEventInfo, 1026, g_printWPTName);
const char g_printDFSRName[] __attribute__((aligned(4))) = "printDFSR";
SCE_DBGINFO_DEFINE(, g_printWPTExceptionInfo, 1015, g_printWPTName);
const char g_printWPTName[] __attribute__((aligned(4))) = "printWPT";
#include "excpmgr_it_names.h"
#define EXCP_IT_TEXT_DEFINE(_index) \
	const char g_excpmgrITText##_index[] __attribute__((aligned(4))) = \
		SCE_EXCP_IT_TEXT_##_index;
EXCP_IT_TEXT_DEFINE(15)
EXCP_IT_TEXT_DEFINE(14)
EXCP_IT_TEXT_DEFINE(13)
EXCP_IT_TEXT_DEFINE(12)
EXCP_IT_TEXT_DEFINE(11)
EXCP_IT_TEXT_DEFINE(10)
EXCP_IT_TEXT_DEFINE(9)
EXCP_IT_TEXT_DEFINE(8)
EXCP_IT_TEXT_DEFINE(7)
EXCP_IT_TEXT_DEFINE(6)
EXCP_IT_TEXT_DEFINE(5)
EXCP_IT_TEXT_DEFINE(4)
EXCP_IT_TEXT_DEFINE(3)
EXCP_IT_TEXT_DEFINE(2)
EXCP_IT_TEXT_DEFINE(1)
EXCP_IT_TEXT_DEFINE(0)
#undef EXCP_IT_TEXT_DEFINE

SCE_DBGINFO_DEFINE(, g_initialPabtProcessIdInfo, 1902, g_initialPabtName);
SCE_DBGINFO_DEFINE(, g_initialUndefThreadNameInfo, 1869, g_initialUndefName);
SCE_DBGINFO_DEFINE(, g_initialPabtExceptionInfo, 1893, g_initialPabtName);

const char *const debugEventNames[] = EXCP_DEBUG_EVENT_NAMES;
#undef EXCP_DEBUG_EVENT_ENTRY

SCE_DBGINFO_DEFINE(, g_printIFSRSeparatorInfo, 857, g_printIFSRName);
#endif
