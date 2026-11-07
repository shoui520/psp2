/*
 * The check-in library's jobs: their mode, their owner and what they call with the service's status
 * and when they end (near_checkin_lib.h).
 */
#include <stddef.h>
#include <scetypes.h>
#include <job/job.h>
#include "near_checkin_lib.h"

using namespace paf;

int NearCheckinLibJob::Setup(SceInt32 type, NearCheckinLib *owner, EndFunction end,
			     CheckinStatusFunction checkin, ItemDownloadStatusFunction download)
{
	switch (type) {
	case NearCheckinLib::MODE_SDK_CHECKIN:
	case NearCheckinLib::MODE_SDK_CHECKIN_RETRY:
	case NearCheckinLib::MODE_SDK_ITEM_DL:
		break;
	default:
		return NearCheckinLib::RESULT_PARAM;
	}
	if (owner == NULL)
		return NearCheckinLib::RESULT_PARAM;
	if (end == NULL)
		return NearCheckinLib::RESULT_PARAM;
	m_type = type;
	m_owner = owner;
	m_end = end;
	m_checkin = checkin;
	m_download = download;
	return 0;
}

void NearCheckinLibJob::RequestAbort(void)
{
	m_abortRequested = true;
}

bool NearCheckinLibJob::IsAbortRequested(void)
{
	return m_abortRequested;
}

void NearCheckinLibJob::Cancel()
{
	m_cancelRequested = true;
}

bool NearCheckinLibJob::IsCancelRequested()
{
	return m_cancelRequested;
}

NearCheckinLibJob::NearCheckinLibJob(const char *name)
	: job::JobItem(name, NULL), m_cancelRequested(false), m_type(0), m_abortRequested(false),
	  m_owner(NULL), m_end(NULL), m_checkin(NULL), m_download(NULL)
{
}

NearCheckinLibJob::~NearCheckinLibJob()
{
}

void NearCheckinLibJob::Finish(int result)
{
	if (m_end != NULL) {
		m_end(m_owner, result);
		m_owner = NULL;
		m_end = NULL;
		m_checkin = NULL;
		m_download = NULL;
	}
}

SceInt32 NearCheckinLibJob::GetType(void)
{
	return m_type;
}

/* reads the service's status and hands it to the owner */
int NearCheckinLibJob::UpdateStatus(bool *finished)
{
	int ret;

	*finished = false;
	switch (m_type) {
	case NearCheckinLib::MODE_SDK_CHECKIN:
	case NearCheckinLib::MODE_SDK_CHECKIN_RETRY: {
		NearCheckinStatus status;

		if (m_owner == NULL)
			return NearCheckinLib::RESULT_STATE;
		if (m_checkin == NULL)
			return NearCheckinLib::RESULT_STATE;
		ret = NearBGServiceGetCheckinStatus(&status);
		m_checkin(m_owner, ret, &status);
		*finished = status.finished;
		break;
	}
	case NearCheckinLib::MODE_SDK_ITEM_DL: {
		NearItemDownloadStatus status;

		if (m_owner == NULL)
			return NearCheckinLib::RESULT_STATE;
		if (m_download == NULL)
			return NearCheckinLib::RESULT_STATE;
		ret = NearBGServiceGetItemDownloadStatus(&status);
		m_download(m_owner, ret, &status);
		*finished = status.finished;
		break;
	}
	default:
		return NearCheckinLib::RESULT_STATE;
	}
	if (ret != 0)
		return ret;
	return 0;
}

/* cancels the service's own job when the library asked for it */
int NearCheckinLibJob::Abort(void)
{
	int ret;

	switch (m_type) {
	case NearCheckinLib::MODE_SDK_CHECKIN:
	case NearCheckinLib::MODE_SDK_CHECKIN_RETRY:
		if (m_abortRequested) {
			ret = NearBGServiceCancelCheckin();
			if (ret != 0)
				return ret;
		}
		break;
	case NearCheckinLib::MODE_SDK_ITEM_DL:
		if (m_abortRequested) {
			ret = NearBGServiceCancelItemDownload();
			if (ret != 0)
				return ret;
		}
		break;
	default:
		return NearCheckinLib::RESULT_STATE;
	}
	return NEAR_CHECKIN_LIB_JOB_CANCELED;
}
