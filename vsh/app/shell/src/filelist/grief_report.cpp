/*
 * grief_report.cpp: temporary grief report file builder and URI launcher for
 * reported activity comments (0x8135D95C..0x8135F46F).
 */
#include "filelist_internal.h"
#include <file/localfile.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>

namespace filelist {

namespace {

enum GriefReportError {
	GRIEF_REPORT_ERR_INVALID_ARG = (int)0x80106271,
	GRIEF_REPORT_ERR_STATE       = (int)0x80106275,
	GRIEF_REPORT_ERR_NOT_READY   = (int)0x80106276,
	GRIEF_REPORT_ERR_NO_MEMORY   = (int)0x80106277
};

enum GriefReportState {
	GRIEF_REPORT_STATE_IDLE = 0,
	GRIEF_REPORT_STATE_WRITING_HEADER = 1,
	GRIEF_REPORT_STATE_HEADER_WRITTEN = 2,
	GRIEF_REPORT_STATE_COMMENT_WRITTEN = 3,
	GRIEF_REPORT_STATE_CLOSED = 4,
	GRIEF_REPORT_STATE_ERROR = 5
};

enum {
	GRIEF_REPORT_WORKDIR_ID = 203,
	GRIEF_REPORT_APPMGR_ALREADY_MOUNTED = (int)0x80800003,
	GRIEF_REPORT_LAUNCH_FLAGS = 0x60000,
	GRIEF_REPORT_OPEN_FLAGS = 0x602,
	GRIEF_REPORT_OPEN_MODE = 6,
	GRIEF_REPORT_TYPE_ACTIVITY_COMMENT = 2,
	GRIEF_REPORT_STATUS_TEXT_CAPACITY = 0x800,
	GRIEF_REPORT_COMMENT_TEXT_CAPACITY = 0x400
};

/* Binary header written at offset 0 of grief_report.tmp (1184 bytes = 0x4A0) */
struct GriefReportBinaryHeader {
	char commentOwnerId[64];
	SceInt64 commentCreationTime;
	char statusOwnerId[64];
	int statusId;
	int commentId;
	char commentMessage[1040];
};

typedef char GriefReportBinaryHeaderSizeCheck[sizeof(GriefReportBinaryHeader) == 1184 ? 1 : -1];

char s_griefReportMountPoint[16];
paf::common::SharedPtr<paf::File> s_griefReportFile;
int s_griefReportBytesWritten = 0;
int s_griefReportState = GRIEF_REPORT_STATE_IDLE;

/* 0x8135D95C */
paf::string MountGriefReportWorkDir()
{
	sce_paf_memset(s_griefReportMountPoint, 0, sizeof(s_griefReportMountPoint));
	int ret = sceAppMgrWorkDirMount(GRIEF_REPORT_WORKDIR_ID, s_griefReportMountPoint);
	if (ret >= 0 || ret == GRIEF_REPORT_APPMGR_ALREADY_MOUNTED)
		return paf::string(s_griefReportMountPoint);
	return paf::string();
}

/* 0x8135D9CA */
int LaunchGriefReportApp(const char *reportPath, int reportSize, int reportType)
{
	(void)reportPath;
	char numBuf[64];
	sce_paf_memset(numBuf, 0, sizeof(numBuf));

	paf::string uri("psgr:send");
	uri += "?";
	uri += "type";
	uri += "=";
	sce_paf_snprintf(numBuf, sizeof(numBuf), "%d", reportType);
	uri += numBuf;
	uri += "&";
	uri += "temppath";
	uri += "=";
	uri += "grief_report.tmp";
	uri += "&";
	uri += "tempsize";
	uri += "=";
	sce_paf_snprintf(numBuf, sizeof(numBuf), "%d", reportSize);
	uri += numBuf;

	sceAppMgrLaunchAppByUri(GRIEF_REPORT_LAUNCH_FLAGS, uri.c_str());
	return 0;
}

/* 0x8135DEF4 */
int WriteStatusHeader(const ActivityDbStatus *status, const ActivityDbComment *comment,
		      paf::common::SharedPtr<paf::File> file, int *outBytesWritten)
{
	GriefReportBinaryHeader header;
	sce_paf_memset(&header, 0, sizeof(header));
	sce_paf_memcpy(header.commentOwnerId, comment->ownerId.c_str(), comment->ownerId.size());
	sce_paf_memcpy(header.statusOwnerId, status->ownerId.c_str(), status->ownerId.size());
	sce_paf_memcpy(&header.commentCreationTime, &comment->creationTime,
		       sizeof(header.commentCreationTime));
	header.statusId = status->statusId;
	header.commentId = comment->commentId;
	sce_paf_memcpy(header.commentMessage, comment->message.c_str(), comment->message.size());

	int written = 0;
	if (file.get() != NULL) {
		written = file->Write(&header, sizeof(header));
		if (written <= 0) {
			file->Close();
			return written;
		}
	}
	*outBytesWritten = written;

	paf::string ownerName(status->ownerId);
	BaseActivityVisualizer::TruncateAtChar(ownerName, '@');

	paf::string ownerLine("Online ID: ");
	ownerLine += ownerName.c_str();
	ownerLine += "\n";

	char formatBuf[40];
	paf::string statusIdLine("Status ID: ");
	sce_paf_snprintf(formatBuf, 32, "%d", status->statusId);
	statusIdLine += formatBuf;
	statusIdLine += "\n";

	paf::string activityLine("Activity: ");
	activityLine += status->message.c_str();
	activityLine += "\n";

	sce_paf_memset(formatBuf, 0, 32);
	paf::string whenLine("When: ");
	SceRtcTick tick;
	tick.tick = static_cast<SceUInt64>(status->creationTime);
	sceRtcFormatRFC3339(formatBuf, &tick, 0);
	whenLine += formatBuf;
	whenLine += "\n";

	size_t totalLen = ownerLine.size() + statusIdLine.size() + activityLine.size() +
			  whenLine.size() + 1;
	if (totalLen > GRIEF_REPORT_STATUS_TEXT_CAPACITY)
		return 0;

	char *textBuf = static_cast<char *>(sce_paf_malloc(GRIEF_REPORT_STATUS_TEXT_CAPACITY));
	if (textBuf == NULL)
		return GRIEF_REPORT_ERR_NO_MEMORY;

	sce_paf_memset(textBuf, 0, GRIEF_REPORT_STATUS_TEXT_CAPACITY);
	sce_paf_snprintf(textBuf, totalLen, "%s%s%s%s", ownerLine.c_str(), statusIdLine.c_str(),
			 activityLine.c_str(), whenLine.c_str());

	if (file.get() != NULL) {
		written = file->Write(textBuf, totalLen);
		if (written <= 0) {
			file->Close();
			sce_paf_free(textBuf);
			return written;
		}
	}
	*outBytesWritten += written;
	sce_paf_free(textBuf);
	return 0;
}

/* 0x8135EA8C */
int WriteCommentEntry(const ActivityDbComment *comment,
		      paf::common::SharedPtr<paf::File> file, int *outBytesWritten)
{
	paf::string ownerName(comment->ownerId);
	BaseActivityVisualizer::TruncateAtChar(ownerName, '@');

	paf::string ownerLine("\n");
	ownerLine += "Online ID: ";
	ownerLine += ownerName.c_str();
	ownerLine += "\n";

	char formatBuf[32];
	paf::string commentIdLine("Comment ID: ");
	sce_paf_snprintf(formatBuf, sizeof(formatBuf), "%d", comment->commentId);
	commentIdLine += formatBuf;
	commentIdLine += "\n";

	paf::string commentLine("Comment: ");
	commentLine += comment->message.c_str();
	commentLine += "\n";

	sce_paf_memset(formatBuf, 0, sizeof(formatBuf));
	paf::string whenLine("When: ");
	SceRtcTick tick;
	tick.tick = static_cast<SceUInt64>(comment->creationTime);
	sceRtcFormatRFC3339(formatBuf, &tick, 0);
	whenLine += formatBuf;
	whenLine += "\n";

	size_t totalLen = ownerLine.size() + commentIdLine.size() + commentLine.size() +
			  whenLine.size() + 1;

	char *textBuf = static_cast<char *>(sce_paf_malloc(GRIEF_REPORT_COMMENT_TEXT_CAPACITY));
	if (textBuf == NULL)
		return GRIEF_REPORT_ERR_NO_MEMORY;

	sce_paf_memset(textBuf, 0, GRIEF_REPORT_COMMENT_TEXT_CAPACITY);
	sce_paf_snprintf(textBuf, totalLen, "%s%s%s%s", ownerLine.c_str(), commentIdLine.c_str(),
			 commentLine.c_str(), whenLine.c_str());

	file->Seek(-1, 1);

	int written = 0;
	if (file.get() != NULL) {
		written = file->Write(textBuf, totalLen);
		if (written <= 0) {
			file->Close();
			sce_paf_free(textBuf);
			return written;
		}
	}
	*outBytesWritten += written - 1;
	sce_paf_free(textBuf);
	return 0;
}

} /* namespace */

/* 0x8135E68C */
int activityGriefReportBegin(const ActivityDbStatus *status, const ActivityDbComment *comment)
{
	int err = 0;
	s_griefReportBytesWritten = 0;
	if (s_griefReportState != GRIEF_REPORT_STATE_IDLE &&
	    s_griefReportState != GRIEF_REPORT_STATE_CLOSED &&
	    s_griefReportFile.get() != NULL) {
		err = s_griefReportFile->Close();
		s_griefReportFile.reset();
	}

	s_griefReportState = GRIEF_REPORT_STATE_IDLE;
	if (status == NULL || comment == NULL)
		return GRIEF_REPORT_ERR_INVALID_ARG;

	paf::string filePath = MountGriefReportWorkDir();
	if (filePath.empty())
		return GRIEF_REPORT_ERR_STATE;

	filePath += "grief_report.tmp";
	s_griefReportFile = paf::LocalFile::Open(filePath.c_str(), GRIEF_REPORT_OPEN_FLAGS,
						 GRIEF_REPORT_OPEN_MODE, &err);
	if (err != 0) {
		s_griefReportState = GRIEF_REPORT_STATE_ERROR;
		return err;
	}

	s_griefReportState = GRIEF_REPORT_STATE_WRITING_HEADER;
	err = WriteStatusHeader(status, comment, s_griefReportFile, &s_griefReportBytesWritten);
	if (err != 0) {
		s_griefReportState = GRIEF_REPORT_STATE_ERROR;
		return err;
	}

	s_griefReportState = GRIEF_REPORT_STATE_HEADER_WRITTEN;
	return 0;
}

/* 0x8135F162 */
int activityGriefReportAppendComment(const ActivityDbComment *comment)
{
	if (comment == NULL)
		return GRIEF_REPORT_ERR_INVALID_ARG;
	if (s_griefReportState != GRIEF_REPORT_STATE_HEADER_WRITTEN &&
	    s_griefReportState != GRIEF_REPORT_STATE_COMMENT_WRITTEN)
		return GRIEF_REPORT_ERR_NOT_READY;

	int err = WriteCommentEntry(comment, s_griefReportFile, &s_griefReportBytesWritten);
	if (err != 0) {
		s_griefReportState = GRIEF_REPORT_STATE_ERROR;
		return err;
	}

	s_griefReportState = GRIEF_REPORT_STATE_COMMENT_WRITTEN;
	return 0;
}

/* 0x8135F258 */
int activityGriefReportEnd(bool launchReportApp)
{
	if (s_griefReportState == GRIEF_REPORT_STATE_CLOSED)
		return GRIEF_REPORT_ERR_STATE;

	if (s_griefReportFile.get() != NULL) {
		int err = s_griefReportFile->Close();
		s_griefReportFile.reset();
		if (err != 0) {
			sceAppMgrUmount(s_griefReportMountPoint);
			return err;
		}
	}

	sceAppMgrUmount(s_griefReportMountPoint);
	s_griefReportState = GRIEF_REPORT_STATE_CLOSED;

	if (launchReportApp) {
		paf::string tempPath("ur0:/temp/grief_report/");
		tempPath += "grief_report.tmp";
		return LaunchGriefReportApp(tempPath.c_str(), s_griefReportBytesWritten,
					    GRIEF_REPORT_TYPE_ACTIVITY_COMMENT);
	}
	return 0;
}

} /* namespace filelist */
