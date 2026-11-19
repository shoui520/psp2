/*
 * SceMailApiForLocalLibc: stores a local message (strings, the time, attachments) as a TLV
 * file in the work directory's "received/" folder, and tells the resident mail process
 * (SceMailResidentPipe) to pick it up.
 */
/* <string> first: scetypes.h defines a _Static_assert macro the SDK's <xstddef> cannot take */
#include <string>
#include <scetypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <kernel.h>
#include <rtc.h>
#include "mail_api_for_local_libc.h"

#define MAIL_LOCAL_BUFFER_SIZE		0x8000
#define MAIL_LOCAL_MAX_MESSAGES		30
#define MAIL_LOCAL_ERROR_SHORT_WRITE	((SceInt32)0x80010016)

/* the TLV file's tags */
#define MAIL_LOCAL_TAG_TITLE		0
#define MAIL_LOCAL_TAG_TEXT		1
#define MAIL_LOCAL_TAG_TIME		2
#define MAIL_LOCAL_TAG_ADDRESS		3
#define MAIL_LOCAL_TAG_BODY		4
#define MAIL_LOCAL_TAG_FLAGS		6

extern "C" {
int sceAppMgrWorkDirMount(SceUInt32 id, char *pMountPoint);
int sceAppMgrUmount(const char *pMountPoint);
}

/* Sony's code calls assign(const char *) once, so its out-of-line copy is part of the module */
template std::string &std::string::assign(const char *);

struct MailLocalSlot {
	SceUInt32 tag;
	const MailLocalAttachment *pAttachment;
	SceSize limit;
};

/* the attachments' tags and their size limits */
static MailLocalSlot s_slotTemplate[4] = {
	{ 4, NULL, 0x200 },
	{ 5, NULL, 0x4B000 },
	{ 7, NULL, 0x2000 },
	{ 8, NULL, 0x6000 },
};

static bool s_mountInitialized;
static char s_mountPoint[16];

/* a directory walk's observer */
class DirectoryObserver {
public:
	SceInt32 error;

	DirectoryObserver() : error(0) {}
	~DirectoryObserver() {}
	virtual bool Begin() = 0;
	virtual void End() = 0;
	virtual void OnFile(std::string &path) = 0;
	virtual void OnDirectory(std::string &path) = 0;
};

class MessageCounter : public DirectoryObserver {
public:
	SceInt32 count;

	MessageCounter() : count(0) {}
	virtual ~MessageCounter() {}
	virtual bool Begin() { return true; }
	virtual void End() {}
	virtual void OnFile(std::string &path) { ++count; }
	virtual void OnDirectory(std::string &path) {}
};

static void WalkDirectory(SceIoDirent *pDirent, std::string &path, DirectoryObserver *pObserver)
{
	if (!pObserver->Begin())
		return;

	SceUID fd = sceIoDopen(path.c_str());
	while (sceIoDread(fd, pDirent) > 0) {
		if (strcmp(pDirent->d_name, ".") == 0 || strcmp(pDirent->d_name, "..") == 0)
			continue;

		size_t length = path.size();
		if (pDirent->d_stat.st_mode & SCE_STM_FDIR) {
			path = path + "/" + pDirent->d_name;
			WalkDirectory(pDirent, path, pObserver);
			pObserver->OnDirectory(path);
			path = path.substr(0, length);
		} else {
			path = path + "/" + pDirent->d_name;
			pObserver->OnFile(path);
			path = path.substr(0, length);
		}
	}

	SceInt32 ret = sceIoDclose(fd);
	if (ret < 0)
		pObserver->error = ret;
	pObserver->End();
}

static inline __attribute__((always_inline)) SceInt32 FlushBuffer(SceUID fd, char *pBuffer, char **ppCursor)
{
	SceSize size = *ppCursor - pBuffer;
	SceSSize ret = sceIoWrite(fd, pBuffer, size);
	if (ret == (SceSSize)size) {
		*ppCursor = pBuffer;
		return 0;
	}
	if (ret > 0)
		ret = MAIL_LOCAL_ERROR_SHORT_WRITE;
	return ret;
}

