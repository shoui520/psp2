/*
 * mtp_storage_usb_handler.cpp: MtpServiceActivationMgr ("ACTIVE"), storage jobs ("STRHDL"),
 * and USB/CD-ROM mode switching and connection monitoring ("USBH")
 * (0x812B48E0 - 0x812B51C5, 0x812B6800 - 0x812B69DB, 0x81431F10 - 0x81432413)
 */
#include <system/system.h>
#include <thread/thread.h>
#include <rtc.h>
extern "C" {
#include <udcd/udcd_user.h>
}
#include <usbstor/usbstor.h>
#include <modulemgr/module_info.h>
#include "mtp_server_internal.h"

extern "C" {
extern int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *info);
}

static SceUInt8 s_usbCdromActive = 0;	/* 0x81545B84 */
static int s_usbDeviceMode = 0;		/* 0x81545B88 */

/* ---------------------------------------------------------------------------
 * MtpServiceActivationMgr ("ACTIVE", 0x812B6800 - 0x812B69DB)
 * --------------------------------------------------------------------------- */

/* 0x812B6800 */
int MtpServiceActivationMgr::initialize()
{
	int ret = sceKernelCreateLwMutex(&m_mutex, "SceMtpServiceActivationMgr",
					 0x2000u, 0, NULL);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): CreateLwMutex failed: 0x%x\n",
			      "ACTIVE", "Initialize", 24, ret);
	return ret;
}

/* 0x812B6846 */
int MtpServiceActivationMgr::finalize()
{
	int ret = sceKernelDeleteLwMutex(&m_mutex);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): DeleteLwMutex failed: 0x%x\n",
			      "ACTIVE", "Finalize", 33, ret);
	return ret;
}

/* 0x812B687C */
int MtpServiceActivationMgr::activate()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): LockLwMutex failed: 0x%x\n",
			      "ACTIVE", "lock", 68, ret);

	if (m_isActive != 0) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): UnlockLwMutex failed: 0x%x\n",
				      "ACTIVE", "unlock", 77, ret);
		return MTP_SVC_ERROR_ALREADY_ACTIVE;
	}

	m_isActive = 1;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): UnlockLwMutex failed: 0x%x\n",
			      "ACTIVE", "unlock", 77, ret);
	return SceLibMtp_2EBE47B8("ux0:");
}

/* 0x812B692C */
int MtpServiceActivationMgr::deactivate()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): LockLwMutex failed: 0x%x\n",
			      "ACTIVE", "lock", 68, ret);

	if (m_isActive == 0) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): UnlockLwMutex failed: 0x%x\n",
				      "ACTIVE", "unlock", 77, ret);
		return MTP_SVC_ERROR_ALREADY_ACTIVE;
	}

	m_isActive = 0;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): UnlockLwMutex failed: 0x%x\n",
			      "ACTIVE", "unlock", 77, ret);
	return SceLibMtp_A824A205("ux0:");
}

/* ---------------------------------------------------------------------------
 * Storage Jobs ("STRHDL", 0x81431F10 - 0x81432113)
 * --------------------------------------------------------------------------- */

class MtpStorageActivationJob : public paf::job::JobItem {
public:
	MtpStorageActivationJob(MtpServiceActivationMgr *mgr, SceUID eventFlag, bool activate)
		: paf::job::JobItem("SceMtpStorageActivationJob", NULL),
		  m_activationMgr(mgr),
		  m_serviceEventFlag(eventFlag),
		  m_activate(activate ? 1 : 0)
	{
	}

	virtual ~MtpStorageActivationJob() {}

