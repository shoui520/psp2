/*
 * The user side of SceVshBridge's calls that take a sized structure, as the stub library links
 * them into a module: the system call gets the structure's size in an argument block, for the
 * kernel to copy that much in (the library's and the file's names ours).
 */
#include <stddef.h>
#include <scetypes.h>
#include <display/display.h>
#include <intrmgr/syscall_copy.h>

int _vshDisplaySetViewportConf(SceUInt32 head, SceUInt32 plane, const SceDisplayViewportConf *conf,
			       const SyscallSizeArgs *args);

int vshDisplaySetViewportConf(SceUInt32 head, SceUInt32 plane, const SceDisplayViewportConf *conf)
{
	SyscallSizeArgs args;

	args.size = conf != NULL ? conf->size : 0;
	return _vshDisplaySetViewportConf(head, plane, conf, &args);
}
