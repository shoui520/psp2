/* Dispatch for application and user-directory path queries. */

#include <scetypes.h>
#include <appmgr/appmgr.h>
#include "mount.h"
#include "app_state_internal.h"

int appmgrConvertVs0UserDrivePathSyscall(const char *path, char *output,
	SceSize size)
{
	return appmgrConvertVs0UserDrivePath(path, output, size);
}

int appmgrGetRawPathSyscall(char *path, char *resolvedPath, int capacity)
{
	return sceAppMgrGetRawPath(path, resolvedPath, capacity);
}

int appmgrGetUserDirPathSyscall(int partitionId, char *path,
	unsigned int capacity)
{
	return appmgrGetUserDirPathById(-1, partitionId, path, capacity);
}

int appmgrGetUserDirPathByIdSyscall(int userId, int partitionId, char *path,
	unsigned int capacity)
{
	return appmgrGetUserDirPathById(userId, partitionId, path, capacity);
}

int appmgrQuitAppSyscall(int appId)
{
	return sceAppMgrQuitApp(appId);
}

int appmgrDumpSyscall(void)
{
	return sceAppMgrDump();
}

int appmgrGetRawPathOfApp0ByAppIdForShellSyscall(int appId, char *path)
{
	return sceAppMgrGetRawPathOfApp0ByAppIdForShell(appId, path);
}

int appmgrGetRawPathOfApp0ByPidForShellSyscall(SceUID pid, char *path)
{
	return sceAppMgrGetRawPathOfApp0ByPidForShell(pid, path);
}