	/* 0x81431F4C */
	virtual int Run()
	{
		int ret = 0;
		if (m_activate != 0) {
			ret = m_activationMgr->activate();
			if (ret < 0 && ret != MTP_SVC_ERROR_ALREADY_ACTIVE)
				sceClibPrintf("[MTP %s] Error %s(%u): Activate failed: 0x%x\n",
					      "STRHDL", "Run", 42, ret);
			return ret;
		}

		MtpStatus hostStatus;
		sceMtpGetHostInfo(&hostStatus);
		if (hostStatus.state == 0 && m_activationMgr->m_isActive != 0) {
			m_activationMgr->m_deactivatePending = 1;
			SceUInt timeout = 900000000u;
			int waitRet = sceKernelWaitEventFlag(
				m_serviceEventFlag, MTP_EVF_CONNECTED,
				SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				NULL, &timeout);
			m_activationMgr->m_deactivatePending = 0;
			if (waitRet == MTP_KERNEL_ERROR_WAIT_TIMEOUT) {
				MtpStatus afterWait;
				sceMtpGetHostInfo(&afterWait);
				if (afterWait.state == 0) {
					int deactRet = m_activationMgr->deactivate();
					if (deactRet < 0 &&
					    deactRet != MTP_SVC_ERROR_ALREADY_ACTIVE)
						sceClibPrintf(
							"[MTP %s] Error %s(%u): Deactivate failed: 0x%x\n",
							"STRHDL", "Run", 60, deactRet);
				}
			}
		}
		return ret;
	}

	virtual void Finish(int result) { (void)result; }

private:
	MtpServiceActivationMgr *m_activationMgr;
	SceUID m_serviceEventFlag;
	SceUInt8 m_activate;
};

class MtpStorageResetJob : public paf::job::JobItem {
public:
	explicit MtpStorageResetJob(MtpServiceActivationMgr *mgr)
		: paf::job::JobItem("SceMtpStorageResetJob", NULL),
		  m_activationMgr(mgr)
	{
	}

	virtual ~MtpStorageResetJob() {}

	/* 0x81432084 */
	virtual int Run()
	{
		int ret = m_activationMgr->deactivate();
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): Deactivate failed: 0x%x\n",
				      "STRHDL", "Run", 105, ret);
		ret = sceMtpReset();
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): sceMtpReset failed: 0x%x\n",
				      "STRHDL", "Run", 110, ret);
		ret = m_activationMgr->activate();
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): Deactivate failed: 0x%x\n",
				      "STRHDL", "Run", 114, ret);
		return ret;
	}

	virtual void Finish(int result) { (void)result; }

private:
	MtpServiceActivationMgr *m_activationMgr;
};

/* ---------------------------------------------------------------------------
 * USB Jobs ("USBH", 0x81432114 - 0x81432413)
 * --------------------------------------------------------------------------- */

class MtpWatchConnectionJob : public paf::job::JobItem {
public:
	MtpWatchConnectionJob(SceUID eventFlag, MtpEventListener *listener)
		: paf::job::JobItem("SceMtpWatchMtpConnectionJob", NULL),
		  m_serviceEventFlag(eventFlag),
		  m_listener(listener)
	{
	}

	virtual ~MtpWatchConnectionJob() {}

	/* 0x81432150 */
	virtual int Run()
	{
		SceUInt timeout = 15000000u;
		int ret = sceKernelWaitEventFlag(
			m_serviceEventFlag, MTP_EVF_AVAILABLE | MTP_EVF_USB_SWITCH,
			SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			NULL, &timeout);
		if (ret >= 0)
			return 0;
		if (ret != MTP_KERNEL_ERROR_WAIT_TIMEOUT)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelWaitEventFlag failed: 0x%x\n",
				"USBH", "Run", 113, ret);
		MtpStatus hostStatus;
		sceMtpGetHostInfo(&hostStatus);
		if (hostStatus.state == 1 || hostStatus.state == 2)
			return -1;
		return 0;
	}

	/* 0x814321D4 */
	virtual void Finish(int result)
	{
		if (result < 0 && m_listener != NULL)
			m_listener->onMtpConnectionTimeout();
	}

private:
	SceUID m_serviceEventFlag;
	MtpEventListener *m_listener;
};

