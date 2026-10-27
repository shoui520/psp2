/* Named readonly objects in the original exception-report order. */
#pragma once

#if FW_DEBUG && !EXCPMGR_NSKBL
#include "excpmgr_report_text.h"
#include "excpmgr_fault_text.h"
#define EXCP_REPORT_TEXT(_name, _text) EXCP_TEXT_REF_##_name
#define EXCP_TEXT_REF_ifsrFormat g_excpmgrTextifsrFormat
#define EXCP_TEXT_REF_debugStatusFormat g_excpmgrTextdebugStatusFormat
#define EXCP_TEXT_REF_ifarFormat g_excpmgrTextifarFormat
#define EXCP_TEXT_REF_separator g_excpmgrTextseparator
#define EXCP_TEXT_REF_valid g_excpmgrTextvalid
#define EXCP_TEXT_REF_invalid g_excpmgrTextinvalid
#define EXCP_TEXT_REF_dfsrFormat g_excpmgrTextdfsrFormat
#define EXCP_TEXT_REF_domainFormat g_excpmgrTextdomainFormat
#define EXCP_TEXT_REF_invalidDomainFormat g_excpmgrTextinvalidDomainFormat
#define EXCP_TEXT_REF_dfarFormat g_excpmgrTextdfarFormat
#define EXCP_TEXT_REF_invalidContextFormat g_excpmgrTextinvalidContextFormat
#define EXCP_TEXT_REF_r0Format g_excpmgrTextr0Format
#define EXCP_TEXT_REF_r4Format g_excpmgrTextr4Format
#define EXCP_TEXT_REF_r8Format g_excpmgrTextr8Format
#define EXCP_TEXT_REF_r12Format g_excpmgrTextr12Format
#define EXCP_TEXT_REF_spFormat g_excpmgrTextspFormat
#define EXCP_TEXT_REF_lrFormat g_excpmgrTextlrFormat
#define EXCP_TEXT_REF_pcFormat g_excpmgrTextpcFormat
#define EXCP_TEXT_REF_cpsrFormat g_excpmgrTextcpsrFormat
#define EXCP_TEXT_REF_contextIdFormat g_excpmgrTextcontextIdFormat
#define EXCP_TEXT_REF_dacrFormat g_excpmgrTextdacrFormat
#define EXCP_TEXT_REF_ttbr1Format g_excpmgrTextttbr1Format
#define EXCP_TEXT_REF_tpidrurwFormat g_excpmgrTexttpidrurwFormat
#define EXCP_TEXT_REF_tpidruroFormat g_excpmgrTexttpidruroFormat
#define EXCP_TEXT_REF_tpidrprwFormat g_excpmgrTexttpidrprwFormat
#define EXCP_TEXT_REF_vfpFormat g_excpmgrTextvfpFormat
#define EXCP_TEXT_REF_fpexcFormat g_excpmgrTextfpexcFormat
#define EXCP_TEXT_REF_fpscrFormat g_excpmgrTextfpscrFormat
#define EXCP_TEXT_REF_arm g_excpmgrTextarm
#define EXCP_TEXT_REF_jazelle g_excpmgrTextjazelle
#define EXCP_TEXT_REF_thumb16 g_excpmgrTextthumb16
#define EXCP_TEXT_REF_thumbEE16 g_excpmgrTextthumbEE16
#define EXCP_TEXT_REF_thumbEE32 g_excpmgrTextthumbEE32
#define EXCP_TEXT_REF_thumb32 g_excpmgrTextthumb32
#define EXCP_TEXT_REF_waypointHeader g_excpmgrTextwaypointHeader
#define EXCP_TEXT_REF_waypointExceptionFormat g_excpmgrTextwaypointExceptionFormat
#define EXCP_TEXT_REF_waypointEventFormat g_excpmgrTextwaypointEventFormat
#define EXCP_TEXT_REF_fingerprintFormat g_excpmgrTextfingerprintFormat
#define EXCP_TEXT_REF_waypointModuleFormat g_excpmgrTextwaypointModuleFormat
#define EXCP_TEXT_REF_waypointNoModuleFormat g_excpmgrTextwaypointNoModuleFormat
#define EXCP_TEXT_REF_processIdFormat g_excpmgrTextprocessIdFormat
#define EXCP_TEXT_REF_processIdNameFormat g_excpmgrTextprocessIdNameFormat
#define EXCP_TEXT_REF_threadIdFormat g_excpmgrTextthreadIdFormat
#define EXCP_TEXT_REF_threadIdNameFormat g_excpmgrTextthreadIdNameFormat
#define EXCP_TEXT_REF_interruptContext g_excpmgrTextinterruptContext
#define EXCP_TEXT_REF_handlerFormat g_excpmgrTexthandlerFormat
#define EXCP_TEXT_REF_emptyFormat g_excpmgrTextemptyFormat
#define EXCP_TEXT_REF_defaultCpsrFormat g_excpmgrTextdefaultCpsrFormat
#define EXCP_TEXT_REF_defaultSpsrFormat g_excpmgrTextdefaultSpsrFormat
#define EXCP_TEXT_REF_undef g_excpmgrTextundef
#define EXCP_TEXT_REF_pabt g_excpmgrTextpabt
#define EXCP_TEXT_REF_dabt g_excpmgrTextdabt
#define EXCP_TEXT_REF_nested g_excpmgrTextnested
#define EXCP_TEXT_REF_vfpBlockName g_excpmgrTextvfpBlockName
#define EXCP_TEXT_REF_undefHeader g_excpmgrTextundefHeader
#define EXCP_TEXT_REF_processNameErrorFormat g_excpmgrTextprocessNameErrorFormat
#define EXCP_TEXT_REF_processNameFormat g_excpmgrTextprocessNameFormat
#define EXCP_TEXT_REF_threadNameErrorFormat g_excpmgrTextthreadNameErrorFormat
#define EXCP_TEXT_REF_threadNameFormat g_excpmgrTextthreadNameFormat
#define EXCP_TEXT_REF_pabtHeader g_excpmgrTextpabtHeader
#define EXCP_TEXT_REF_dabtHeader g_excpmgrTextdabtHeader
#define EXCP_TEXT_REF_fault00 (g_faultText.fault00)
#define EXCP_TEXT_REF_alignmentFault (g_faultText.alignmentFault)
#define EXCP_TEXT_REF_debugEvent (g_faultText.debugEvent)
#define EXCP_TEXT_REF_sectionAccessFlag (g_faultText.sectionAccessFlag)
#define EXCP_TEXT_REF_instructionCacheMaintenance (g_faultText.instructionCacheMaintenance)
#define EXCP_TEXT_REF_sectionTranslation (g_faultText.sectionTranslation)
#define EXCP_TEXT_REF_pageAccessFlag (g_faultText.pageAccessFlag)
#define EXCP_TEXT_REF_pageTranslation (g_faultText.pageTranslation)
#define EXCP_TEXT_REF_synchronousExternalAbort (g_faultText.synchronousExternalAbort)
#define EXCP_TEXT_REF_sectionDomain (g_faultText.sectionDomain)
#define EXCP_TEXT_REF_fault0A (g_faultText.fault0A)
#define EXCP_TEXT_REF_pageDomain (g_faultText.pageDomain)
#define EXCP_TEXT_REF_firstTranslationExternalAbort (g_faultText.firstTranslationExternalAbort)
#define EXCP_TEXT_REF_sectionPermission (g_faultText.sectionPermission)
#define EXCP_TEXT_REF_secondTranslationExternalAbort (g_faultText.secondTranslationExternalAbort)
#define EXCP_TEXT_REF_pagePermission (g_faultText.pagePermission)
#define EXCP_TEXT_REF_fault10 (g_faultText.fault10)
#define EXCP_TEXT_REF_fault11 (g_faultText.fault11)
#define EXCP_TEXT_REF_fault12 (g_faultText.fault12)
#define EXCP_TEXT_REF_fault13 (g_faultText.fault13)
#define EXCP_TEXT_REF_lockdown (g_faultText.lockdown)
#define EXCP_TEXT_REF_fault15 (g_faultText.fault15)
#define EXCP_TEXT_REF_asynchronousExternalAbort (g_faultText.asynchronousExternalAbort)
#define EXCP_TEXT_REF_fault17 (g_faultText.fault17)
#define EXCP_TEXT_REF_fault18 (g_faultText.fault18)
#define EXCP_TEXT_REF_memoryParity (g_faultText.memoryParity)
#define EXCP_TEXT_REF_coprocessorAbort (g_faultText.coprocessorAbort)
#define EXCP_TEXT_REF_fault1B (g_faultText.fault1B)
#define EXCP_TEXT_REF_firstTranslationParity (g_faultText.firstTranslationParity)
#define EXCP_TEXT_REF_fault1D (g_faultText.fault1D)
#define EXCP_TEXT_REF_secondTranslationParity (g_faultText.secondTranslationParity)
#define EXCP_TEXT_REF_fault1F (g_faultText.fault1F)
#define EXCP_TEXT_REF_fault01 g_fault01
#define EXCP_TEXT_REF_fault04 g_fault04
#define EXCP_TEXT_REF_fault16 g_fault16
#include "excpmgr_debug_events.h"

