#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include "update_internal.h"
#include <acmgr/acmgr.h>

int sceSblUsVerifyPupSegmentByIdForUser(const char *path, uint64_t seg_id)
{
	char buffer[1024];
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	result = SceSysmemForDriver_DB3EC244(buffer, path, sizeof(buffer));
	if (result < 0 || result == 1024)
		result = (int)0x800f0216;
	else
		result = sceSblUsVerifyPupSegmentById(buffer, seg_id);
	EXIT_SYSCALL(state);
	return result;
}
