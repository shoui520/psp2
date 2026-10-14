/* Original readonly order: named debug records interleaved with their text. */
#define SCE_DBGINFO_SECTION ".rodata"
#include "intrmgr.h"
#include "intrmgr_readonly.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-macro-redefined"
#undef __FILE_NAME__
#define __FILE_NAME__ g_intrmgrFileName
#pragma GCC diagnostic pop

#if FW_DEBUG
/* GCC emits explicit readonly definitions in reverse declaration order. */
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterSubIntrHandlerLine1005Info, 1005,
	g_intrSceKernelRegisterSubIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterSubIntrHandlerLine992Info, 992,
	g_intrSceKernelRegisterSubIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelEnableSubIntrLine1136Info, 1136,
	g_intrSceKernelEnableSubIntr);
const char g_intrInvalidFastMutexOwnerCfunc[] __attribute__((aligned(4), used)) =
	"invalidFastMutexOwnerCfunc";
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterSubIntrHandlerLine967Info, 967,
	g_intrSceKernelRegisterSubIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelGetIntrTargetCpuLine860Info, 860,
	g_intrSceKernelGetIntrTargetCpu);
const char g_intrSceKernelIntrMgrModuleStart[] __attribute__((aligned(4), used)) =
	"sceKernelIntrMgrModuleStart";
const char g_intrSceKernelEnableIntr[] __attribute__((aligned(4), used)) =
	"sceKernelEnableIntr";
SCE_DBGINFO_DEFINE(, g_intrSceKernelGetIntrTargetCpuLine864Info, 864,
	g_intrSceKernelGetIntrTargetCpu);
SCE_DBGINFO_DEFINE(, g_intrInvalidApiStatCfuncLine2321Info, 2321,
	g_intrInvalidApiStatCfunc);
SCE_DBGINFO_DEFINE(, g_intrInvalidFastMutexOwnerCfuncLine2341Info, 2341,
	g_intrInvalidFastMutexOwnerCfunc);
SCE_DBGINFO_DEFINE(, g_intrSceKernelSetIntrTargetCpuLine843Info, 843,
	g_intrSceKernelSetIntrTargetCpu);
SCE_DBGINFO_DEFINE(, g_intrSceKernelSetIntrTargetCpuLine838Info, 838,
	g_intrSceKernelSetIntrTargetCpu);
const char g_intrSceKernelReleaseIntrHookHandler[] __attribute__((aligned(4), used)) =
	"sceKernelReleaseIntrHookHandler";
const char g_intrSceKernelRegisterSubIntrHandler[] __attribute__((aligned(4), used)) =
	"sceKernelRegisterSubIntrHandler";
const char g_intrSceKernelDisableSubIntr[] __attribute__((aligned(4), used)) =
	"sceKernelDisableSubIntr";
SCE_DBGINFO_DEFINE(, g_intrSceKernelResumeIntrLine743Info, 743,
	g_intrSceKernelResumeIntr);
const char g_intrSceKernelIsSubInterruptOccurred[] __attribute__((aligned(4), used)) =
	"sceKernelIsSubInterruptOccurred";
const char g_intrSceKernelRegisterIntrHandler[] __attribute__((aligned(4), used)) =
	"sceKernelRegisterIntrHandler";
SCE_DBGINFO_DEFINE(, g_intrSceKernelSuspendIntrLine709Info, 709,
	g_intrSceKernelSuspendIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterIntrHookHandlerLine603Info, 603,
	g_intrSceKernelRegisterIntrHookHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterIntrHookHandlerLine599Info, 599,
	g_intrSceKernelRegisterIntrHookHandler);
const char g_intrInvalidApiStatCfunc[] __attribute__((aligned(4), used)) =
	"invalidApiStatCfunc";
SCE_DBGINFO_DEFINE(, g_intrSceKernelResumeSubIntrLine1253Info, 1253,
	g_intrSceKernelResumeSubIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelResumeSubIntrLine1244Info, 1244,
	g_intrSceKernelResumeSubIntr);
const char g_intrSceKernelEnableSubIntr[] __attribute__((aligned(4), used)) =
	"sceKernelEnableSubIntr";
SCE_DBGINFO_DEFINE(, g_intrSceKernelSuspendIntrLine705Info, 705,
	g_intrSceKernelSuspendIntr);
const char g_intrSceKernelSuspendSubIntr[] __attribute__((aligned(4), used)) =
	"sceKernelSuspendSubIntr";
const char g_intrSceKernelGenerateSoftIntr[] __attribute__((aligned(4), used)) =
	"sceKernelGenerateSoftIntr";
