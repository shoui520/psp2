/*
 * chk_ux0_free_space_job.cpp: background job checking ux0: free space on return
 * from an application and posting a low-space notification if <= 40 MiB
 * (0x81298D24-0x81299047).
 */
#include "topmenu_internal.h"

namespace {

static const SceUInt64 TOPMENU_UX0_MIN_FREE_BYTES = 0x2800000ULL; /* 40 MiB */
static const SceUInt32 TOPMENU_MSG_NEED_FREE_SPACE_ID = 0x6997BA1Cu;

/* 0x81298D24..0x81298EEF */
class ChkUx0FreeSpaceJob : public paf::job::JobItem {
public:
	ChkUx0FreeSpaceJob()
		: paf::job::JobItem("ChkUx0FreeSpaceJob", NULL),
		  m_maxSize(0),
		  m_freeSize(0)
	{
	}

	virtual ~ChkUx0FreeSpaceJob() {}

	/* 0x81298D60 */
	virtual int32_t Run()
	{
		return gameIoGetDeviceSpace("ux0:", &m_maxSize, &m_freeSize);
	}

	/* 0x81298D7A */
	virtual void Finish(int32_t result)
	{
		if (result != 0 || m_freeSize > TOPMENU_UX0_MIN_FREE_BYTES)
			return;

		ImposePluginInterface *imposeIf = g_topWindow->m_imposeIf;
		if (imposeIf == NULL)
			return;

		ImposeNotice notice;
		paf::wstring message = shellFormatText(
			g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_MSG_NEED_FREE_SPACE_ID)),
			L"40MB");
		notice.name = "NeedFreeSpace";
		notice.message = message.c_str();
		notice.callback = NULL;
		imposeIf->showNotice(&notice);
	}

private:
	SceUInt64 m_maxSize;
	SceUInt64 m_freeSize;
};

} /* namespace */

/* 0x81298EF0 */
void topmenuStartCheckUx0FreeSpace(void)
{
	paf::common::SharedPtr<paf::job::JobItem> job(new ChkUx0FreeSpaceJob());
	paf::job::JobQueue::default_queue->Enqueue(job);
}
