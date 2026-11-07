/*
 * The near background service's client: the IPMI connection to NearBGService, and its methods,
 * which note when the service has ended the connection.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "../../../lowlayer/src/ipmi/client.h"
#include "../../../lowlayer/src/ipmi/ipmi_error.h"
#include "near_checkin_lib.h"

int NearBGServiceClient::Connect(const NearBGServiceConnectParam *param)
{
	void *memory;
	int result;
	int ret;

	if (m_client != NULL)
		return NEAR_BG_SERVICE_ERROR_CONNECTED;

	IPMI::Client::Config config;

	sce_paf_strncpy(config.serviceName, NEAR_BG_SERVICE_NAME, sizeof(config.serviceName));
	config.msgQueueCount = 1;
	config.msgQueueBufferSize[0] = NEAR_BG_SERVICE_MSG_QUEUE_SIZE;
	memory = sce_paf_malloc(IPMI::Client::getBufferSize(&config));
	if (memory == NULL)
		return NEAR_BG_SERVICE_ERROR_NO_MEMORY;
	m_buffer = sce_paf_malloc(NEAR_BG_SERVICE_BUFFER_SIZE);
	if (m_buffer == NULL) {
		sce_paf_free(memory);
		return NEAR_BG_SERVICE_ERROR_NO_MEMORY;
	}
	ret = IPMI::Client::create(&m_client, &config, this, memory);
	if (ret != 0) {
		sce_paf_free(memory);
		sce_paf_free(m_buffer);
		return ret;
	}
	ret = m_client->connect(param, sizeof(*param), &result);
	if (ret != 0) {
		int err = NEAR_BG_SERVICE_ERROR_UNKNOWN;

		switch (ret) {
		case IPMI_ERROR_SERVER_NOT_FOUND:
			err = NEAR_BG_SERVICE_ERROR_NOT_FOUND;
			break;
		case IPMI_ERROR_CONNECTION_REFUSED:
			err = result;
			break;
		}
		m_client->destroy();
		m_client = NULL;
		sce_paf_free(memory);
		sce_paf_free(m_buffer);
		return err;
	}
	m_terminated = false;
	m_reserved1 = -1;
	m_reserved2 = -1;
	return 0;
}

int NearBGServiceClient::Disconnect(void)
{
	if (m_client == NULL)
		return NEAR_BG_SERVICE_ERROR_NOT_CONNECTED;
	if (m_terminated)
		m_client->terminateConnection();
	else if (m_client->disconnect() == IPMI_ERROR_BUSY)
		return NEAR_BG_SERVICE_ERROR_CONNECTED;
	m_client->destroy();
	sce_paf_free(m_client);
	m_client = NULL;
	sce_paf_free(m_buffer);
	return 0;
}

void NearBGServiceClient::Update(void)
{
}


int NearBGServiceClient::GetItemDownloadStatus(NearItemDownloadStatus *status)
{
	int ret = NearBGServiceInvokeGetItemDownloadStatus(m_client, status);

	if (ret != 0 && ret == IPMI_ERROR_TERMINATED)
		m_terminated = true;
	return ret;
}

int NearBGServiceClient::CancelItemDownload(void)
{
	int ret = NearBGServiceInvokeCancelItemDownload(m_client);

	if (ret != 0 && ret == IPMI_ERROR_TERMINATED)
		m_terminated = true;
	return ret;
}

int NearBGServiceClient::GetCheckinStatus(NearCheckinStatus *status)
{
	int ret = NearBGServiceInvokeGetCheckinStatus(m_client, status);

	if (ret != 0 && ret == IPMI_ERROR_TERMINATED)
		m_terminated = true;
	return ret;
}

int NearBGServiceClient::CancelCheckin(void)
{
	int ret = NearBGServiceInvokeCancelCheckin(m_client);

	if (ret != 0 && ret == IPMI_ERROR_TERMINATED)
		m_terminated = true;
	return ret;
}

int NearBGServiceClient::GetMode(SceInt32 *mode)
{
	int ret = NearBGServiceInvokeGetMode(m_client, mode);

	if (ret != 0 && ret == IPMI_ERROR_TERMINATED)
		m_terminated = true;
	return ret;
}

int NearBGServiceClient::IsBusy(bool *busy)
{
	int ret = NearBGServiceInvokeIsBusy(m_client, busy);

	if (ret != 0 && ret == IPMI_ERROR_TERMINATED)
		m_terminated = true;
	return ret;
}

int NearBGServiceClient::GetReady(NearBGServiceReady *ready)
{
	int ret = NearBGServiceInvokeGetReady(m_client, ready);

	if (ret != 0 && ret == IPMI_ERROR_TERMINATED)
		m_terminated = true;
	return ret;
}
