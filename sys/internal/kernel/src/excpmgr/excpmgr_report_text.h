/* GCC reverses readonly declarations; this list yields Sony's text order. */
#pragma once

#define EXCP_REPORT_TEXTS(X) \
	X(dabtHeader, "\n*** DABT Exception at CPU%d\n") \
	X(pabtHeader, "\n*** PABT Exception at CPU%d\n") \
	X(threadNameFormat, "ThreadName:  %p[%s] (tid=0x%08X)\n") \
	X(threadNameErrorFormat, "ThreadName:  error 0x%08X (tid=0x%08X)\n") \
	X(processNameFormat, "ProcessName: %p[%s] (pid=0x%08X)\n") \
	X(processNameErrorFormat, "ProcessName: error 0x%08X (pid=0x%08X)\n") \
	X(undefHeader, "\n*** UNDEF Exception at CPU%d\n") \
	X(vfpBlockName, "SceVfpIntRegs") \
	X(nested, "NEST") \
	X(dabt, "DABT") \
	X(pabt, "PABT") \
	X(undef, "UNDEF") \
	X(defaultSpsrFormat, "spsr     0x%08x\n") \
	X(defaultCpsrFormat, "cpsr     0x%08x\n") \
	X(emptyFormat, "()\n") \
	X(handlerFormat, "(excode %d, handler %p)\n") \
	X(interruptContext, "  Context:Interrupt/Exception Handler\n") \
	X(threadIdNameFormat, "ThreadId:  0x%08X [%s]\n") \
	X(threadIdFormat, "ThreadId:  0x%08X\n") \
	X(processIdNameFormat, "ProcessId: 0x%08X [%s]\n") \
	X(processIdFormat, "ProcessId: 0x%08X\n") \
	X(waypointNoModuleFormat, "(%2d) PC:0x%08X, TARGETPC:0x%08X %s%c%c [no module info]\n") \
	X(waypointModuleFormat, "(%2d) PC:0x%08X, TARGETPC:0x%08X %s%c%c [%s + 0x%08X]\n") \
	X(uidFormat, "0x%08X") \
	X(waypointEventFormat, "(%2d) PC:0x%08X, TARGETPC:0x%08X <<< %s >>>\n") \
	X(waypointExceptionFormat, "(%2d) PC:0x%08X, TARGETPC:0x%08X <<< %s Exception >>>\n") \
	X(waypointHeader, "=== Print Waypoint ===\n") \
	X(thumb32, "T32") \
	X(thumbEE32, "E32") \
	X(thumbEE16, "E16") \
	X(thumb16, "T16") \
	X(jazelle, "Jzl") \
	X(arm, "ARM") \
	X(fpscrFormat, "FPSCR: 0x%08X\n") \
	X(fpexcFormat, "FPEXC: 0x%08X\n") \
	X(vfpFormat, "d%-2d,d%-2d(q%-2d) : 0x%08X%08X 0x%08X%08X\n") \
	X(tpidrprwFormat, "TPIDRPRW   : 0x%08X\n") \
	X(tpidruroFormat, "TPIDRURO   : 0x%08X\n") \
	X(tpidrurwFormat, "TPIDRURW   : 0x%08X\n") \
	X(ttbr1Format, "TTBR1      : 0x%08X\n") \
	X(dacrFormat, "DACR       : 0x%08X\n") \
	X(contextIdFormat, "Context ID : 0x%08X\n") \
	X(cpsrFormat, "CPSR  : 0x%08X [%s]\n") \
	X(pcFormat, "PC    : 0x%08X\n") \
	X(lrFormat, "LR    : 0x%08X\n") \
	X(spFormat, "SP    : 0x%08X\n") \
	X(r12Format, "r12   : 0x%08X\n") \
	X(r8Format, "r8-r11: 0x%08X 0x%08X 0x%08X 0x%08X\n") \
	X(r4Format, "r4-r7 : 0x%08X 0x%08X 0x%08X 0x%08X\n") \
	X(r0Format, "r0-r3 : 0x%08X 0x%08X 0x%08X 0x%08X\n") \
	X(invalidContextFormat, "  illegal base addr\n") \
	X(dfarFormat, "DFAR           0x%08X [ %s ]\n") \
	X(invalidDomainFormat, "DFSR DOMAIN    [ invalid ]\n") \
	X(domainFormat, "DFSR DOMAIN    0x%X [ %s ]\n") \
	X(dfsrFormat, "DFSR           0x%08X [ %s ]\n") \
	X(invalid, "invalid") \
	X(valid, "valid") \
	X(separator, "\n") \
	X(ifarFormat, "IFAR           0x%08X\n") \
	X(debugStatusFormat, "DBGDSCR        0x%08X [ %s ]\n") \
	X(ifsrFormat, "IFSR           0x%08X [ %s ]\n")

#define EXCP_REPORT_TEXT_DECLARE(_name, _text) \
	extern const char g_excpmgrText##_name[sizeof(_text)];
EXCP_REPORT_TEXTS(EXCP_REPORT_TEXT_DECLARE)
#undef EXCP_REPORT_TEXT_DECLARE
