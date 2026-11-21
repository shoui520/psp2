/*
 * sceAppUtilLaunchWebBrowser: the web browser opened at a page or a search through its URI
 * ("wbapp0:openurl?len=<n>&str=<page>"), and what the web browser calls share: the URI built piece
 * by piece, and the files kept in the application's work directory.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
#include <appmgr.h>
#include <apputil.h>
#include <appmgr/appmgr.h>
#include "../apputil_private.h"

/* the longest page or search sceAppUtilLaunchWebBrowser takes */
#define WEBBROWSER_STR_MAX		960

int _sceAppUtilLaunchWebBrowser(const SceAppUtilWebBrowserParam *param)
{
	char *cursor;
	SceSize length;
	char part[WEBBROWSER_URI_PART_SIZE];
	char uri[WEBBROWSER_URI_SIZE];
	const char *prefix;
	SceSize n;
	int launchMode, ret;

	cursor = uri;
	length = 0;
	sceClibMemsetChk(uri, 0, sizeof(uri), sizeof(uri));
	sceClibMemsetChk(part, 0, sizeof(part), sizeof(part));
	if (param == NULL) {
		paramError(150);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param->wbstr == NULL) {
		paramError(151);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	ret = sceClibStrnlen((const char *)param->wbstr, WEBBROWSER_STR_MAX);
	if (ret > WEBBROWSER_STR_MAX) {
		paramError(152);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if ((SceUInt32)ret != param->wbstrLength) {
		paramError(152);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	ret = getFreeStackSize();
	if (ret < APPUTIL_LAUNCH_STACK_SIZE)
		return SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
	switch (param->launchMode & 3) {
	case 0:
		prefix = "wbapp0:";
		launchMode = 0x20000;
		break;
	case 1:
		prefix = "wbapp1:";
		launchMode = 0x60000;
		break;
	default:
		paramError(153);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibStrncpy(cursor, prefix, 7);
	webBrowserUriAdvance(&cursor, &length, 7);
	switch (param->launchMode & 0x3c) {
	case 0:
		prefix = "openurl?";
		n = 8;
		break;
	case 4:
		prefix = "search?";
		n = 7;
		break;
	default:
		paramError(153);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibStrncpy(cursor, prefix, n);
	webBrowserUriAdvance(&cursor, &length, n);
	ret = sceClibSnprintf(part, sizeof(part), "%s%d&%s", "len=", param->wbstrLength, "str=");
	if (ret < 0) {
		paramError(154);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibStrncpy(cursor, part, ret);
	webBrowserUriAdvance(&cursor, &length, ret);
	sceClibStrncpy(cursor, (const char *)param->wbstr, param->wbstrLength);
	return sceAppMgrLaunchAppByUri(launchMode, uri);
}

SceSize webBrowserUriAppend(char **cursor, const char *str, SceSize maxLength)
{
	SceSize n;

	n = sceClibStrnlen(str, maxLength);
	sceClibStrncpy(*cursor, str, n);
	return n;
}

void webBrowserUriAdvance(char **cursor, SceSize *length, SceSize n)
{
	*cursor += n;
	*length += n;
}

WebBrowserFile::WebBrowserFile()
{
}

/* size bytes of path read from its start */
static inline int readFileHeader(const char *path, void *buf, SceSize size)
{
	SceUID fd;
	int ret;

	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	ret = fd;
	if (fd >= 0) {
		sceIoLseek(fd, 0, SCE_SEEK_SET);
		ret = sceIoRead(fd, buf, size);
		sceIoClose(fd);
	}
	return ret;
}

/* size bytes written at offset of path, the file created when it is not there */
static inline int writeFile(const char *path, SceUInt32 offset, const void *buf, SceSize size)
{
	SceIoStat stat;
	SceUID fd;
	int ret;

	if (sceIoGetstat(path, &stat) != 0)
		fd = sceIoOpen(path, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, SCE_STM_RWU);
	else
		fd = sceIoOpen(path, SCE_O_RDWR, SCE_STM_RWU);
	ret = fd;
	if (fd >= 0) {
		ret = 0;
		sceIoLseek(fd, offset, SCE_SEEK_SET);
		sceIoWrite(fd, buf, size);
		sceIoClose(fd);
	}
	return ret;
}

int WebBrowserFile::Append(const char *path, WebBrowserFileHeader *header, const WebBrowserFileRecord *record,
			   SceUInt32 maxCount)
{
	SceUInt32 offset, count;
	int ret;

	ret = readFileHeader(path, header, sizeof(*header));
	if (ret < 0 && ret != SCE_ERROR_ERRNO_ENOENT)
		return ret;
	header->headerSize = sizeof(*header);
	count = header->count + 1;
	header->reserved = 0;
	offset = header->recordsSize;
	if (maxCount != 0 && count > maxCount)
		return -2;
	header->count = count;
	header->recordsSize = offset + sizeof(*record);
	ret = writeFile(path, 0, header, sizeof(*header));
	if (ret >= 0)
		ret = writeFile(path, offset + sizeof(*header), record, sizeof(*record));
	/* the stack measured, and nothing done with it */
	getFreeStackSize();
	return ret;
}

int webBrowserFileMount(int type, char *mountPoint)
{
	if (type != WEBBROWSER_FILE_MOUNT_WORKDIR)
		return -1;
	return sceAppMgrWorkDirMount(APPUTIL_MOUNT_ID_WEBBROWSER, (ScePfsRndDriveId *)mountPoint);
}

int webBrowserFileUmount(const char *mountPoint)
{
	return sceAppMgrUmount(mountPoint);
}

int webBrowserFileRemove(const char *name)
{
	char mountPoint[16];
	char path[256];
	int ret;

	sceClibMemset(path, 0, sizeof(path));
	sceClibMemset(mountPoint, 0, sizeof(mountPoint));
	if (webBrowserFileMount(WEBBROWSER_FILE_MOUNT_WORKDIR, mountPoint) < 0)
		return SCE_APPUTIL_ERROR_NOT_MOUNTED;
	sceClibStrncpy(path, mountPoint, sizeof(path));
	sceClibStrncat(path, name, sceClibStrnlen(name, sizeof(path)));
	ret = sceIoRemove(path);
	if (ret == SCE_ERROR_ERRNO_ENOENT)
		ret = 0;
	webBrowserFileUmount(mountPoint);
	return ret;
}