SCE_DBGINFO_DEFINE(, g_intrSceKernelReleaseSubIntrHandlerLine1086Info, 1086,
	g_intrSceKernelReleaseSubIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrIllegalUserDACRCfuncLine2352Info, 2352,
	g_intrIllegalUserDACRCfunc);
SCE_DBGINFO_DEFINE(, g_intrSceKernelReleaseSubIntrHandlerLine1074Info, 1074,
	g_intrSceKernelReleaseSubIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrIllegalUserDACRCfuncLine2350Info, 2350,
	g_intrIllegalUserDACRCfunc);
const char g_intrSceKernelAllocSystemCallTable[] __attribute__((aligned(4), used)) =
	"sceKernelAllocSystemCallTable";
SCE_DBGINFO_DEFINE(, g_intrSceKernelReleaseSubIntrHandlerLine1079Info, 1079,
	g_intrSceKernelReleaseSubIntrHandler);
const char g_intrSceKernelDisableIntr[] __attribute__((aligned(4), used)) =
	"sceKernelDisableIntr";
const char g_intrSceKernelSetIntrTargetCpu[] __attribute__((aligned(4), used)) =
	"sceKernelSetIntrTargetCpu";
const char g_intrSceKernelGetIntrTargetCpu[] __attribute__((aligned(4), used)) =
	"sceKernelGetIntrTargetCpu";
const char g_intrSceKernelQueryIntrHandlerInfo[] __attribute__((aligned(4), used)) =
	"sceKernelQueryIntrHandlerInfo";
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterIntrHookHandlerLine607Info, 607,
	g_intrSceKernelRegisterIntrHookHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelGetIntrPriorityLine811Info, 811,
	g_intrSceKernelGetIntrPriority);
const char g_intrSceKernelRegisterIntrHookHandler[] __attribute__((aligned(4), used)) =
	"sceKernelRegisterIntrHookHandler";
const char g_intrIllegalUserDACRX[] __attribute__((aligned(4), used)) =
	"\n*** Illegal User DACR = 0x%08X.\n";
const char g_intrFastMutexX[] __attribute__((aligned(4), used)) =
	"*  FastMutex: 0x%08X [%s]\n";
const char g_intrThreadIDX[] __attribute__((aligned(4), used)) =
	"** ThreadID:  0x%08X [%s]\n";
const char g_intrProcessIDX[] __attribute__((aligned(4), used)) =
	"** ProcessID: 0x%08X [%s]\n";
const char g_intrFastMutexHasNotBeenUnlockedAfterSyscall[] __attribute__((aligned(4), used)) =
	"\n*** FastMutex has not been unlocked after syscall.\n";
const char g_intrWarningDetectInvalidApiStatXLrX[] __attribute__((aligned(4), used)) =
	"warning: detect invalid api stat(0x%08X), lr = 0x%08X, syscall_entry 0x%08x\n";
const char g_intrRegisterSyscallEntryFailedX[] __attribute__((aligned(4), used)) =
	"register syscall entry failed. 0x%08x\n";
const char g_intrCreateHeapFailedX[] __attribute__((aligned(4), used)) =
	"Create Heap failed. 0x%08x\n";
const char g_intrSceIntrMgr[] __attribute__((aligned(4), used)) =
	"SceIntrMgr";
const char g_intrRegisterExceptionHandlerFailedX[] __attribute__((aligned(4), used)) =
	"register exception handler failed. 0x%08x\n";
const char g_intrAllocateIntcRegisterAreaFailedX[] __attribute__((aligned(4), used)) =
	"allocate intc register area failed. 0x%08x\n";
const char g_intrSceInterruptControllerReg[] __attribute__((aligned(4), used)) =
	"SceInterruptControllerReg";
const char g_intrCPUIntrNameXNestCount[] __attribute__((aligned(4), used)) =
	"     CPU = %d, intr name %s (0x%08x:%d), nest count = %d\n";
const char g_intrDetectIntrStackOverflow[] __attribute__((aligned(4), used)) =
	"!!! detect intr stack overflow !!!\n";
const char g_intrHighestPendingX[] __attribute__((aligned(4), used)) =
	"             : [Highest Pending] 0x%08x\n";
const char g_intrRunningPriorityX[] __attribute__((aligned(4), used)) =
	"             : [Running Priority] 0x%08x\n";
const char g_intrAcknowledgeCPUSourceInterruptIDX[] __attribute__((aligned(4), used)) =
	"             : [Acknowledge] - <CPU Source> %d, <Interrupt ID> 0x%x\n";