/* These strings are individually aligned in Sony's readonly region. */
#define EXCP_DEBUG_EVENT_FIELD(_index, _text) \
	char text##_index[sizeof(_text)] __attribute__((aligned(4)));
typedef struct ExcpDebugEventText {
	SCE_EXCP_DEBUG_EVENTS(EXCP_DEBUG_EVENT_FIELD)
} ExcpDebugEventText;
#undef EXCP_DEBUG_EVENT_FIELD
extern const ExcpDebugEventText g_debugEventText;
extern const char g_conditionText[16][4];
extern const unsigned int thumbNop;
extern const unsigned int armNop;
extern const SceKernelDebugInfo g_savedRegistersTpidrprwInfo;
extern const SceKernelDebugInfo g_registerHandlerPriorityInfo;
extern const SceKernelDebugInfo g_registerHandlerCodeInfo;
extern const SceKernelDebugInfo g_registerHandlerNullInfo;
extern const SceKernelDebugInfo g_panicUndefInfo;
extern const SceKernelDebugInfo g_defaultCpsrInfo;
typedef struct ExcpExceptionText {
	char empty[1] __attribute__((aligned(4)));
	char halting[sizeof("Halting Debug-mode")] __attribute__((aligned(4)));
	char secureMonitor[sizeof("Secure Monitor")] __attribute__((aligned(4)));
	char three[sizeof("3")] __attribute__((aligned(4)));
	char impreciseAbort[sizeof("Imprecise Data Abort")] __attribute__((aligned(4)));
	char t2eeTrap[sizeof("T2EE Trap")] __attribute__((aligned(4)));
	char reset[sizeof("Reset")] __attribute__((aligned(4)));
	char undefined[sizeof("Undefined")] __attribute__((aligned(4)));
	char svc[sizeof("SVC")] __attribute__((aligned(4)));
	char prefetchAbort[sizeof("Prefetch Abort")] __attribute__((aligned(4)));
	char preciseAbort[sizeof("Precise Data Abort")] __attribute__((aligned(4)));
	char softwareWatchpoint[sizeof("Software Watchpoint")] __attribute__((aligned(4)));
	char irq[sizeof("IRQ")] __attribute__((aligned(4)));
	char fiq[sizeof("FIQ")] __attribute__((aligned(4)));
} ExcpExceptionText;
extern const ExcpExceptionText g_exceptionText;
extern const char *const exceptionNames[16];
extern const SceKernelDebugInfo g_savedRegistersR8Info;
extern const SceKernelDebugInfo g_defaultHandlerInfo;
extern const SceKernelDebugInfo g_releaseHandlerInfo;
extern const SceKernelDebugInfo g_releaseHandlerCodeInfo;
extern const SceKernelDebugInfo g_releaseHandlerNullInfo;
extern const SceKernelDebugInfo g_releaseHandlerLinkInfo;
extern const SceKernelDebugInfo g_releaseHandlerRestoreInfo;
extern const SceKernelDebugInfo g_savedRegistersR12Info;
extern const SceKernelDebugInfo g_savedRegistersSpInfo;
extern const SceKernelDebugInfo g_savedRegistersLrInfo;
extern const SceKernelDebugInfo g_savedRegistersPcInfo;
extern const SceKernelDebugInfo g_savedRegistersContextIdInfo;
extern const SceKernelDebugInfo g_savedRegistersDacrInfo;
extern const SceKernelDebugInfo g_savedRegistersTtbr1Info;
extern const SceKernelDebugInfo g_savedRegistersTpidrurwInfo;
extern const SceKernelDebugInfo g_savedRegistersTpidruroInfo;
extern const SceKernelDebugInfo g_ifsrInfo;
extern const SceKernelDebugInfo g_ifsrDebugStatusInfo;
extern const SceKernelDebugInfo g_ifsrAddressInfo;
extern const SceKernelDebugInfo g_printWPTHeaderInfo;
extern const SceKernelDebugInfo g_panicCommonProcessIdInfo;
extern const SceKernelDebugInfo g_panicCommonProcessNameInfo;
extern const SceKernelDebugInfo g_panicCommonThreadIdInfo;
extern const SceKernelDebugInfo g_panicCommonThreadNameInfo;
extern const SceKernelDebugInfo g_panicCommonContextInfo;
extern const SceKernelDebugInfo g_initialUndefProcessNameErrorInfo;
extern const SceKernelDebugInfo g_initialDabtProcessIdInfo;
extern const SceKernelDebugInfo g_initialDabtThreadIdInfo;
extern const SceKernelDebugInfo g_initialDabtContextInfo;
extern const SceKernelDebugInfo g_initialDabtProcessNameErrorInfo;
extern const SceKernelDebugInfo g_initialDabtProcessNameInfo;
extern const SceKernelDebugInfo g_initialDabtThreadNameErrorInfo;
extern const SceKernelDebugInfo g_initialDabtThreadNameInfo;
extern const char g_releaseHandlerName[];
extern const char g_panicCommonName[];
extern const char g_registerHandlerName[];
extern const char g_fault01[];
extern const char g_fault04[];
extern const char g_fault16[];
extern const SceKernelDebugInfo g_printIFSRSeparatorInfo;
extern const SceKernelDebugInfo g_initialPabtExceptionInfo;
extern const SceKernelDebugInfo g_initialUndefThreadNameInfo;
extern const SceKernelDebugInfo g_initialPabtProcessIdInfo;
extern const SceKernelDebugInfo g_printWPTExceptionInfo;
extern const SceKernelDebugInfo g_printWPTEventInfo;
extern const SceKernelDebugInfo g_printWPTModuleInfo;
extern const SceKernelDebugInfo g_defaultResetInfo;
extern const SceKernelDebugInfo g_panicPabtInfo;
extern const SceKernelDebugInfo g_defaultSvcInfo;
extern const SceKernelDebugInfo g_panicDabtInfo;
extern const SceKernelDebugInfo g_defaultSpsrInfo;
extern const SceKernelDebugInfo g_registerDefaultInvalidHandlerInfo;
extern const SceKernelDebugInfo g_savedRegistersFlagsInfo;
extern const SceKernelDebugInfo g_savedRegistersEncodingInfo;
extern const SceKernelDebugInfo g_savedRegistersSeparatorInfo;
extern const SceKernelDebugInfo g_initialUndefProcessNameInfo;
extern const SceKernelDebugInfo g_initialUndefThreadNameErrorInfo;
extern const SceKernelDebugInfo g_initialPabtThreadIdInfo;
extern const SceKernelDebugInfo g_initialPabtContextInfo;
extern const SceKernelDebugInfo g_initialPabtProcessNameErrorInfo;
extern const SceKernelDebugInfo g_initialPabtProcessNameInfo;
extern const SceKernelDebugInfo g_initialPabtThreadNameErrorInfo;
extern const SceKernelDebugInfo g_initialPabtThreadNameInfo;
extern const SceKernelDebugInfo g_dfsrInfo;
extern const SceKernelDebugInfo g_dfsrDomainInfo;
extern const SceKernelDebugInfo g_dfsrInvalidDomainInfo;
extern const SceKernelDebugInfo g_dfsrAddressInfo;
extern const SceKernelDebugInfo g_printWPTNoModuleInfo;
extern const SceKernelDebugInfo g_savedRegistersHeaderInfo;
extern const SceKernelDebugInfo g_defaultReservedInfo;
extern const SceKernelDebugInfo g_panicNestedInfo;
extern const SceKernelDebugInfo g_defaultIrqInfo;
extern const SceKernelDebugInfo g_initialUndefExceptionInfo;
extern const SceKernelDebugInfo g_printVfpPairsInfo;
extern const SceKernelDebugInfo g_savedRegistersGeneralInfo;
extern const SceKernelDebugInfo g_initialUndefProcessIdInfo;
extern const SceKernelDebugInfo g_initialUndefThreadIdInfo;
extern const SceKernelDebugInfo g_initialUndefContextInfo;
extern const SceKernelDebugInfo g_printVfpFpexcInfo;
extern const SceKernelDebugInfo g_printVfpFpscrInfo;
extern const SceKernelDebugInfo g_printVfpSeparatorInfo;
extern const SceKernelDebugInfo g_initInfo;
extern const SceKernelDebugInfo g_dfsrSeparatorInfo;
extern const SceKernelDebugInfo g_initialDabtExceptionInfo;
extern const char g_excpmgrFileName[];
extern const char g_printIFSRName[];
extern const char g_initialPabtName[];
extern const char g_initialUndefName[];
extern const char g_printWPTName[];
extern const char g_printDFSRName[];
extern const char g_panicPabtName[];
extern const char g_panicDabtName[];
extern const char g_defaultResetName[];
extern const char g_defaultSvcName[];
extern const char g_defaultHandlerName[];
extern const char g_defaultReservedName[];
extern const char g_registerDefaultName[];
extern const char g_printVfpName[];
extern const char g_savedRegistersName[];
extern const char g_panicUndefName[];
extern const char g_initialDabtName[];
extern const char g_panicNestedName[];
extern const char g_defaultIrqName[];
extern const char g_initName[];
extern const char *const waypointNames[8];
extern const char g_waypointDirectBranch[];
extern const char g_waypointIndirectBranch[];
extern const char g_waypointException[];
extern const char g_waypointBarrier[];
extern const char g_waypointDebugEntry[];
extern const char g_waypointDebugExit[];
extern const char g_waypointSix[];
extern const char g_waypointSeven[];
extern const char g_domainNoAccessName[];
extern const char g_domainClientName[];
extern const char g_domainReservedName[];
extern const char g_domainManagerName[];
#define EXCP_IT_TEXT_DECLARE(_index) extern const char g_excpmgrITText##_index[];
EXCP_IT_TEXT_DECLARE(0)
EXCP_IT_TEXT_DECLARE(1)
EXCP_IT_TEXT_DECLARE(2)
EXCP_IT_TEXT_DECLARE(3)
EXCP_IT_TEXT_DECLARE(4)
EXCP_IT_TEXT_DECLARE(5)
EXCP_IT_TEXT_DECLARE(6)
EXCP_IT_TEXT_DECLARE(7)
EXCP_IT_TEXT_DECLARE(8)
EXCP_IT_TEXT_DECLARE(9)
EXCP_IT_TEXT_DECLARE(10)
EXCP_IT_TEXT_DECLARE(11)
EXCP_IT_TEXT_DECLARE(12)
EXCP_IT_TEXT_DECLARE(13)
EXCP_IT_TEXT_DECLARE(14)
EXCP_IT_TEXT_DECLARE(15)
#undef EXCP_IT_TEXT_DECLARE