/* appends a TLV record, flushing the buffer first when it does not fit */
static inline __attribute__((always_inline)) SceInt32 PutRecord(SceUID fd, char *pBuffer, char **ppCursor, char *pEnd, SceUInt32 tag,
			  const void *pData, SceSize size)
{
	SceUInt32 header[2];
	SceSSize ret;

	header[0] = tag;
	header[1] = size;
	if (pEnd - *ppCursor < (SceSSize)(size + 8)) {
		ret = FlushBuffer(fd, pBuffer, ppCursor);
		if (ret != 0)
			return ret;
		if (size + 8 > MAIL_LOCAL_BUFFER_SIZE) {
			ret = sceIoWrite(fd, header, 8);
			if (ret != 8) {
				if (ret > 0)
					ret = MAIL_LOCAL_ERROR_SHORT_WRITE;
				return ret;
			}
			ret = sceIoWrite(fd, pData, size);
			if (ret != (SceSSize)size) {
				if (ret > 0)
					ret = MAIL_LOCAL_ERROR_SHORT_WRITE;
				return ret;
			}
			return 0;
		}
	}
	SceUInt32 *p = (SceUInt32 *)*ppCursor;
	p[0] = header[0];
	p[1] = header[1];
	*ppCursor += 8;
	if (pData != NULL && size != 0) {
		memcpy_s(*ppCursor, pEnd - *ppCursor, pData, size);
		*ppCursor += size;
	}
	return 0;
}

/* appends a file as a record: its type, its size, then its contents */
static inline __attribute__((always_inline)) SceInt32 PutFile(SceUID fd, char *pBuffer, char **ppCursor, SceUInt32 tag, const char *pPath)
{
	SceIoStat stat;
	SceUInt32 header[2];
	SceSSize ret;

	if (sceIoGetstat(pPath, &stat) != 0)
		return MAIL_LOCAL_ERROR_SHORT_WRITE;
	header[0] = tag;
	header[1] = stat.st_size;
	ret = sceIoWrite(fd, pBuffer, *ppCursor - pBuffer);
	if (ret == *ppCursor - pBuffer) {
		*ppCursor = pBuffer;
	} else {
		if (ret > 0)
			return MAIL_LOCAL_ERROR_SHORT_WRITE;
		if (ret != 0)
			return ret;
	}

	SceUID src = sceIoOpen(pPath, 0x10001, 0400);
	if (src < 0)
		return 0;
	ret = sceIoWrite(fd, header, 8);
	if (ret == 8) {
		SceSSize n;
		do {
			n = sceIoRead(src, pBuffer, MAIL_LOCAL_BUFFER_SIZE);
			if (n == 0) {
				sceIoClose(src);
				return 0;
			}
			ret = sceIoWrite(fd, pBuffer, n);
		} while (ret == n);
		if (ret > 0)
			ret = MAIL_LOCAL_ERROR_SHORT_WRITE;
	} else if (ret > 0) {
		ret = MAIL_LOCAL_ERROR_SHORT_WRITE;
	}
	return ret;
}

static void *AllocateZeroed(SceSize size)
{
	void *p = malloc(size);
	if (p != NULL)
		memset(p, 0, size);
	return p;
}

/* closes the file the message is written to when its function is left */
struct FileCloser {
	SceUID fd;

	~FileCloser()
	{
		if (fd != -1)
			sceIoClose(fd);
	}
};

/* frees the write buffer when the function is left */
struct BufferHolder {
	char *pBuffer;

	~BufferHolder()
	{
		free(pBuffer);
	}
};