const char g_intrBinaryX[] __attribute__((aligned(4), used)) =
	"             : [Binary] 0x%08x\n";
const char g_intrMaskX[] __attribute__((aligned(4), used)) =
	"             : [Mask] 0x%08x\n";
const char g_intrCPUInterfaceControlX[] __attribute__((aligned(4), used)) =
	"CPU Interface: [Control] 0x%08x\n";
const char g_intrCPUTargetIDX[] __attribute__((aligned(4), used)) =
	"                    : [CPU Target] (ID %d - %d) 0x%08x\n";
const char g_intrPriorityIDX[] __attribute__((aligned(4), used)) =
	"                    : [Priority] (ID %d - %d) 0x%08x\n";
const char g_intrActiveIDX[] __attribute__((aligned(4), used)) =
	"                    : [Active] (ID %d - %d) 0x%08x\n";
const char g_intrEnableIDX[] __attribute__((aligned(4), used)) =
	"                    : [Enable] (ID %d - %d) 0x%08x\n";
const char g_intrControlX[] __attribute__((aligned(4), used)) =
	"                    : [Control] 0x%08x\n";
const char g_intrInterruptDistributorControllerTypeCPUNumITLines[] __attribute__((aligned(4), used)) =
	"Interrupt Distributor : [Controller Type] - <CPU num> %d, <IT lines num> %d\n";
const char g_intrCPSRX[] __attribute__((aligned(4), used)) =
	"CPSR 0x%08x\n";
const char g_intrAllocateSystemCallTableFailedX[] __attribute__((aligned(4), used)) =
	"allocate system call table failed. 0x%08x\n";
const char g_intrSceSyscallTable[] __attribute__((aligned(4), used)) =
	"SceSyscallTable";
const char g_intrStackOverflowedIntrNameSubintrName[] __attribute__((aligned(4), used)) =
	" stack overflowed. intr name [ %s (%d) ], subintr name [ %s (%d) ]\n";
const char g_intrSubInterruptHandlerIsNotRegistered[] __attribute__((aligned(4), used)) =
	"sub interrupt handler is not registered. %d-%d\n";
const char g_intrUnknownSubinterruptCode[] __attribute__((aligned(4), used)) =
	"Unknown subinterrupt code. %d-%d\n";
const char g_intrAlreadyRegistered2[] __attribute__((aligned(4), used)) =
	"already registered: %d-%d %s %p\n";
const char g_intrNotYetRegistered[] __attribute__((aligned(4), used)) =
	"not yet registered. %d\n";
const char g_intrInvalidHandlerAddress2[] __attribute__((aligned(4), used)) =
	"invalid handler address.\n";
const char g_intrUnknownTargetX[] __attribute__((aligned(4), used)) =
	"Unknown target. 0x%x\n";
const char g_intrUnknownFilter[] __attribute__((aligned(4), used)) =
	"Unknown filter. %d\n";
const char g_intrCanTChangeSGIPriority[] __attribute__((aligned(4), used)) =
	"Can't change SGI priority. %d\n";
const char g_intrInvalidArgument[] __attribute__((aligned(4), used)) =
	"Invalid argument\n";
const char g_intrInterruptHandlerIsNotRegistered2[] __attribute__((aligned(4), used)) =
	"interrupt handler is not registered\n";
const char g_intrInterruptHandlerIsNotRegisteredIntrcode[] __attribute__((aligned(4), used)) =
	"interrupt handler is not registered. intrcode %d\n";
const char g_intrIllegalInterruptCode[] __attribute__((aligned(4), used)) =
	"Illegal interrupt code. %d\n";
const char g_intrInvalidOldHandlerAddress[] __attribute__((aligned(4), used)) =
	"invalid old handler address. %p\n";
const char g_intrInvalidNewHandlerAddress[] __attribute__((aligned(4), used)) =
	"invalid new handler address. %p\n";
const char g_intrInterruptHandlerIsNotRegistered[] __attribute__((aligned(4), used)) =
	"interrupt handler is not registered. %d\n";
const char g_intrAlreadyRegistered[] __attribute__((aligned(4), used)) =
	"already registered: %d %s %p\n";
const char g_intrInvalidTargetCpu[] __attribute__((aligned(4), used)) =
	"invalid target cpu. %d\n";
const char g_intrInvalidPriority[] __attribute__((aligned(4), used)) =
	"invalid priority. %d\n";
const char g_intrInvalidHandlerAddress[] __attribute__((aligned(4), used)) =
	"invalid handler address. %p\n";
const char g_intrUnknownType[] __attribute__((aligned(4), used)) =
	"Unknown type. %d\n";