class MtpSwitchUsbModeJob : public paf::job::JobItem {
public:
	MtpSwitchUsbModeJob(int usbMode, MtpEventListener *listener)
		: paf::job::JobItem("SceSwitchUsbDeviceModeJob", NULL),
		  m_usbMode(usbMode),
		  m_listener(listener)
	{
	}

	virtual ~MtpSwitchUsbModeJob() {}

	/* 0x81432224 */
	virtual int Run()
	{
		if (m_usbMode == MTP_USB_NORMAL_MODE) {
			int stopRet = sceUsbstorVStorStop();
			if (stopRet < 0)
				sceClibPrintf(
					"[MTP %s] Error %s(%u): sceUsbstorVStorStop failed: 0x%x\n",
					"USBH", "Run", 173, stopRet);
			return sceMtpResume();
		}
		if (m_usbMode != MTP_USB_CDROM_MODE)
			return 0;

		int suspendRet = sceMtpSuspend();
		if (suspendRet < 0)
			return suspendRet;

		const char *fwRev = "1.00";
		SceKernelSystemSwVersion swVer;
		sce_paf_memset(&swVer, 0, sizeof(swVer));
		swVer.size = sizeof(swVer);
		int verRet = sceKernelGetSystemSwVersion(&swVer);
		if (verRet >= 0)
			fwRev = swVer.versionString;
		else
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelGetSystemSwVersion failed: 0x%x\n",
				"USBH", "startCdromEmulation", 205, verRet);

		int vstorRet = sceUsbstorVStorSetDeviceInfo("\"PS Vita\" CMA", fwRev);
		if (vstorRet < 0) {
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceUsbstorVStorSetDeviceInfo failed: 0x%x\n",
				"USBH", "startCdromEmulation", 211, vstorRet);
		} else {
			vstorRet = sceUsbstorVStorSetImgFilePath(
				"vs0:vsh/mtpresponder/CMAInstaller.img");
			if (vstorRet < 0) {
				sceClibPrintf(
					"[MTP %s] Error %s(%u): sceUsbstorVStorSetImgFilePath failed: 0x%x\n",
					"USBH", "startCdromEmulation", 216, vstorRet);
			} else {
				vstorRet = sceUsbstorVStorStart(USBSTOR_TYPE_CDROM);
				if (vstorRet < 0)
					sceClibPrintf(
						"[MTP %s] Error %s(%u): sceUsbstorVStorStart failed: 0x%x\n",
						"USBH", "startCdromEmulation", 221, vstorRet);
			}
		}

		if (vstorRet < 0) {
			sceMtpResume();
			return vstorRet;
		}

		int waitRet = -1;
		SceUdcdWaitParam waitParam;
		sce_paf_memset(&waitParam, 0, sizeof(waitParam));
		waitParam.driverName = "USBStor_Driver";
		waitParam.cable = SCE_UDCD_STATUS_CABLE_DISCONNECTED;

		while (s_usbCdromActive != 0) {
			waitRet = sceUdcdWaitState(&waitParam, 500000u);
			if (waitRet != MTP_UDCD_ERROR_WAIT_TIMEOUT) {
				if (waitRet < 0)
					sceClibPrintf(
						"[MTP %s] Error %s(%u): sceUdcdWaitState failed: 0x%x\n",
						"USBH", "waitDisconnection", 237, waitRet);
				break;
			}
		}
		return waitRet;
	}

	/* 0x814323F0 */
	virtual void Finish(int result)
	{
		if (m_usbMode == MTP_USB_CDROM_MODE && result == 0 && m_listener != NULL) {
			s_usbCdromActive = 0;
			m_listener->onMtpDisconnected();
		}
	}

private:
	int m_usbMode;
	MtpEventListener *m_listener;
};

/* ---------------------------------------------------------------------------
 * SyncCall thunks into MtpEventListener (0x812B5170 - 0x812B51C5)
 * --------------------------------------------------------------------------- */