/* Calls retain Sony's source lines while their records follow data order. */
#undef SCE_DBGINFO_FUNC
#define SCE_DBGINFO_FUNC(_line, _func) EXCP_DBGINFO_##_line(_func)
#define EXCP_DBGINFO_189(_func) (&g_registerHandlerNullInfo)
#define EXCP_DBGINFO_195(_func) (&g_registerHandlerCodeInfo)
#define EXCP_DBGINFO_200(_func) (&g_registerHandlerPriorityInfo)
#define EXCP_DBGINFO_242(_func) (&g_registerDefaultInvalidHandlerInfo)
#define EXCP_DBGINFO_276(_func) (&g_releaseHandlerInfo)
#define EXCP_DBGINFO_278(_func) (&g_releaseHandlerCodeInfo)
#define EXCP_DBGINFO_283(_func) (&g_releaseHandlerNullInfo)
#define EXCP_DBGINFO_288(_func) (&g_releaseHandlerLinkInfo)
#define EXCP_DBGINFO_295(_func) (&g_releaseHandlerRestoreInfo)
#define EXCP_DBGINFO_508(_func) (&g_defaultHandlerInfo)
#define EXCP_DBGINFO_509(_func) (&g_defaultCpsrInfo)
#define EXCP_DBGINFO_510(_func) (&g_defaultSpsrInfo)
#define EXCP_DBGINFO_522(_func) (&g_defaultResetInfo)
#define EXCP_DBGINFO_529(_func) (&g_defaultSvcInfo)
#define EXCP_DBGINFO_536(_func) (&g_defaultReservedInfo)
#define EXCP_DBGINFO_543(_func) (&g_defaultIrqInfo)
#define EXCP_DBGINFO_681(_func) (&g_savedRegistersGeneralInfo)
#define EXCP_DBGINFO_689(_func) (&g_savedRegistersHeaderInfo)
#define EXCP_DBGINFO_694(_func) (&g_savedRegistersFlagsInfo)
#define EXCP_DBGINFO_699(_func) (&g_savedRegistersR8Info)
#define EXCP_DBGINFO_701(_func) (&g_savedRegistersR12Info)
#define EXCP_DBGINFO_703(_func) (&g_savedRegistersSpInfo)
#define EXCP_DBGINFO_705(_func) (&g_savedRegistersLrInfo)
#define EXCP_DBGINFO_709(_func) (&g_savedRegistersPcInfo)
#define EXCP_DBGINFO_713(_func) (&g_savedRegistersContextIdInfo)
#define EXCP_DBGINFO_716(_func) (&g_savedRegistersDacrInfo)
#define EXCP_DBGINFO_718(_func) (&g_savedRegistersTtbr1Info)
#define EXCP_DBGINFO_720(_func) (&g_savedRegistersTpidrurwInfo)
#define EXCP_DBGINFO_723(_func) (&g_savedRegistersTpidruroInfo)
#define EXCP_DBGINFO_725(_func) (&g_savedRegistersTpidrprwInfo)
#define EXCP_DBGINFO_727(_func) (&g_savedRegistersEncodingInfo)
#define EXCP_DBGINFO_729(_func) (&g_savedRegistersSeparatorInfo)
#define EXCP_DBGINFO_778(_func) (&g_printVfpPairsInfo)
#define EXCP_DBGINFO_781(_func) (&g_printVfpFpexcInfo)
#define EXCP_DBGINFO_782(_func) (&g_printVfpFpscrInfo)
#define EXCP_DBGINFO_783(_func) (&g_printVfpSeparatorInfo)
#define EXCP_DBGINFO_849(_func) (&g_ifsrInfo)
#define EXCP_DBGINFO_852(_func) (&g_ifsrDebugStatusInfo)
#define EXCP_DBGINFO_856(_func) (&g_ifsrAddressInfo)
#define EXCP_DBGINFO_857(_func) (&g_printIFSRSeparatorInfo)
#define EXCP_DBGINFO_918(_func) (&g_dfsrInfo)
#define EXCP_DBGINFO_931(_func) (&g_dfsrDomainInfo)
#define EXCP_DBGINFO_933(_func) (&g_dfsrInvalidDomainInfo)
#define EXCP_DBGINFO_938(_func) (&g_dfsrAddressInfo)
#define EXCP_DBGINFO_940(_func) (&g_dfsrSeparatorInfo)
#define EXCP_DBGINFO_998(_func) (&g_printWPTHeaderInfo)
#define EXCP_DBGINFO_1015(_func) (&g_printWPTExceptionInfo)
#define EXCP_DBGINFO_1026(_func) (&g_printWPTEventInfo)
#define EXCP_DBGINFO_1069(_func) (&g_printWPTModuleInfo)
#define EXCP_DBGINFO_1078(_func) (&g_printWPTNoModuleInfo)
#define EXCP_DBGINFO_1135(_func) (&g_panicCommonProcessIdInfo)
#define EXCP_DBGINFO_1137(_func) (&g_panicCommonProcessNameInfo)
#define EXCP_DBGINFO_1143(_func) (&g_panicCommonThreadIdInfo)
#define EXCP_DBGINFO_1145(_func) (&g_panicCommonThreadNameInfo)
#define EXCP_DBGINFO_1148(_func) (&g_panicCommonContextInfo)
#define EXCP_DBGINFO_1204(_func) (&g_panicUndefInfo)
#define EXCP_DBGINFO_1212(_func) (&g_panicPabtInfo)
#define EXCP_DBGINFO_1220(_func) (&g_panicDabtInfo)
#define EXCP_DBGINFO_1228(_func) (&g_panicNestedInfo)
#define EXCP_DBGINFO_1649(_func) (&g_initInfo)
#define EXCP_DBGINFO_1813(_func) (&g_initialUndefExceptionInfo)
#define EXCP_DBGINFO_1822(_func) (&g_initialUndefProcessIdInfo)
#define EXCP_DBGINFO_1825(_func) (&g_initialUndefThreadIdInfo)
#define EXCP_DBGINFO_1827(_func) (&g_initialUndefContextInfo)
#define EXCP_DBGINFO_1861(_func) (&g_initialUndefProcessNameErrorInfo)
#define EXCP_DBGINFO_1863(_func) (&g_initialUndefProcessNameInfo)
#define EXCP_DBGINFO_1867(_func) (&g_initialUndefThreadNameErrorInfo)
#define EXCP_DBGINFO_1869(_func) (&g_initialUndefThreadNameInfo)
#define EXCP_DBGINFO_1893(_func) (&g_initialPabtExceptionInfo)
#define EXCP_DBGINFO_1902(_func) (&g_initialPabtProcessIdInfo)
#define EXCP_DBGINFO_1905(_func) (&g_initialPabtThreadIdInfo)
#define EXCP_DBGINFO_1907(_func) (&g_initialPabtContextInfo)
#define EXCP_DBGINFO_1944(_func) (&g_initialPabtProcessNameErrorInfo)
#define EXCP_DBGINFO_1946(_func) (&g_initialPabtProcessNameInfo)
#define EXCP_DBGINFO_1950(_func) (&g_initialPabtThreadNameErrorInfo)
#define EXCP_DBGINFO_1952(_func) (&g_initialPabtThreadNameInfo)
#define EXCP_DBGINFO_1982(_func) (&g_initialDabtExceptionInfo)
#define EXCP_DBGINFO_1994(_func) (&g_initialDabtProcessIdInfo)
#define EXCP_DBGINFO_1998(_func) (&g_initialDabtThreadIdInfo)
#define EXCP_DBGINFO_2000(_func) (&g_initialDabtContextInfo)
#define EXCP_DBGINFO_2035(_func) (&g_initialDabtProcessNameErrorInfo)
#define EXCP_DBGINFO_2037(_func) (&g_initialDabtProcessNameInfo)
#define EXCP_DBGINFO_2041(_func) (&g_initialDabtThreadNameErrorInfo)
#define EXCP_DBGINFO_2043(_func) (&g_initialDabtThreadNameInfo)
#endif
