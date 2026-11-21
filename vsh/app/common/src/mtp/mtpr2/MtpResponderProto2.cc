/*
 * MtpResponderProto2.cc: the responder that downloads the file from the PC over the MTP
 * connection, protocol 2 (0x811372D8-0x81137894): it starts the MTP client, waits for the
 * connection, opens the object and receives it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <std/stdlib.h>
#include <std/string.h>

#include "mtpr2_internal.h"
#include "../../mtp_client/mtp_client_lib.h"

using namespace mtpr2;

#define PROTO2_CLOSE_DONE		0x2001	/* PLACEHOLDER names */
#define PROTO2_CLOSE_FAILED		0x2002
#define PROTO2_CLOSE_DELAY_US		300000
#define PROTO2_CONNECT_TIMEOUT_US	30000000
#define PROTO2_CONNECT_POLL_US		10000
#define PROTO2_ERROR_CLOSE		((int)0x8054001A)	/* PLACEHOLDER name: meaning not known */
#define PROTO2_ERROR_STATUS		((int)0x80243007)	/* PLACEHOLDER name */

static MtpClient *s_mtpClient;		/* PLACEHOLDER name */

/* the MTP client stopped and deleted */
static inline void deleteClient(void)
{
	if (s_mtpClient != NULL) {
		s_mtpClient->Bgdl_81137ADE();
		MTPR2_PRINT_ERROR();
		if (s_mtpClient != NULL)
			delete s_mtpClient;
		s_mtpClient = NULL;
	}
}

MtpResponderProto2::MtpResponderProto2()
{
	started = true;
	handle = -1;
	offset = 0;
	remaining = 0;
	aborted = false;
}

int MtpResponderProto2::endDownload(void)
{
	int ret = 0;
	int mode;

	if (started == false)
		return MTPR2_ERROR_NOT_STARTED;
	mode = PROTO2_CLOSE_DONE;
	if (failed != false)
		mode = PROTO2_CLOSE_FAILED;
	MTPR2_PRINT_ERROR();
	if (s_mtpClient != NULL) {
		s_mtpClient->Bgdl_81138068(handle, mode);
		sceKernelDelayThread(PROTO2_CLOSE_DELAY_US);
		ret = s_mtpClient->Bgdl_81138184(handle);
		sceKernelDelayThread(PROTO2_CLOSE_DELAY_US);
		deleteClient();
		if (ret < 0) {
			if (ret == PROTO2_ERROR_CLOSE)
				MTPR2_PRINT_ERROR();
			else
				MTPR2_PRINT_ERROR();
		}
	}
	started = false;
	failed = false;
	if (ret != 0)
		return ret;
	return 0;
}

MtpResponderProto2::~MtpResponderProto2()
{
	if (started != false)
		endDownload();
	started = false;
}

/* the MTP client, started once */
static __attribute__((noinline)) int createClient(void)
{
	int ret = 0;

	if (s_mtpClient == NULL) {
		s_mtpClient = new MtpClient();
		if (s_mtpClient == NULL)
			return MTP_CLIENT_ERROR_NO_MEMORY;
		ret = s_mtpClient->initializeClient();
		if (ret != 0) {
			MTPR2_PRINT_ERROR();
			deleteClient();
		}
	}
	return ret;
}

