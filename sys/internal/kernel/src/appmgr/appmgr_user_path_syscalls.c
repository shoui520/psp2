/* User-memory arguments and outputs for application path queries. */

#include <intrmgr/syscall_copy.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include <appmgr/appmgr.h>

typedef struct AppmgrRawPathArguments {
	unsigned char reserved[16];
} AppmgrRawPathArguments;

_Static_assert(sizeof(AppmgrRawPathArguments) == 16,
	"raw path argument block");

typedef struct AppmgrUserDirPathArguments {
	SceSize capacity;
	unsigned char reserved[12];
} AppmgrUserDirPathArguments;

_Static_assert(sizeof(AppmgrUserDirPathArguments) == 16,
	"user directory argument block");

extern int appmgrConvertVs0UserDrivePathSyscall(const char *path, char *output,
	SceSize size);
extern int appmgrGetRawPathSyscall(char *path, char *resolvedPath, int capacity);
extern int appmgrGetUserDirPathSyscall(int partitionId, char *path,
	unsigned int capacity);
extern int appmgrGetUserDirPathByIdSyscall(int userId, int partitionId,
	char *path, unsigned int capacity);
extern int appmgrGetRawPathOfApp0ByAppIdForShellSyscall(int appId, char *path);
extern int appmgrGetRawPathOfApp0ByPidForShellSyscall(SceUID pid, char *path);

int _sceAppMgrConvertVs0UserDrivePath(const char *path, char *convertedPath,
	SceSize capacity, const AppmgrRawPathArguments *userArguments)
{
	AppmgrRawPathArguments arguments;
	char *name = NULL;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, path, 292);
	if (convertedPath == NULL)
		return appmgrConvertVs0UserDrivePathSyscall(name, NULL, capacity);
	buffer = __builtin_alloca(292);
	__builtin_memset(buffer, 0, 292);
	ret = appmgrConvertVs0UserDrivePathSyscall(name, buffer, capacity);
	length = strnlen(buffer, 292) + 1;
	if (length >= 292)
		length = 292;
	return copyResult(ret, sceKernelCopyToUser(convertedPath, buffer, length));
}

int _sceAppMgrGetRawPath(char *path, char *resolvedPath, int capacity,
	const AppmgrRawPathArguments *userArguments)
{
	AppmgrRawPathArguments arguments;
	char *name = path;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, path, 292);
	if (resolvedPath == NULL)
		return appmgrGetRawPathSyscall(name, NULL, capacity);
	buffer = __builtin_alloca(292);
	__builtin_memset(buffer, 0, 292);
	ret = appmgrGetRawPathSyscall(name, buffer, capacity);
	length = strnlen(buffer, 292) + 1;
	if (length >= 292)
		length = 292;
	return copyResult(ret, sceKernelCopyToUser(resolvedPath, buffer, length));
}

int _sceAppMgrGetUserDirPath(int partitionId, char *path, unsigned int capacity,
	const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (path == NULL)
		return appmgrGetUserDirPathSyscall(partitionId, NULL, capacity);
	buffer = __builtin_alloca(32);
	__builtin_memset(buffer, 0, 32);
	ret = appmgrGetUserDirPathSyscall(partitionId, buffer, capacity);
	length = strnlen(buffer, 32) + 1;
	if (length >= 32)
		length = 32;
	return copyResult(ret, sceKernelCopyToUser(path, buffer, length));
}

int _sceAppMgrGetUserDirPathById(int userId, int partitionId, char *path,
	const AppmgrUserDirPathArguments *userArguments)
{
	AppmgrUserDirPathArguments arguments;
	unsigned int capacity;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	capacity = arguments.capacity;
	if (path == NULL)
		return appmgrGetUserDirPathByIdSyscall(userId, partitionId, NULL,
			capacity);
	buffer = __builtin_alloca(32);
	__builtin_memset(buffer, 0, 32);
	ret = appmgrGetUserDirPathByIdSyscall(userId, partitionId, buffer,
		capacity);
	length = strnlen(buffer, 32) + 1;
	if (length >= 32)
		length = 32;
	return copyResult(ret, sceKernelCopyToUser(path, buffer, length));
}

int _sceAppMgrGetRawPathOfApp0ByAppIdForShell(int appId, char *path)
{
	char *buffer;
	int result;

	if (path == NULL)
		return appmgrGetRawPathOfApp0ByAppIdForShellSyscall(appId, NULL);
	buffer = __builtin_alloca(292);
	__builtin_memset(buffer, 0, 292);
	result = appmgrGetRawPathOfApp0ByAppIdForShellSyscall(appId, buffer);
	return copyResult(result, sceKernelCopyToUser(path, buffer, 292));
}

int _sceAppMgrGetRawPathOfApp0ByPidForShell(SceUID pid, char *path)
{
	char *buffer;
	int result;

	if (path == NULL)
		return appmgrGetRawPathOfApp0ByPidForShellSyscall(pid, NULL);
	buffer = __builtin_alloca(292);
	__builtin_memset(buffer, 0, 292);
	result = appmgrGetRawPathOfApp0ByPidForShellSyscall(pid, buffer);
	return copyResult(result, sceKernelCopyToUser(path, buffer, 292));
}