static __attribute__((noinline)) SceInt32 MailLocalWrite(const MailLocalMessage *pMessage, const char *pMountPoint)
{
	if (pMessage == NULL)
		return SCE_MAIL_API_FOR_LOCAL_ERROR_INVALID_ARGUMENT;

	BufferHolder holder;
	holder.pBuffer = (char *)AllocateZeroed(MAIL_LOCAL_BUFFER_SIZE);
	char *pBuffer = holder.pBuffer;
	if (pBuffer == NULL)
		return SCE_MAIL_API_FOR_LOCAL_ERROR_NO_MEMORY;
	char *pEnd = pBuffer + MAIL_LOCAL_BUFFER_SIZE;
	char *pCursor = pBuffer;

	SceRtcTick tick;
	if (pMessage->pTime != NULL)
		tick = *pMessage->pTime;
	else
		sceRtcGetCurrentTick(&tick);

	std::string dir;
	dir = pMountPoint;
	dir += "received/";
	sceIoMkdir(dir.c_str(), 0x186);

	{
		MessageCounter counter;
		SceIoDirent dirent;
		std::string root(dir.c_str());
		WalkDirectory(&dirent, root, &counter);
		if (counter.count > MAIL_LOCAL_MAX_MESSAGES)
			return SCE_MAIL_API_FOR_LOCAL_ERROR_TOO_MANY_MESSAGES;
	}

	/* the first free name, counting up from the time */
	char *pName = pBuffer + sizeof(SceIoStat);
	SceUInt64 id = tick.tick;
	for (;;) {
		snprintf(pName, MAIL_LOCAL_BUFFER_SIZE - sizeof(SceIoStat), "%s%llu.bin", dir.c_str(), id);
		if (sceIoGetstat(pName, (SceIoStat *)pBuffer) != 0)
			break;
		id = ++tick.tick;
	}
	dir = pName;

	std::string file(pMountPoint);
	file += "localmessage.bin";
	FileCloser out;
	out.fd = sceIoOpen(file.c_str(), 0x602, 0x186);
	if (out.fd < 0)
		out.fd = -1;

	if (pMessage->pTitle != NULL && strlen(pMessage->pTitle) != 0) {
		if (PutRecord(out.fd, pBuffer, &pCursor, pEnd, MAIL_LOCAL_TAG_TITLE, pMessage->pTitle,
			      strlen(pMessage->pTitle)) != 0)
			return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_WRITE;
	}
	if (pMessage->pText != NULL && strlen(pMessage->pText) != 0) {
		if (PutRecord(out.fd, pBuffer, &pCursor, pEnd, MAIL_LOCAL_TAG_TEXT, pMessage->pText,
			      strlen(pMessage->pText)) != 0)
			return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_WRITE;
	}

	if ((SceSize)(pEnd - pCursor) < 0x10) {
		if (FlushBuffer(out.fd, pBuffer, &pCursor) != 0)
			return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_WRITE;
	}
	((SceUInt32 *)pCursor)[0] = MAIL_LOCAL_TAG_TIME;
	((SceUInt32 *)pCursor)[1] = sizeof(SceUInt64);
	memcpy_s(pCursor + 8, pEnd - (pCursor + 8), &tick, sizeof(SceUInt64));
	pCursor += 16;

	if (pMessage->pAddress != NULL && strlen(pMessage->pAddress) != 0) {
		if (PutRecord(out.fd, pBuffer, &pCursor, pEnd, MAIL_LOCAL_TAG_ADDRESS, pMessage->pAddress,
			      strlen(pMessage->pAddress)) != 0)
			return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_WRITE;
	}

	MailLocalSlot slots[4];
	memcpy(slots, s_slotTemplate, sizeof(slots));
	slots[0].pAttachment = pMessage->pBody;
	slots[1].pAttachment = pMessage->pImage;
	slots[2].pAttachment = pMessage->pThumbnail;
	slots[3].pAttachment = pMessage->pIcon;

	for (SceUInt32 i = 0; i < 4; i++) {
		const MailLocalAttachment *pAttachment = slots[i].pAttachment;
		if (pAttachment == NULL)
			continue;

		if (pAttachment->isFile) {
			std::string path((const char *)pAttachment->pData);
			SceBool exists = SCE_TRUE;
			SceSize fileSize = 0;

			SceIoStat *pStat = (SceIoStat *)malloc(sizeof(SceIoStat));
			if (pStat != NULL) {
				if (sceIoGetstat(path.c_str(), pStat) != 0) {
					exists = SCE_FALSE;
					fileSize = 0;
				} else {
					fileSize = pStat->st_size;
				}
				free(pStat);
			}
			if (exists) {
				if (slots[i].tag == MAIL_LOCAL_TAG_BODY) {
					if (fileSize > 4 * slots[i].limit)
						return SCE_MAIL_API_FOR_LOCAL_ERROR_TOO_LARGE;
					void *pCheck = AllocateZeroed(fileSize + 1);
					if (pCheck == NULL)
						return SCE_MAIL_API_FOR_LOCAL_ERROR_NO_MEMORY;
					SceUID checkFd = sceIoOpen(path.c_str(), SCE_O_RDONLY, 0400);
					if (checkFd < 0) {
						free(pCheck);
						return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_READ;
					}
					if (sceIoRead(checkFd, pCheck, fileSize) == 0) {
						sceIoClose(checkFd);
						free(pCheck);
						return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_READ;
					}
					sceIoClose(checkFd);
					free(pCheck);
				} else if (fileSize > slots[i].limit) {
					return SCE_MAIL_API_FOR_LOCAL_ERROR_TOO_LARGE;
				}
			}

			SceInt32 ret = PutFile(out.fd, pBuffer, &pCursor, slots[i].tag,
					       (const char *)pAttachment->pData);
			if (ret != 0) {
				if (ret == MAIL_LOCAL_ERROR_SHORT_WRITE)
					return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_READ;
				return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_WRITE;
			}
		} else {
			SceSize size = pAttachment->size;
			if (slots[i].tag != MAIL_LOCAL_TAG_BODY && size > slots[i].limit)
				return SCE_MAIL_API_FOR_LOCAL_ERROR_TOO_LARGE;
			if (PutRecord(out.fd, pBuffer, &pCursor, pEnd, slots[i].tag, pAttachment->pData,
				      size) != 0)
				return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_WRITE;
		}
	}

	if (pMessage->pFlags != NULL) {
		if ((SceSize)(pEnd - pCursor) < 0xC) {
			if (FlushBuffer(out.fd, pBuffer, &pCursor) != 0)
				return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_WRITE;
		}
		((SceUInt32 *)pCursor)[0] = MAIL_LOCAL_TAG_FLAGS;
		((SceUInt32 *)pCursor)[1] = sizeof(SceUInt32);
		memcpy_s(pCursor + 8, pEnd - (pCursor + 8), pMessage->pFlags, sizeof(SceUInt32));
		pCursor += 12;
	}

	if (FlushBuffer(out.fd, pBuffer, &pCursor) != 0)
		return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_WRITE;
	if (out.fd != -1)
		sceIoClose(out.fd);
	out.fd = -1;
	if (sceIoRename(file.c_str(), dir.c_str()) != 0)
		return SCE_MAIL_API_FOR_LOCAL_ERROR_IO_WRITE;
	return 0;
}

