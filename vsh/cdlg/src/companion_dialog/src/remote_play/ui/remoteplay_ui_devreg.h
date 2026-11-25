/*
 * SceCompanionDialogPlugin: RemotePlay device registration UI & worker thread.
 */
#ifndef COMPANION_REMOTEPLAY_UI_DEVREG_H
#define COMPANION_REMOTEPLAY_UI_DEVREG_H

#include <stdint.h>
#include <scetypes.h>
#include <plugin.h>
#include <widget/core/widget.h>
#include <widget/core/event.h>
#include <thread/thread.h>
#include "../remoteplay_regist.h"
#include "../../furl_stack.h"

typedef void (*RemotePlayRegistCallback)(SceInt32 result, const RemotePlayRecord *record, const uint8_t *mac);

class RemotePlayDeviceRegisterThread : public paf::thread::Thread {
public:
	RemotePlayDeviceRegisterThread(SceUInt32 pin, RemotePlayRegistCallback cb);
	virtual ~RemotePlayDeviceRegisterThread();
	virtual void EntryFunction();

	void RequestAbort();

private:
	SceUInt32 m_pin;
	RemotePlayRegistCallback m_callback;
	RemotePlaySession m_session;
};

class RemotePlayApp {
public:
	RemotePlayApp();
	virtual ~RemotePlayApp();

	virtual SceInt32 OnLoad(paf::Plugin *plugin);
	virtual SceInt32 OnStart(paf::Plugin *plugin);
	virtual SceInt32 OnStop(paf::Plugin *plugin);
	virtual SceInt32 OnUnload(paf::Plugin *plugin);

	void StartDeviceRegister(RemotePlayRegistCallback cb);
	void AbortDeviceRegister();
	void JoinDeviceRegister();
	void SetPin(SceUInt32 pin) { m_pin = pin; }
	SceUInt32 GetPin() const { return m_pin; }

private:
	static void OnMainThreadTick(void *userData);

	paf::Plugin *m_plugin;
	paf::thread::Mutex m_mutex;
	SceUInt32 m_pin;
	RemotePlayDeviceRegisterThread *m_worker;
};

SceInt32 RemotePlayUiStart(paf::Plugin *plugin);
void RemotePlayUiStop(paf::Plugin *plugin);
SceInt32 RemotePlayUiGetPinState();
SceInt32 RemotePlayUiGetRegistState();
void RemotePlayUiInitPinPage(paf::ui::Widget *pageWidget);
void RemotePlayUiInitWaitPage(paf::ui::Widget *pageWidget);
void RemotePlayUiShowErrorDialog(SceInt32 errorCode);
SceInt32 RemotePlayUiSaveRegistration(const char *ps4Mac, const char *ps4RegistKey,
	const char *apSsid, const char *apBssid, const char *apKey, const uint8_t *rpKey);

#endif /* COMPANION_REMOTEPLAY_UI_DEVREG_H */