/* 0x812B5170 */
static int notifyMtpBusyOnMainThread(void *userArg)
{
	MtpEventListener *listener = (MtpEventListener *)userArg;
	if (listener != NULL)
		listener->onMtpBusy();
	return 0;
}

/* 0x812B517E */
static int notifyMtpAvailableOnMainThread(void *userArg)
{
	MtpEventListener *listener = (MtpEventListener *)userArg;
	if (listener != NULL)
		listener->onMtpAvailable();
	return 0;
}

/* 0x812B518C */
static int notifyMtpConnectedOnMainThread(void *userArg)
{
	MtpEventListener *listener = (MtpEventListener *)userArg;
	if (listener != NULL)
		listener->onMtpConnected();
	return 0;
}

/* 0x812B519A */
static int notifyMtpDisconnectedOnMainThread(void *userArg)
{
	MtpEventListener *listener = (MtpEventListener *)userArg;
	if (listener != NULL)
		listener->onMtpDisconnected();
	return 0;
}

/* 0x812B51A8 */
static int notifyUsbDetectedOnMainThread(void *userArg)
{
	MtpEventListener *listener = (MtpEventListener *)userArg;
	if (listener != NULL)
		listener->onUsbDetected();
	return 0;
}

/* 0x812B51B6 */
static int notifyCmaDeactivatedOnMainThread(void *userArg)
{
	MtpEventListener *listener = (MtpEventListener *)userArg;
	if (listener != NULL)
		listener->onCmaDeactivated();
	return 0;
}

/* ---------------------------------------------------------------------------
 * MtpServer storage / USB / listener notification methods
 * --------------------------------------------------------------------------- */

/* 0x812B48E0 / 0x812B49CA */
void MtpServer::enqueueStorageResetJob()
{
	paf::common::SharedPtr<paf::job::JobItem> job(new MtpStorageResetJob(&m_activationMgr));
	if (job.get() != NULL)
		enqueueServiceJob(job);
}

/* 0x812B49D2 / 0x812B4AC8 */
void MtpServer::enqueueStorageActivationJob(bool activate)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new MtpStorageActivationJob(&m_activationMgr, m_serviceEventFlag, activate));
	if (job.get() != NULL)
		enqueueServiceJob(job);
}

/* 0x812B4BC0 */
int MtpServer::switchUsbMode(int usbMode)
{
	if (paf::system::Model::GetType() == 1) {
		sceClibPrintf("[MTP %s] Error %s(%u): Dolce is not support usb mode.\n",
			      "USBH", "SwitchUsbMode", 249);
		return MTP_SVC_ERROR_NOT_SUPPORTED;
	}

	if (usbMode == MTP_USB_NORMAL_MODE) {
		s_usbCdromActive = 0;
	} else if (usbMode == MTP_USB_CDROM_MODE) {
		s_usbCdromActive = 1;
	} else {
		sceClibPrintf("[MTP %s] Error %s(%u): Unknown mode: %d\n",
			      "USBH", "SwitchUsbMode", 263, usbMode);
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	}

	s_usbDeviceMode = usbMode;
	if (m_activationMgr.m_deactivatePending != 0) {
		int ret = sceKernelSetEventFlag(m_serviceEventFlag, MTP_EVF_CONNECTED);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
				"USBH", "SwitchUsbMode", 272, ret);
	}

	int ret = sceKernelSetEventFlag(m_serviceEventFlag, MTP_EVF_USB_SWITCH);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
			      "USBH", "SwitchUsbMode", 277, ret);

	paf::common::SharedPtr<paf::job::JobItem> job(
		new MtpSwitchUsbModeJob(usbMode, m_eventListener));
	if (job.get() == NULL)
		return MTP_SVC_ERROR_NO_MEMORY;
	return enqueueServiceJob(job);
}

/* 0x812B4DAC */
bool MtpServer::isUsbCdromMode()
{
	return s_usbCdromActive != 0;
}

