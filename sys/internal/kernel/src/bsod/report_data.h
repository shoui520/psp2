/* Native BSOD diagnostic strings/status tables. */
static const char s_bsodTextCause[] = "*** %s @ CPU%d ***\n";
static const char s_bsodTextUnknownCause[] = "*** Unknown cause 0x%08X @ CPU%d ***\n";
static const char s_bsodTextSessionId[] = "SessionId: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n";
static const char s_bsodTextProcessId[] = "ProcessId: 0x%08X\n";
static const char s_bsodTextProcessIdName[] = "ProcessId: 0x%08X [%s]\n";
static const char s_bsodTextThreadId[] = "ThreadId:  0x%08X\n";
static const char s_bsodTextThreadIdName[] = "ThreadId:  0x%08X [%s]\n";
static const char s_bsodTextHandlerContext[] = "Context:   Interrupt/Exception Handler\n";
static const char s_bsodTextR0R3[] = "r0-r3 : 0x%08X 0x%08X 0x%08X 0x%08X\n";
static const char s_bsodTextR4R7[] = "r4-r7 : 0x%08X 0x%08X 0x%08X 0x%08X\n";
static const char s_bsodTextR8R11[] = "r8-r11: 0x%08X 0x%08X 0x%08X 0x%08X\n";
static const char s_bsodTextR12[] = "r12   : 0x%08X\n";
static const char s_bsodTextSp[] = "SP    : 0x%08X\n";
static const char s_bsodTextLr[] = "LR    : 0x%08X\n";
static const char s_bsodTextPc[] = "PC    : 0x%08X\n";
static const char s_bsodTextCpsr[] = "CPSR  : 0x%08X [%s]\n";
static const char s_bsodTextVfpRegisters[] = "d%-2d-d%-2d : 0x%08X%08X 0x%08X%08X 0x%08X%08X 0x%08X%08X\n";
static const char s_bsodTextFpexc[] = "FPEXC: 0x%08X\n";
static const char s_bsodTextFpscr[] = "FPSCR: 0x%08X\n";
static const char s_bsodTextNewline[] = "\n";
static const char s_bsodTextContextId[] = "Context ID : 0x%08X\n";
static const char s_bsodTextDacr[] = "DACR       : 0x%08X\n";
static const char s_bsodTextTtbr1[] = "TTBR1      : 0x%08X\n";
static const char s_bsodTextTpidrurw[] = "TPIDRURW   : 0x%08X\n";
static const char s_bsodTextTpidruro[] = "TPIDRURO   : 0x%08X\n";
static const char s_bsodTextTpidrprw[] = "TPIDRPRW   : 0x%08X\n";
static const char s_bsodTextIfsr[] = "IFSR           0x%08X [ %s ]\n";
static const char s_bsodTextDbgdscr[] = "DBGDSCR        0x%08X [ %s ]\n";
static const char s_bsodTextAifsr[] = "AIFSR          0x%08X\n";
static const char s_bsodTextIfar[] = "IFAR           0x%08X\n";
static const char s_bsodTextDfsr[] = "DFSR           0x%08X [ %s ]\n";
static const char s_bsodTextDfsrDomain[] = "DFSR DOMAIN    0x%X [ %s ]\n";
static const char s_bsodTextDfsrDomainInvalid[] = "DFSR DOMAIN    [ invalid ]\n";
static const char s_bsodTextAdfsr[] = "ADFSR          0x%08X\n";
static const char s_bsodTextInvalid[] = "invalid";
static const char s_bsodTextValid[] = "valid";
static const char s_bsodTextDfar[] = "DFAR           0x%08X [ %s ]\n";
static const char s_bsodTextKernelBacktrace[] = "*** Kernel stack backtrace ***\n";
static const char s_bsodTextUserBacktraceEmpty[] = "*** User stack backtrace(empty) ***\n";
static const char s_bsodTextUserBacktrace[] = "*** User stack backtrace ***\n";
static const char s_bsodTextHex[] = "0x%08X";
static const char s_bsodTextBacktraceFrame[] = "(%2d) PC:0x%08X, SP:0x%08X(size=0x%08X byte) [%s + 0x%08X]\n";
static const char s_bsodTextBacktraceFrameNoModule[] = "(%2d) PC:0x%08X, SP:0x%08X(size=0x%08X byte) [no module info]\n";
static const char s_bsodTextMenuSpacing[] = "\n\n\n\n\n\n\n";
static const char s_bsodTextMenuCross[] = "\t\t[CROSS]    : Cancel\n";
static const char s_bsodTextMenuSelect[] = "\t\t[SELECT]   : Cancel\n";
static const char s_bsodTextMenuStart[] = "\t\t[START]    : Reboot system and generate kdump\n";
static const char s_bsodTextMenuNewline[] = "\n";
static const char s_bsodTextMenuStandby[] = "\t\t[STANDBY]  : Power off\n";
static const char *const s_bsodReasons[] = {"Reset exception", "Undefined instruction exception", "Supervisor call", "Prefetch abort exception", "Data abort exception", "Reserved exception", "IRQ exception", "FIQ exception"};
#include <excpmgr/arm_psr_text.h>
static const char *const s_bsodIfsr[] = {"0x00", "0x01", "Debug event", "Section Access Flag fault", "0x04", "Section Translation fault", "Page Access Flag fault", "Page Translation fault", "Synchronous external abort", "Section Domain fault", "0x0A", "Page Domain fault", "1st level Translation table walk synchronous external abort", "Section Permission fault", "2nd level Translation table walk synchronous external abort", "Page Permission fault", "0x10", "0x11", "0x12", "0x13", "Lockdown", "0x15", "0x16", "0x17", "0x18", "Memory access synchronous parity error", "Coprocessor abort", "0x1B", "1st level Translation table walk synchronous parity error", "0x1D", "2nd level Translation table walk synchronous parity error", "0x1F"};
static const char *const s_bsodDebugStatus[] = {"Halt Request debug event", "Breakpoint debug event", "Asynchronous Watchpoint debug event", "BKPT Instruction debug event", "External Debug Request debug event", "Vector Catch debug event", "0x6", "0x7", "OS Unlock Catch debug event", "0x9", "Synchronous Watchpoint debug event", "0xB", "0xC", "0xD", "0xE", "0xF"};
static const char *const s_bsodDomains[] = {"No access", "Client", "Reserved", "Manager"};
static const struct { const char *name; uint16_t addressValid, domainValid; } s_bsodDfsr[32] = {
	{ "0x00", 0, 0 },
	{ "Alignment fault", 1, 0 },
	{ "Debug event", 0, 0 },
	{ "Section Access Flag fault", 1, 0 },
	{ "Instruction cache maintenance fault", 1, 0 },
	{ "Section Translation fault", 1, 0 },
	{ "Page Access Flag fault", 1, 1 },
	{ "Page Translation fault", 1, 1 },
	{ "Synchronous external abort", 1, 0 },
	{ "Section Domain fault", 1, 1 },
	{ "0x0A", 0, 0 },
	{ "Page Domain fault", 1, 1 },
	{ "1st level Translation table walk synchronous external abort", 1, 0 },
	{ "Section Permission fault", 1, 1 },
	{ "2nd level Translation table walk synchronous external abort", 1, 1 },
	{ "Page Permission fault", 1, 1 },
	{ "0x10", 0, 0 },
	{ "0x11", 0, 0 },
	{ "0x12", 0, 0 },
	{ "0x13", 0, 0 },
	{ "Lockdown", 0, 0 },
	{ "0x15", 0, 0 },
	{ "Asynchronous external abort", 0, 0 },
	{ "0x17", 0, 0 },
	{ "0x18", 0, 0 },
	{ "Memory access synchronous parity error", 1, 0 },
	{ "Coprocessor abort", 0, 0 },
	{ "0x1B", 0, 0 },
	{ "1st level Translation table walk synchronous parity error", 1, 0 },
	{ "0x1D", 0, 0 },
	{ "2nd level Translation table walk synchronous parity error", 1, 1 },
	{ "0x1F", 0, 0 },
};