int MtpResponderProto2::startDownload(const char *url, long long offset, int *kind,
				      unsigned int *size, unsigned int *time)
{
	MtpClientStatus status;
	MtpClientStatus connection;
	char data[4];
	unsigned int received;
	int mode;
	int ret;

	*size = 0;
	failed = false;
	*time = 0;
	this->offset = offset;
	sce_paf_printf_dummy();
	ret = createClient();
	if (ret != 0) {
		MTPR2_PRINT_ERROR();
		return ret;
	}
	ret = s_mtpClient->Bgdl_81138432(&status);
	if (ret != 0) {
		MTPR2_PRINT_ERROR();
		return ret;
	}
	mode = 0;
	ret = s_mtpClient->Bgdl_81138498(&mode);
	if (ret != 0) {
		MTPR2_PRINT_ERROR();
		return ret;
	}
	if (mode == 2 || (status.state != 4 && status.state != 3)) {
		ret = s_mtpClient->Bgdl_811384C4();
		if (ret != 0 && ret != MTP_CLIENT_ERROR_BUSY) {
			MTPR2_PRINT_ERROR();
			return ret;
		}
	}
	ret = s_mtpClient->Bgdl_811384AE(1);
	if (ret != 0 && ret != MTP_CLIENT_ERROR_BUSY) {
		MTPR2_PRINT_ERROR();
		return ret;
	}

	sce_paf_memset(&connection, 0, sizeof(connection));

	SceInt64 start = sceKernelGetSystemTimeWide();

	ret = 0;
	while (sceKernelGetSystemTimeWide() - start <= PROTO2_CONNECT_TIMEOUT_US) {
		if (s_mtpClient == NULL) {
			ret = MTP_CLIENT_ERROR_NOT_CONNECTED;
			goto error;
		}
		ret = s_mtpClient->Bgdl_81138432(&connection);
		if (ret != 0) {
			MTPR2_PRINT_ERROR();
			goto connected;
		}
		if (connection.state > 2 && connection.state <= 4)
			goto connected;
		if (aborted == true)
			goto connected;
		sceKernelDelayThread(PROTO2_CONNECT_POLL_US);
	}
	MTPR2_PRINT_ERROR();
connected:
	if (ret != 0) {
error:
		MTPR2_PRINT_ERROR();
		return ret;
	}
	if (aborted != false)
		return MTPR2_ERROR_CANCELED;
	MTPR2_PRINT_ERROR();
	ret = s_mtpClient->Bgdl_81138432(&status);
	if (ret != 0) {
		MTPR2_PRINT_ERROR();
		if (ret == PROTO2_ERROR_STATUS)
			MTPR2_PRINT_ERROR();
		return ret;
	}
	switch (status.kind) {
	case 1:
		*kind = 1;
		break;
	case 2:
		*kind = 2;
		break;
	default:
		MTPR2_PRINT_ERROR();
		*kind = 3;
		break;
	}
	ret = s_mtpClient->Bgdl_811380BC(url, sce_paf_strlen(url) + 1, 0, 0, &handle);
	if (ret != 0) {
		MTPR2_PRINT_ERROR();
		s_mtpClient->Bgdl_81137ADE();
		return ret;
	}
	ret = s_mtpClient->Bgdl_81138266(handle, data, 0, &received, &remaining);
	if (ret != 0) {
		int err;

		MTPR2_PRINT_ERROR();
		failed = true;
		err = endDownload();
		if (err != 0) {
			MTPR2_PRINT_ERROR();
			ret = err;
		}
	}
	MTPR2_PRINT_ERROR();
	*size = remaining;
	return ret;
}

int MtpResponderProto2::receiveData(void *buf, unsigned int size, unsigned int *received)
{
	int ret;

	if (remaining == 0) {
		MTPR2_PRINT_ERROR();
		*received = 0;
		return 0;
	}
	if (s_mtpClient == NULL)
		return MTP_CLIENT_ERROR_NOT_CONNECTED;
	ret = s_mtpClient->Bgdl_81138266(handle, buf, size, received, &remaining);
	if (ret >= 0)
		return 0;
	MTPR2_PRINT_ERROR();
	*received = 0;
	failed = true;

	int err = endDownload();

	if (err != 0) {
		MTPR2_PRINT_ERROR();
		return err;
	}
	return ret;
}

int MtpResponderProto2::abortDownload(void)
{
	int ret;

	if (started == false)
		return MTPR2_ERROR_CANCELED;
	aborted = true;
	MTPR2_PRINT_ERROR();
	if (s_mtpClient == NULL)
		return MTP_CLIENT_ERROR_NOT_CONNECTED;
	ret = s_mtpClient->Bgdl_8113844A(handle);
	if (ret < 0)
		MTPR2_PRINT_ERROR();
	return ret;
}