const char g_intrUnknownInterruptCode[] __attribute__((aligned(4), used)) =
	"Unknown interrupt code. %d\n";
SCE_DBGINFO_DEFINE(, g_intrSceKernelAllocSystemCallTableLine1446Info, 1446,
	g_intrSceKernelAllocSystemCallTable);
const char g_intrSceKernelResumeSubIntr[] __attribute__((aligned(4), used)) =
	"sceKernelResumeSubIntr";
const char g_intrSceKernelSuspendIntr[] __attribute__((aligned(4), used)) =
	"sceKernelSuspendIntr";
SCE_DBGINFO_DEFINE(, g_intrSceKernelReleaseIntrHandlerLine543Info, 543,
	g_intrSceKernelReleaseIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterIntrHandlerLine415Info, 415,
	g_intrSceKernelRegisterIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelReleaseIntrHandlerLine533Info, 533,
	g_intrSceKernelReleaseIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelGenerateSoftIntrLine897Info, 897,
	g_intrSceKernelGenerateSoftIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelGenerateSoftIntrLine891Info, 891,
	g_intrSceKernelGenerateSoftIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelSuspendSubIntrLine1197Info, 1197,
	g_intrSceKernelSuspendSubIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelGenerateSoftIntrLine885Info, 885,
	g_intrSceKernelGenerateSoftIntr);
const char g_intrIllegalUserDACRCfunc[] __attribute__((aligned(4), used)) =
	"illegalUserDACRCfunc";
SCE_DBGINFO_DEFINE(, g_intrSceKernelIntrMgrModuleStartLine2251Info, 2251,
	g_intrSceKernelIntrMgrModuleStart);
SCE_DBGINFO_DEFINE(, g_intrIllegalUserDACRCfuncLine2354Info, 2354,
	g_intrIllegalUserDACRCfunc);
SCE_DBGINFO_DEFINE(, g_intrSceKernelClearIntrPendingStatusForDebuggerLine937Info, 937,
	g_intrSceKernelClearIntrPendingStatusForDebugger);
SCE_DBGINFO_DEFINE(, g_intrStartLine2025Info, 2025,
	g_intrStart);
SCE_DBGINFO_DEFINE(, g_intrSceKernelSetIntrTargetCpuLine833Info, 833,
	g_intrSceKernelSetIntrTargetCpu);
SCE_DBGINFO_DEFINE(, g_intrSceKernelGetIntrPendingStatusForDebuggerLine927Info, 927,
	g_intrSceKernelGetIntrPendingStatusForDebugger);
SCE_DBGINFO_DEFINE(, g_intrInvalidFastMutexOwnerCfuncLine2338Info, 2338,
	g_intrInvalidFastMutexOwnerCfunc);
SCE_DBGINFO_DEFINE(, g_intrSceKernelGetIntrPriorityLine807Info, 807,
	g_intrSceKernelGetIntrPriority);
SCE_DBGINFO_DEFINE(, g_intrSceKernelSetIntrPriorityLine776Info, 776,
	g_intrSceKernelSetIntrPriority);
SCE_DBGINFO_DEFINE(, g_intrStartLine2024Info, 2024,
	g_intrStart);
SCE_DBGINFO_DEFINE(, g_intrStartLine2012Info, 2012,
	g_intrStart);
SCE_DBGINFO_DEFINE(, g_intrSceKernelSetIntrPriorityLine789Info, 789,
	g_intrSceKernelSetIntrPriority);
SCE_DBGINFO_DEFINE(, g_intrSceKernelSetIntrPriorityLine785Info, 785,
	g_intrSceKernelSetIntrPriority);
SCE_DBGINFO_DEFINE(, g_intrSceKernelSetIntrPriorityLine780Info, 780,
	g_intrSceKernelSetIntrPriority);
SCE_DBGINFO_DEFINE(, g_intrStartLine1998Info, 1998,
	g_intrStart);
SCE_DBGINFO_DEFINE(, g_intrSceKernelDisableIntrLine691Info, 691,
	g_intrSceKernelDisableIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelDisableIntrLine687Info, 687,
	g_intrSceKernelDisableIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelCallSubIntrHandlerLine1347Info, 1347,
	g_intrSceKernelCallSubIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelCallSubIntrHandlerLine1323Info, 1323,
	g_intrSceKernelCallSubIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelCallSubIntrHandlerLine1315Info, 1315,
	g_intrSceKernelCallSubIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelSuspendSubIntrLine1215Info, 1215,
	g_intrSceKernelSuspendSubIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterIntrHandlerLine452Info, 452,
	g_intrSceKernelRegisterIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterIntrHandlerLine420Info, 420,
	g_intrSceKernelRegisterIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelReleaseSubIntrHandlerLine1065Info, 1065,
	g_intrSceKernelReleaseSubIntrHandler);