/* 0x812B4DB8 */
int MtpServer::getUsbModeState(int *outState)
{
	if (outState == NULL)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;

	*outState = 0;
	if (s_usbDeviceMode == MTP_USB_CDROM_MODE) {
		UdcdUserDeviceState state;
		sce_paf_memset(&state, 0, sizeof(state));
		sceUdcdGetDeviceState(&state);
		if (state.activation == SCE_UDCD_STATUS_ACTIVATED &&
		    (state.cable & SCE_UDCD_STATUS_CABLE_CONNECTED) != 0) {
			if ((state.connection & SCE_UDCD_STATUS_CONNECTION_ESTABLISHED) != 0)
				*outState = 3;
			else
				*outState = 2;
		} else {
			*outState = 1;
		}
	}
	return 0;
}

/* 0x812B4E36 */
bool MtpServer::isDialogSuppressed()
{
	SceRtcTick currentTick;
	sceRtcGetCurrentTick(&currentTick);
	SceUInt64 lastActivityTick = SceLibMtp_38CBDF4A();
	return lastActivityTick != 0 && (currentTick.tick - lastActivityTick) < 8000000ULL;
}

/* 0x812B4E94 */
void MtpServer::setCmaActive(int active)
{
	m_cmaActive = (active != 0) ? 1 : 0;
	if (active == 0)
		paf::thread::SyncCall::main_thread_synccall.Call(
			notifyCmaDeactivatedOnMainThread, m_eventListener);
}

/* 0x812B4EBE */
int MtpServer::isCmaActive() const
{
	return m_cmaActive;
}

/* 0x812B4EC4 */
void MtpServer::enqueueWatchConnectionJob()
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new MtpWatchConnectionJob(m_serviceEventFlag, m_eventListener));
	if (job.get() != NULL)
		enqueueServiceJob(job);
}

/* 0x812B4FB6 */
void MtpServer::notifyMtpAvailable()
{
	int ret = sceKernelSetEventFlag(m_serviceEventFlag, MTP_EVF_AVAILABLE);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
			      "USBH", "notifyMtpAvailable", 370, ret);
	paf::thread::SyncCall::main_thread_synccall.Call(
		notifyMtpAvailableOnMainThread, m_eventListener);
}

/* 0x812B5008 */
void MtpServer::notifyUsbDetection()
{
	SceUInt32 numWaitThreads = 0;
	int ret = sceKernelCancelEventFlag(m_serviceEventFlag, 0, &numWaitThreads);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelCancelEventFlag failed: 0x%x\n",
			      "USBH", "notifyUsbDetection", 347, ret);
	paf::thread::SyncCall::main_thread_synccall.Call(
		notifyUsbDetectedOnMainThread, m_eventListener);
}

/* 0x812B507A */
void MtpServer::notifyMtpBusy()
{
	paf::thread::SyncCall::main_thread_synccall.Call(
		notifyMtpBusyOnMainThread, m_eventListener);
}

/* 0x812B5098 */
void MtpServer::notifyMtpConnection()
{
	if (m_activationMgr.m_deactivatePending != 0) {
		int ret = sceKernelSetEventFlag(m_serviceEventFlag, MTP_EVF_CONNECTED);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
				"USBH", "notifyMtpConnection", 381, ret);
	}
	paf::thread::SyncCall::main_thread_synccall.Call(
		notifyMtpConnectedOnMainThread, m_eventListener);
}

/* 0x812B50F0 */
void MtpServer::notifyMtpDisconnection()
{
	SceUInt32 numWaitThreads = 0;
	int ret = sceKernelCancelEventFlag(m_serviceEventFlag, 0, &numWaitThreads);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelCancelEventFlag failed: 0x%x\n",
			      "USBH", "notifyMtpDisconnection", 392, ret);
	paf::thread::SyncCall::main_thread_synccall.Call(
		notifyMtpDisconnectedOnMainThread, m_eventListener);
}

/* 0x812B5162 */
void MtpServer::notifyHostReady()
{
	if (m_eventListener != NULL)
		m_eventListener->onHostReady();
}
