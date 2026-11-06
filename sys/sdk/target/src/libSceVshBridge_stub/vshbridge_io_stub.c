/*
 * The user side of SceVshBridge's mount and PS code calls, as the stub library links them into
 * a program: vshIoMount gives the system call sceIoMount's arguments and the path's size in an
 * argument block (the file's name ours; the order is cui_setupper's).
 */
#include <stddef.h>
#include <scetypes.h>
#include <vshbridge/vshbridge.h>

SceUInt32 vshIoMount(int id, const char *path, int permission, void *data,
		     SceUInt32 reservedLow, SceUInt32 reservedHigh)
{
	VshIoMountArgs args;
	SceSize length;

	args.data = data;
	args.reservedLow = reservedLow;
	args.reservedHigh = reservedHigh;
	length = 0;
	if (path != NULL) {
		while (path[length] != '\0')
			length++;
		length++;
	}
	args.pathSize = length;
	return _vshIoMount(id, path, permission, &args);
}

SceUInt32 vshSblAimgrGetPscode(void *code)
{
	return _vshSblAimgrGetPscode(code);
}