void MailLocalNoOperation(void)
{
}

SceInt32 mail_api_for_local_libc_2A03C7B4(const MailLocalMessage *pMessage)
{
	SceInt32 result;

	if (s_mountPoint[0] == '0')
		return SCE_MAIL_API_FOR_LOCAL_ERROR_NOT_MOUNTED;
	if (pMessage == NULL)
		return SCE_MAIL_API_FOR_LOCAL_ERROR_INVALID_ARGUMENT;
	if (pMessage->pBody == NULL || pMessage->pBody->size == 0
	    || strlen((const char *)pMessage->pBody->pData) == 0)
		return SCE_MAIL_API_FOR_LOCAL_ERROR_INVALID_ARGUMENT;
	if (pMessage->pAddress != NULL && strlen(pMessage->pAddress) > 9)
		return SCE_MAIL_API_FOR_LOCAL_ERROR_TOO_LARGE;

	result = MailLocalWrite(pMessage, s_mountPoint);
	SceUID pipe = sceKernelOpenMsgPipe("SceMailResidentPipe");
	if (pipe != -1) {
		SceUInt32 notice[28];
		SceSize sent;

		memset(&notice[1], 0, sizeof(notice) - sizeof(notice[0]));
		notice[0] = 2;
		result = sceKernelSendMsgPipe(pipe, notice, sizeof(notice), 1, &sent, NULL);
		sceKernelCloseMsgPipe(pipe);
	}
	return result;
}

extern "C" int module_start(SceSize argSize, const void *argp)
{
	if (s_mountInitialized)
		return 0;

	memset(s_mountPoint, 0, sizeof(s_mountPoint));
	int ret = sceAppMgrWorkDirMount(0xC9, s_mountPoint);
	if (ret == 0)
		s_mountInitialized = true;
	if (ret == (int)0x80800003)
		ret = 0;
	return ret;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	if (s_mountInitialized) {
		sceAppMgrUmount(s_mountPoint);
		s_mountInitialized = false;
	}
	return 0;
}
