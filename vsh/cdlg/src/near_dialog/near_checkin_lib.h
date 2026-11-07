/*
 * The check-in library's own interface between its files (near_checkin_lib.cpp and the files of its
 * jobs, of the near background service's client and of near_auto_checkin's parameter): the jobs, the
 * client and what they share. No record names the files, the classes or the functions (the names are
 * ours).
 */
#ifndef NEAR_DIALOG_NEAR_CHECKIN_LIB_H
#define NEAR_DIALOG_NEAR_CHECKIN_LIB_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <job/job.h>
#include "../../../lowlayer/src/ipmi/client.h"
#include "near_dialog_plugin.h"

/* the queue's thread */
#define NEAR_CHECKIN_LIB_QUEUE_PRIORITY		0x10000114
#define NEAR_CHECKIN_LIB_QUEUE_STACK_SIZE	0x4000

/* how often the jobs poll the service (ms), and how long the disconnection is retried */
#define NEAR_CHECKIN_LIB_POLL_INTERVAL		100
#define NEAR_CHECKIN_LIB_TERM_TIMEOUT		5000
/* how often the service is connected to after it has been launched, and launched again */
#define NEAR_CHECKIN_LIB_CONNECT_TRIES		4
#define NEAR_CHECKIN_LIB_LAUNCH_TRIES		3
#define NEAR_CHECKIN_LIB_RETRY_INTERVAL		1000

/* what a job's Run returns when it was canceled */
#define NEAR_CHECKIN_LIB_JOB_CANCELED		255
/* ConnectJob::Run's results: the service runs another mode, is busy, is not ready */
#define NEAR_CHECKIN_LIB_JOB_OTHER_MODE		3
#define NEAR_CHECKIN_LIB_JOB_BUSY		6
#define NEAR_CHECKIN_LIB_JOB_NOT_READY		7

/* the near background service */
#define NEAR_BG_SERVICE_NAME			"NearBGService"
#define NEAR_BG_SERVICE_MSG_QUEUE_SIZE		3120
#define NEAR_BG_SERVICE_BUFFER_SIZE		0x400

/* its methods */
#define NEAR_BG_SERVICE_METHOD_GET_ITEM_DOWNLOAD_STATUS	0x12340000
#define NEAR_BG_SERVICE_METHOD_CANCEL_ITEM_DOWNLOAD	0x12340001
#define NEAR_BG_SERVICE_METHOD_GET_CHECKIN_STATUS	0x12340002
#define NEAR_BG_SERVICE_METHOD_CANCEL_CHECKIN		0x12340003
#define NEAR_BG_SERVICE_METHOD_GET_MODE			0x12340004
#define NEAR_BG_SERVICE_METHOD_IS_BUSY			0x12340005
#define NEAR_BG_SERVICE_METHOD_GET_READY		0x12340006

/* the client's errors */
#define NEAR_BG_SERVICE_ERROR_NOT_FOUND		-101
#define NEAR_BG_SERVICE_ERROR_NO_MEMORY		-200
#define NEAR_BG_SERVICE_ERROR_UNKNOWN		-999
#define NEAR_BG_SERVICE_ERROR_CONNECTED		-2
#define NEAR_BG_SERVICE_ERROR_NOT_CONNECTED	-4
#define NEAR_BG_SERVICE_ERROR_PARAM		-6
#define NEAR_BG_SERVICE_ERROR_NO_CLIENT		-7
#define NEAR_BG_SERVICE_ERROR_NO_CONNECTION	-8

/* the limits of a gift discovering ID's parts */
#define NEAR_GIFT_DISCOVERING_ID_NUMBER_LEN	17
#define NEAR_GIFT_DISCOVERING_ID_CODE_LEN	33
#define NEAR_GIFT_DISCOVERING_ID_NAME_LEN	65

/* what the service is connected with */
struct NearBGServiceConnectParam {
	SceUInt8 reserved;
};

/* whether the service is ready to check in */
struct NearBGServiceReady {
	bool initialized;
	bool permitted;
};

/* a gift discovering ID, "near:<number>,<code>,<name>": the parts of the download's URI */
struct NearGiftDiscoveringId {
	NearGiftDiscoveringId() : number(0)
	{
	}

	paf::string name;
	paf::string code;
	SceUInt32 number;
};

/* what MODE_MANUAL_POST's URI carries */
struct NearManualPostParam {
	SceInt64 id;
	paf::vector<SceUInt64> items;
	paf::vector<SceUInt64> gifts;
	bool option1;
	bool option2;
	bool option3;
};

/* the near background service's client: one connection at a time */
class NearBGServiceClient {
public:
	NearBGServiceClient() : m_client(NULL), m_terminated(false)
	{
	}

