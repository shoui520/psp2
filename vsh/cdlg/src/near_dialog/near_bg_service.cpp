/*
 * The near background service's client as the check-in library's jobs use it: one connection at a
 * time.
 */
#include <stddef.h>
#include <scetypes.h>
#include "near_checkin_lib.h"

static NearBGServiceClient *s_client;

int NearBGServiceConnect(const NearBGServiceConnectParam *param)
{
	int ret;

	if (param == NULL)
		return NEAR_BG_SERVICE_ERROR_PARAM;
	if (s_client != NULL)
		return NEAR_BG_SERVICE_ERROR_CONNECTED;
	s_client = new NearBGServiceClient();
	if (s_client == NULL)
		return NEAR_BG_SERVICE_ERROR_NO_CLIENT;
	ret = s_client->Connect(param);
	if (ret == 0)
		return 0;
	if (s_client != NULL)
		delete s_client;
	s_client = NULL;
	return ret;
}

int NearBGServiceDisconnect(void)
{
	int ret;

	if (s_client == NULL)
		return NEAR_BG_SERVICE_ERROR_NO_CONNECTION;
	ret = s_client->Disconnect();
	if (ret != 0)
		return ret;
	if (s_client != NULL)
		delete s_client;
	s_client = NULL;
	return 0;
}

int NearBGServiceGetItemDownloadStatus(NearItemDownloadStatus *status)
{
	if (s_client == NULL)
		return NEAR_BG_SERVICE_ERROR_NO_CONNECTION;
	if (status == NULL)
		return NEAR_BG_SERVICE_ERROR_PARAM;
	return s_client->GetItemDownloadStatus(status);
}

int NearBGServiceCancelItemDownload(void)
{
	if (s_client == NULL)
		return NEAR_BG_SERVICE_ERROR_NO_CONNECTION;
	return s_client->CancelItemDownload();
}

int NearBGServiceGetCheckinStatus(NearCheckinStatus *status)
{
	if (s_client == NULL)
		return NEAR_BG_SERVICE_ERROR_NO_CONNECTION;
	if (status == NULL)
		return NEAR_BG_SERVICE_ERROR_PARAM;
	return s_client->GetCheckinStatus(status);
}

int NearBGServiceCancelCheckin(void)
{
	if (s_client == NULL)
		return NEAR_BG_SERVICE_ERROR_NO_CONNECTION;
	return s_client->CancelCheckin();
}

int NearBGServiceGetMode(SceInt32 *mode)
{
	if (s_client == NULL)
		return NEAR_BG_SERVICE_ERROR_NO_CONNECTION;
	if (mode == NULL)
		return NEAR_BG_SERVICE_ERROR_PARAM;
	return s_client->GetMode(mode);
}

int NearBGServiceIsBusy(bool *busy)
{
	if (s_client == NULL)
		return NEAR_BG_SERVICE_ERROR_NO_CONNECTION;
	if (busy == NULL)
		return NEAR_BG_SERVICE_ERROR_PARAM;
	return s_client->IsBusy(busy);
}

int NearBGServiceGetReady(NearBGServiceReady *ready)
{
	if (s_client == NULL)
		return NEAR_BG_SERVICE_ERROR_NO_CONNECTION;
	if (ready == NULL)
		return NEAR_BG_SERVICE_ERROR_PARAM;
	return s_client->GetReady(ready);
}
