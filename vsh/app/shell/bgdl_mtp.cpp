/*
 * bgdl_mtp.cpp: the background download library and MTP (0x810FCC30-0x810FCD10): the host's
 * functions the shell gives libmtp (they go to the library), and the library's calls into
 * libmtp's background copy. No record names the file or its functions: the names are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>

#include "shell.h"
#include "src/esecron/include/esetask.hpp"
#include "src/livespace_db/lsdb_keys.h"

/* libmtp's background copy (mtp_bgcopy_if.cpp); libmtp.h does not declare them yet */
extern "C" {
int SceLibMtp_E3EBABB4(SceUInt32 id, MtpBgCopyProgress *progress);
int SceLibMtp_981E220E(SceUInt32 id);
int SceLibMtp_FC6D4772(void);
int SceLibMtp_BF311CC8(const MtpBgCopyRunParam *param);
}

/* (0x810FCC30) */
int shellMtpHostCheckContent(const BgdlMtpContent *content)
{
	return bgdlCheckMtpContent(content, 0);
}

/* (0x810FCC3A) */
int shellMtpHostAddTask(const BgdlMtpRequest *request, SceUInt32 *id)
{
	return bgdlAddMtpTask(request, id);
}

/* (0x810FCC42) */
int shellMtpHostRemoveTask(SceUInt32 id)
{
	return bgdlRemoveTask(id);
}

/* (0x810FCC4A) */
int shellMtpHostReloadTask(SceUInt32 id)
{
	return bgdlReloadTask(id);
}

/* (0x810FCC52) */
int shellMtpHostListTasks(void)
{
	return bgdlListMtpTasks();
}

/* (0x810FCC5A) */
paf::string shellMtpHostGetTaskFolder(int taskId)
{
	return bgdlTaskFolder(taskId, false);
}

/* (0x810FCC64) */
int bgdlMtpGetCopyProgress(SceUInt32 id, MtpBgCopyProgress *progress)
{
	return SceLibMtp_E3EBABB4(id, progress);
}

/* (0x810FCC6C) */
int bgdlMtpCancelCopy(SceUInt32 id)
{
	return SceLibMtp_981E220E(id);
}

/* (0x810FCC74) the copy runs only while libmtp lets one start */
int bgdlMtpRunCopy(const MtpBgCopyRunParam *param)
{
	if (param == NULL)
		return 0x80103A0E;
	if (SceLibMtp_FC6D4772() != 0)
		return SceLibMtp_BF311CC8(param);
	return 0x80546610;
}
