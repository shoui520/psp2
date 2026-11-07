/*
 * The check-in library's connection (SceNearCheckinLibConnectJob): launches the near background
 * service in the job's mode and connects to it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <appmgr.h>
#include <std/string.h>
#include <std/string>
#include <std/list>
#include <common/string_util.h>
#include <thread/thread.h>
#include "near_checkin_lib.h"

using namespace paf;

/* ScePafStdc's strtol, which paf's public std/stdlib.h does not declare yet */
extern "C" long sce_paf_strtol(const char *str, char **endptr, int base);

NearCheckinLibConnectJob::NearCheckinLibConnectJob()
	: NearCheckinLibJob("SceNearCheckinLibConnectJob")
{
}

NearCheckinLibConnectJob::~NearCheckinLibConnectJob()
{
}

int NearCheckinLibConnectJob::Setup(SceInt32 type, NearCheckinLib *owner, EndFunction end,
				    CheckinStatusFunction checkin,
				    ItemDownloadStatusFunction download,
				    const string &giftDiscoveringId)
{
	int ret;

	switch (type) {
	case NearCheckinLib::MODE_SDK_CHECKIN:
	case NearCheckinLib::MODE_SDK_CHECKIN_RETRY:
		if (checkin == NULL)
			return NearCheckinLib::RESULT_PARAM;
		break;
	case NearCheckinLib::MODE_SDK_ITEM_DL:
		if (download == NULL)
			return NearCheckinLib::RESULT_PARAM;
		break;
	default:
		return NearCheckinLib::RESULT_PARAM;
	}
	ret = NearCheckinLibJob::Setup(type, owner, end, checkin, download);
	if (ret != 0)
		return ret;
	m_giftDiscoveringId = giftDiscoveringId;
	return 0;
}

/* the parts of a gift discovering ID, with or without its "near:" */
static int NearParseGiftDiscoveringId(const string &giftDiscoveringId, NearGiftDiscoveringId *id)
{
	char *end;

	if (id == NULL)
		return NearCheckinLib::RESULT_PARAM;

	list<string> parts;

	common::string_util::tokenize(parts, giftDiscoveringId, 0, ',');
	end = NULL;

	list<string>::iterator it = parts.begin();
	bool prefixed;

	{
		string prefix("near:");

		prefixed = sce_paf_strcmp(it->substr(0, 5).c_str(), prefix.c_str()) == 0;
	}
	if (prefixed)
		++it;
	if (it->size() > NEAR_GIFT_DISCOVERING_ID_NUMBER_LEN)
		return NearCheckinLib::RESULT_PARAM;
	id->number = sce_paf_strtol(it->c_str(), &end, 16);
	++it;
	id->code = *it;
	if (id->code.size() > NEAR_GIFT_DISCOVERING_ID_CODE_LEN)
		return NearCheckinLib::RESULT_PARAM;
	++it;
	id->name = *it;
	if (id->name.size() > NEAR_GIFT_DISCOVERING_ID_NAME_LEN)
		return NearCheckinLib::RESULT_PARAM;
	return 0;
}

/* launches the service with the job's mode */
int NearCheckinLibConnectJob::Launch(void)
{
	string param("");
	int ret;

	switch (GetType()) {
	case NearCheckinLib::MODE_SDK_CHECKIN:
	case NearCheckinLib::MODE_SDK_CHECKIN_RETRY:
		NearMakeAutoCheckinParam(param, GetType(), &m_giftDiscoveringId);
		break;
	case NearCheckinLib::MODE_SDK_ITEM_DL: {
		NearGiftDiscoveringId id;

		ret = NearParseGiftDiscoveringId(m_giftDiscoveringId, &id);
		if (ret != 0)
			return ret;
		NearMakeAutoCheckinParam(param, GetType(), &id);
		break;
	}
	default:
		return NearCheckinLib::RESULT_STATE;
	}

	string uri("near_auto_checkin:");

	uri.append(param);
	ret = sceAppMgrLaunchAppByUri(0, uri.c_str());
	if (ret != 0)
		return ret;
	return 0;
}

/* connects to the service, which is launched, a few times while it is not found yet */
static inline int NearCheckinLibConnect(const NearBGServiceConnectParam *param)
{
	int ret;

	for (int i = 0;;) {
		ret = NearBGServiceConnect(param);
		if (ret == 0)
			return 0;
		if (ret != NEAR_BG_SERVICE_ERROR_NOT_FOUND)
			return ret;
		if (++i >= NEAR_CHECKIN_LIB_CONNECT_TRIES)
			return ret;
		thread::Sleep(NEAR_CHECKIN_LIB_RETRY_INTERVAL);
	}
}

int NearCheckinLibConnectJob::Run()
{
	int ret;

	if (IsCancelRequested())
		return NEAR_CHECKIN_LIB_JOB_CANCELED;
	for (int tries = 0;;) {
		NearBGServiceConnectParam param;

		ret = Launch();
		if (ret != 0)
			return ret;
		sce_paf_memset(&param, 0, sizeof(param));
		ret = NearCheckinLibConnect(&param);
		if (ret == 0)
			break;
		if (ret != NEAR_BG_SERVICE_ERROR_NOT_FOUND)
			return ret;
		thread::Sleep(NEAR_CHECKIN_LIB_RETRY_INTERVAL);
		if (++tries >= NEAR_CHECKIN_LIB_LAUNCH_TRIES)
			return NEAR_CHECKIN_LIB_JOB_OTHER_MODE;
	}
	if (IsCancelRequested())
		return NEAR_CHECKIN_LIB_JOB_CANCELED;

	SceInt32 mode = 0;

	ret = NearBGServiceGetMode(&mode);
	if (ret != 0)
		return ret;
	if (GetType() != mode)
		return NEAR_CHECKIN_LIB_JOB_OTHER_MODE;
	if (IsCancelRequested())
		return Abort();

	bool busy = false;

	ret = NearBGServiceIsBusy(&busy);
	if (ret != 0)
		return ret;
	if (busy)
		return NEAR_CHECKIN_LIB_JOB_BUSY;
	if (IsCancelRequested())
		return Abort();

	NearBGServiceReady ready;

	ready.initialized = false;
	ready.permitted = false;
	ret = NearBGServiceGetReady(&ready);
	if (ret != 0)
		return ret;
	if (!ready.initialized || !ready.permitted)
		return NEAR_CHECKIN_LIB_JOB_NOT_READY;
	if (IsCancelRequested())
		return Abort();

	bool finished = false;

	ret = UpdateStatus(&finished);
	if (ret != 0)
		return ret;
	if (IsCancelRequested())
		return Abort();
	return 0;
}