const char g_intrSceKernelClearIntrPendingStatusForDebugger[] __attribute__((aligned(4), used)) =
	"sceKernelClearIntrPendingStatusForDebugger";
SCE_DBGINFO_DEFINE(, g_intrSceKernelDisableSubIntrLine1171Info, 1171,
	g_intrSceKernelDisableSubIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelDisableSubIntrLine1162Info, 1162,
	g_intrSceKernelDisableSubIntr);
const char g_intrSceKernelGetIntrPendingStatusForDebugger[] __attribute__((aligned(4), used)) =
	"sceKernelGetIntrPendingStatusForDebugger";
SCE_DBGINFO_DEFINE(, g_intrSceKernelEnableSubIntrLine1127Info, 1127,
	g_intrSceKernelEnableSubIntr);
const char g_intrSceKernelReleaseIntrHandler[] __attribute__((aligned(4), used)) =
	"sceKernelReleaseIntrHandler";
SCE_DBGINFO_DEFINE(, g_intrInvalidFastMutexOwnerCfuncLine2335Info, 2335,
	g_intrInvalidFastMutexOwnerCfunc);
SCE_DBGINFO_DEFINE(, g_intrInvalidFastMutexOwnerCfuncLine2334Info, 2334,
	g_intrInvalidFastMutexOwnerCfunc);
const char g_intrSceKernelGetIntrPriority[] __attribute__((aligned(4), used)) =
	"sceKernelGetIntrPriority";
SCE_DBGINFO_DEFINE(, g_intrInvalidFastMutexOwnerCfuncLine2333Info, 2333,
	g_intrInvalidFastMutexOwnerCfunc);
SCE_DBGINFO_DEFINE(, g_intrIllegalUserDACRCfuncLine2351Info, 2351,
	g_intrIllegalUserDACRCfunc);
SCE_DBGINFO_DEFINE(, g_intrSceKernelIntrMgrModuleStartLine2250Info, 2250,
	g_intrSceKernelIntrMgrModuleStart);
const char g_intrStart[] __attribute__((aligned(4), used)) =
	"Start";
const char g_intrSceKernelSetIntrPriority[] __attribute__((aligned(4), used)) =
	"sceKernelSetIntrPriority";
const char g_intrSceKernelCallSubIntrHandler[] __attribute__((aligned(4), used)) =
	"sceKernelCallSubIntrHandler";
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterSubIntrHandlerLine971Info, 971,
	g_intrSceKernelRegisterSubIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelEnableIntrLine674Info, 674,
	g_intrSceKernelEnableIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelEnableIntrLine670Info, 670,
	g_intrSceKernelEnableIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelResumeIntrLine747Info, 747,
	g_intrSceKernelResumeIntr);
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterIntrHandlerLine390Info, 390,
	g_intrSceKernelRegisterIntrHandler);
const char g_intrmgrFileName[] __attribute__((aligned(4), used)) =
	"intrmgr.c";
SCE_DBGINFO_DEFINE(, g_intrSceKernelReleaseIntrHookHandlerLine643Info, 643,
	g_intrSceKernelReleaseIntrHookHandler);
const char g_intrSceKernelResumeIntr[] __attribute__((aligned(4), used)) =
	"sceKernelResumeIntr";
SCE_DBGINFO_DEFINE(, g_intrSceKernelIsSubInterruptOccurredLine1288Info, 1288,
	g_intrSceKernelIsSubInterruptOccurred);
const char g_intrSceKernelReleaseSubIntrHandler[] __attribute__((aligned(4), used)) =
	"sceKernelReleaseSubIntrHandler";
SCE_DBGINFO_DEFINE(, g_intrSceKernelIsSubInterruptOccurredLine1279Info, 1279,
	g_intrSceKernelIsSubInterruptOccurred);
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterIntrHandlerLine399Info, 399,
	g_intrSceKernelRegisterIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelRegisterIntrHandlerLine395Info, 395,
	g_intrSceKernelRegisterIntrHandler);
SCE_DBGINFO_DEFINE(, g_intrSceKernelIntrMgrModuleStartLine2264Info, 2264,
	g_intrSceKernelIntrMgrModuleStart);
SCE_DBGINFO_DEFINE(, g_intrSceKernelQueryIntrHandlerInfoLine1467Info, 1467,
	g_intrSceKernelQueryIntrHandlerInfo);
#endif

