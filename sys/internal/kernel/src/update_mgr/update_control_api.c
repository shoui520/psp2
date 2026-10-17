#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include "update_internal.h"
#include <acmgr/acmgr.h>

int sceSblUsGetUpdateModeForUser(SceUpdateMode *output)
{
	SceUpdateMode mode = 0;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	result = SpkgUtilGetUpdateMode(&mode);
	if (result == 0)
		result = SceSysmemForDriver_6D88EF8A(output, &mode, sizeof(mode));
	EXIT_SYSCALL(state);
	return result;
}

int sceSblUsSetUpdateModeForUser(SceUpdateMode mode)
{
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	result = SpkgUtilSetUpdateMode(mode);
	EXIT_SYSCALL(state);
	return result;
}

int sceSblUsCheckSystemIntegrityForUser(void)
{
	unsigned int state;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	EXIT_SYSCALL(state);
	return 0;
}

int sceSblUsPowerControlForUser(int operation, int flags)
{
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	result = SpkgUtilPowerControl(operation, flags);
	EXIT_SYSCALL(state);
	return result;
}

int sceSblUsGetApplicableVersionForUser(int type, UpdateApplicableVersion *output)
{
	UpdateApplicableVersion info;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	result = SpkgInfoUtilGetApplicableVersion(type, &info);
	if (result == 0)
		result = SceSysmemForDriver_6D88EF8A(output, &info, sizeof(info));
	EXIT_SYSCALL(state);
	return result;
}
