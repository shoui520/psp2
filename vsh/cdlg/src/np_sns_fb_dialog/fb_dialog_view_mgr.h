/*
 * SceNpSnsFacebookDialogPlugin: FbViewBase interface and FbViewManager.
 */
#ifndef NP_SNS_FB_DIALOG_VIEW_MGR_H
#define NP_SNS_FB_DIALOG_VIEW_MGR_H

#include <stdint.h>
#include <scetypes.h>
#include <std/vector>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include "fb_dialog_plugin.h"
#include "fb_dialog_job.h"

/* View IDParam hashes */
#define FB_VIEW_BG_ID			0x9572CA6EU
#define FB_VIEW_LOGIN_ID		0x2039A849U
#define FB_VIEW_CONFIRM_BIND_ID		0x0B876C53U
#define FB_VIEW_INPUT_SHORT_ID		0x36183D29U
#define FB_VIEW_INPUT_LONG_ID		0x8F947E60U
#define FB_VIEW_PERMISSION_ID		0xA07067CBU
#define FB_VIEW_WAIT_ID			0x337FEFEBU
#define FB_VIEW_ERROR_ID		0xB852BBDDU
#define FB_VIEW_CHAT_RESTRICT_ID	0xE964DFEEU
#define FB_VIEW_FEED_CONFIRM_ID		0x30324EE8U
#define FB_VIEW_RETRY_AUTH_ID		0xE6200466U
#define FB_VIEW_RETRY_SERVER_ID		0xE6637120U
#define FB_VIEW_NOTICE_ID		0x2B3F4A6EU

/* Base class for all 13 Facebook dialog views (vtable 0x81010994) */
class FbViewBase : public FbComponent {
public:
	FbViewBase(FbEventBus *bus, paf::Plugin *plugin)
		: m_bus(bus), m_plugin(plugin), m_open(0), m_id("", 0)
	{
	}
	virtual ~FbViewBase();

	virtual paf::IDParam GetId() const;
	virtual int Open();
	virtual int Suspend();
	virtual int Resume();
	virtual int Close();
	virtual int IsOpen() const;
	virtual int IsDismissed() const;
	virtual int IsOverlay() const;
	virtual int SetBgColor(const paf::math::v4 &color);
	virtual int OnBgAction();
	virtual void SetParamInt1(int value);
	virtual void SetParamInt2(int value);
	virtual void SetParamPtr1(const void *ptr);
	virtual void SetParamPtr2(const void *ptr);
	virtual void SetParamPtr3(const void *ptr);
	virtual void SetParamPtr4(const void *ptr);

	FbEventBus *m_bus;
	paf::Plugin *m_plugin;
	int m_open;
	paf::IDParam m_id;
};

/* Manages the view stack (bg + layer 1 + layer 2 overlay) */
class FbViewManager : public FbComponent, public FbEventListener {
public:
	FbViewManager(FbEventBus *bus, paf::Plugin *plugin);
	virtual ~FbViewManager();
	virtual void OnAttach(FbDialogBase *owner);
	virtual void OnDetach();
	virtual int OnEvent(int event, void *arg);

	int AddView(FbViewBase *view);
	void SetAborting();
	int SetBgColor(const paf::math::v4 &color);
	int OnBgAction();
	int CloseTop();
	int CloseAllLayers();
	int CheckTopDismissed();
	int CloseBg();
	int CloseAll();
	int OpenBg();
	int ShowView(const paf::IDParam &id, int p1, int p2, const void *p3, const void *p4,
		     const void *p5, const void *p6);

	int ShowLogin(const void *titleName, const void *account, const void *password);
	int ShowConfirmBind(const void *titleName);
	int ShowInput(const void *account, const void *password);
	int ShowPermission(const void *permList, const void *titleName);
	int ShowWait();
	int ShowError(int error);
	int ShowChatRestrict(int error);
	int ShowFeedConfirm(int postType, int feedData);
	int ShowRetryAuth(int error);
	int ShowRetryServer(int error);
	int ShowNotice(int error);
	int IsWaitOpen();

	FbViewBase *FindView(uint32_t idHash) const;

	int m_aborting;
	FbEventBus *m_bus;
	paf::Plugin *m_plugin;
	uint8_t m_useLongInput;
	uint8_t m_padding[3];
	int m_stackDepth;
	paf::vector<FbViewBase *> m_views;
	int m_reserved;
	paf::IDParam m_layer1Id;
	paf::IDParam m_layer2Id;
};

#endif /* NP_SNS_FB_DIALOG_VIEW_MGR_H */