	/* does nothing; nothing calls it */
	void Update(void);

	int Connect(const NearBGServiceConnectParam *param);
	int Disconnect(void);
	int GetItemDownloadStatus(NearItemDownloadStatus *status);
	int CancelItemDownload(void);
	int GetCheckinStatus(NearCheckinStatus *status);
	int CancelCheckin(void);
	int GetMode(SceInt32 *mode);
	int IsBusy(bool *busy);
	int GetReady(NearBGServiceReady *ready);

	IPMI::Client *m_client;
	/* set to -1 by the connection; nothing reads them */
	SceInt32 m_reserved1;
	SceInt32 m_reserved2;
	void *m_buffer;
	/* the service ended the connection */
	bool m_terminated;
};

int NearBGServiceConnect(const NearBGServiceConnectParam *param);
int NearBGServiceDisconnect(void);
int NearBGServiceGetItemDownloadStatus(NearItemDownloadStatus *status);
int NearBGServiceCancelItemDownload(void);
int NearBGServiceGetCheckinStatus(NearCheckinStatus *status);
int NearBGServiceCancelCheckin(void);
int NearBGServiceGetMode(SceInt32 *mode);
int NearBGServiceIsBusy(bool *busy);
int NearBGServiceGetReady(NearBGServiceReady *ready);

/* a job of the library: its mode, its owner, what it calls when it ends and with the status */
class NearCheckinLibJob : public paf::job::JobItem {
public:
	typedef void (*EndFunction)(NearCheckinLib *lib, SceInt32 result);
	typedef void (*CheckinStatusFunction)(NearCheckinLib *lib, SceInt32 error,
					      const NearCheckinStatus *status);
	typedef void (*ItemDownloadStatusFunction)(NearCheckinLib *lib, SceInt32 error,
						   const NearItemDownloadStatus *status);

	NearCheckinLibJob(const char *name);
	virtual ~NearCheckinLibJob();
	virtual void Cancel();
	virtual void Finish(int result);
	virtual bool IsCancelRequested();

	int Setup(SceInt32 type, NearCheckinLib *owner, EndFunction end,
		  CheckinStatusFunction checkin, ItemDownloadStatusFunction download);
	void RequestAbort(void);
	bool IsAbortRequested(void);
	SceInt32 GetType(void);
	int UpdateStatus(bool *finished);
	int Abort(void);

	/* set by Cancel */
	bool m_cancelRequested;
	SceInt32 m_type;
	/* set by the library's Cancel: the service's own job is canceled too */
	bool m_abortRequested;
	NearCheckinLib *m_owner;
	EndFunction m_end;
	CheckinStatusFunction m_checkin;
	ItemDownloadStatusFunction m_download;
};

/* launches the service in the job's mode and connects to it */
class NearCheckinLibConnectJob : public NearCheckinLibJob {
public:
	NearCheckinLibConnectJob();
	virtual ~NearCheckinLibConnectJob();
	virtual int Run();

	int Setup(SceInt32 type, NearCheckinLib *owner, EndFunction end,
		  CheckinStatusFunction checkin, ItemDownloadStatusFunction download,
		  const paf::string &giftDiscoveringId);
	int Launch(void);

	paf::string m_giftDiscoveringId;
};

/* polls the service's status until its job has finished */
class NearCheckinLibStatusJob : public NearCheckinLibJob {
public:
	NearCheckinLibStatusJob();
	virtual ~NearCheckinLibStatusJob();
	virtual int Run();
};

/* disconnects from the service */
class NearCheckinLibTermJob : public NearCheckinLibJob {
public:
	NearCheckinLibTermJob();
	virtual ~NearCheckinLibTermJob();
	virtual int Run();
};

/* the service's methods (near_bg_service_ipc_client.cpp) */
int NearBGServiceInvokeGetItemDownloadStatus(IPMI::Client *client,
						    NearItemDownloadStatus *status);
int NearBGServiceInvokeCancelItemDownload(IPMI::Client *client);
int NearBGServiceInvokeGetCheckinStatus(IPMI::Client *client, NearCheckinStatus *status);
int NearBGServiceInvokeCancelCheckin(IPMI::Client *client);
int NearBGServiceInvokeGetMode(IPMI::Client *client, SceInt32 *mode);
int NearBGServiceInvokeIsBusy(IPMI::Client *client, bool *busy);
int NearBGServiceInvokeGetReady(IPMI::Client *client, NearBGServiceReady *ready);

/* near_auto_checkin's parameter for the mode (near_auto_checkin_param.cpp) */
void NearMakeAutoCheckinParam(paf::string &param, SceInt32 mode, const void *data);

#endif
