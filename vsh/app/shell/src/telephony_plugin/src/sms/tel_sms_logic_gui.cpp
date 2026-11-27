/*
 * tel_sms_logic_gui.cpp: SMS notification dispatcher (LSDB new-event popup +
 * power wake) and Class-0 Flash SMS dialog presenter
 * (3.65-CEX: 0x81251198..0x812517EB, 0x8142BA80..0x8142BADC).
 */
#include <std/new>
#include <std/string.h>
#include <scetypes.h>
#include <plugin.h>
#include <widget/w_text.h>
#include <common/encoded_string.h>
#include <kernel/processmgr.h>
#include <processmgr/processmgr.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "plugins.h"
#include "shell_util_server.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "../tel_internal.h"

namespace telephony {

/* Template and widget ID hashes for the Class-0 Flash SMS dialog (0x8142BADC) */
enum TelFlashSmsResourceHash {
	TEL_FLASH_TEMPLATE_HASH = 0x4ABD2C1F,		/* 1253937887 */
	TEL_FLASH_HEADER_TEXT_HASH = 0x81597442,
	TEL_FLASH_HEADER_MSG_HASH = 0xF2583831,		/* -229096847 */
	TEL_FLASH_SENDER_TEXT_HASH = 0x24347701,	/* 607402529 */
	TEL_FLASH_TIME_TEXT_HASH = 0xC1E23566,		/* -1042135962 */
	TEL_FLASH_BODY_TEXT_HASH = 0x5E2B617D		/* 1579958397 */
};

/* Static LSDB notification key initialized for SMS alerts (0x812517B2) */
static LsdbNewEventKey s_smsNotificationKey;

static void telInitSmsNotificationKey(void)
{
	if (s_smsNotificationKey.titleId.empty()) {
		s_smsNotificationKey.titleId = "telephony";
		s_smsNotificationKey.itemId = "telephony";
		s_smsNotificationKey.rowid = 0;
	}
}

/* No-op button handler for Class-0 Flash SMS dialog */
class TelFlashSmsDialogHandler : public CommonGuiDialogHandler {
public:
	virtual ~TelFlashSmsDialogHandler() {}
	virtual void operator()(SceInt32 dialogId, SceInt32 buttonId)
	{
		(void)dialogId;
		(void)buttonId;
	}
};

/* Open handler that inflates the Flash SMS template and fills sender/time/body (0x8142BADC) */
class TelFlashSmsDialogOpenHandler : public CommonGuiLayoutDialogOpenHandler {
public:
	TelFlashSmsDialogOpenHandler(paf::Plugin *plugin, const TelSmsMessage &message)
		: m_plugin(plugin), m_message(message)
	{
	}
	virtual ~TelFlashSmsDialogOpenHandler() {}

	virtual void operator()(SceInt32 dialogId)
	{
		paf::ui::Widget *textWidget = SceCommonGuiDialog_8947FC23(dialogId, 1);
		if (textWidget == NULL)
			return;

		paf::ui::Widget *parentWidget = textWidget->m_parent;
		textWidget->DestroyWidget();

		paf::IDParam templateId(TEL_FLASH_TEMPLATE_HASH);
		paf::Plugin::TemplateOpenParam openParam;
		if (m_plugin->TemplateOpen(parentWidget, templateId, openParam) != 0)
			return;

		paf::IDParam headerWidgetId(TEL_FLASH_HEADER_TEXT_HASH);
		paf::ui::Widget *headerWidget = parentWidget->FindChild(headerWidgetId, 0);
		if (headerWidget != NULL && headerWidget->IsInherit(paf::ui::Text::TypeName())) {
			paf::IDParam headerStrId(TEL_FLASH_HEADER_MSG_HASH);
			const wchar_t *headerStr = m_plugin->GetString(headerStrId);
			paf::wstring wideHeader(headerStr != NULL ? headerStr : L"");
			headerWidget->SetString(wideHeader);
		}

		paf::IDParam senderWidgetId(TEL_FLASH_SENDER_TEXT_HASH);
		paf::ui::Widget *senderWidget = parentWidget->FindChild(senderWidgetId, 0);
		if (senderWidget != NULL && senderWidget->IsInherit(paf::ui::Text::TypeName())) {
			const TelPhoneNumber &peerNum =
				m_message.GetFirstPeer().GetAddress().GetNumber();
			paf::common::EncodedString encoded;
			ScePafMisc_C11CB5E4(&encoded, peerNum.GetDigits(),
					    paf::common::EncodedString::Encoding_Utf8);
			const wchar_t *wideNum = ScePafMisc_53948F03(&encoded);
			paf::wstring senderText(wideNum != NULL ? wideNum : L"");
			senderWidget->SetString(senderText);
			ScePafMisc_B45A7521(&encoded);
		}

		paf::IDParam timeWidgetId(TEL_FLASH_TIME_TEXT_HASH);
		paf::ui::Widget *timeWidget = parentWidget->FindChild(timeWidgetId, 0);
		if (timeWidget != NULL && timeWidget->IsInherit(paf::ui::Text::TypeName())) {
			TelDateTime deliverTime = m_message.GetDeliverTime();
			deliverTime.ConvertUtcToLocal();
			paf::wstring formattedTime = deliverTime.FormatLocaleString();
			timeWidget->SetString(formattedTime);
		}

		paf::IDParam bodyWidgetId(TEL_FLASH_BODY_TEXT_HASH);
		paf::ui::Widget *bodyWidget = parentWidget->FindChild(bodyWidgetId, 0);
		if (bodyWidget != NULL && bodyWidget->IsInherit(paf::ui::Text::TypeName()))
			bodyWidget->SetString(m_message.GetBody());
	}

private:
	paf::Plugin *m_plugin;
	TelSmsMessage m_message;
};

/* 0x81251198 */
TelSmsLogicGui::TelSmsLogicGui(TelSmsLogicMediatorBase *smsMediator,
			       TelSmsDbManager *dbManager)
	: m_smsMediator(smsMediator), m_dbManager(dbManager)
{
}

/* 0x812511AC */
TelSmsLogicGui::~TelSmsLogicGui()
{
}

/* 0x812511D0 */
TelephonyResult TelSmsLogicGui::Attach()
{
	m_smsMediator->AddObserver(this);
	return TelephonyResult(TEL_OK);
}

/* 0x812511E4 */
TelephonyResult TelSmsLogicGui::Detach()
{
	m_smsMediator->RemoveObserver(this);
	return TelephonyResult(TEL_OK);
}

/* 0x812511F8 */
TelephonyResult TelSmsLogicGui::PostSmsNotification(const paf::wstring &messageText)
{
	TelephonyResult res(TEL_OK);
	if (paf::Plugin::Find("telephony_plugin") == NULL) {
		res.SetCode(TEL_ERR_NOT_FOUND);
		return res;
	}

	telInitSmsNotificationKey();

	LsdbNewEvent notice;
	notice.titleId = s_smsNotificationKey.titleId;
	notice.itemId = s_smsNotificationKey.itemId;
	notice.rowid = s_smsNotificationKey.rowid;
	notice.newFlag = true;

	paf::common::EncodedString encoded;
	ScePafMisc_7709D1B2(&encoded, messageText,
			    paf::common::EncodedString::Encoding_Utf16);
	const char *utf8Desc = ScePafMisc_A7C6AF5C(&encoded);
	notice.desc = (utf8Desc != NULL) ? utf8Desc : "";
	ScePafMisc_B45A7521(&encoded);

	notice.execTitleId = "NPXS10016";
	notice.execArg = "telephony";
	notice.iconPath = "vs0:app/NPXS10015/sce_sys/icon0.png";
	notice.actType = 2;
	notice.notifyFlags = 16;

	int lsdbErr = SceLsdb_315B9FD6(&notice, true);
	if (lsdbErr != 0) {
		res.SetCode(lsdbErr);
		return res;
	}

	paf::Plugin *powerPlugin = paf::Plugin::Find("power_manage_plugin");
	if (powerPlugin != NULL) {
		ShellPluginInterface *iface =
			static_cast<ShellPluginInterface *>(powerPlugin->GetInterface(1));
		if (iface != NULL) {
			sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
			/* Interface slot 13 (+0x34): wake display on incoming notification */
			iface->func[13]();
		}
	}
	return res;
}

/* 0x812514B6 */
void TelSmsLogicGui::OpenFlashSmsDialog(const TelEventInfo &event)
{
	const TelSmsDbMessageEvent &msgEvent =
		static_cast<const TelSmsDbMessageEvent &>(event);
	const TelSmsMessage &message = msgEvent.GetMessage();

	CommonGuiLayoutDialogParam dialogParam;
	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	dialogParam.resourcePlugin = telPlugin;
	dialogParam.dimmer = 1;

	void *handlerMem = telMalloc(sizeof(TelFlashSmsDialogHandler));
	dialogParam.handler = (handlerMem != NULL)
		? new (handlerMem) TelFlashSmsDialogHandler() : NULL;

	void *openMem = telMalloc(sizeof(TelFlashSmsDialogOpenHandler));
	dialogParam.openHandler = (openMem != NULL)
		? new (openMem) TelFlashSmsDialogOpenHandler(telPlugin, message) : NULL;

	dialogParam.layout = SceCommonGuiDialog_AF75CA6D;
	shellStateWatcherOpenDialog(&dialogParam, true, 0);
}

/* 0x812516A0 */
void TelSmsLogicGui::HandleMessageStatusUpdated(const TelEventInfo &event)
{
	const TelSmsDbMessageIdEvent &idEvent =
		static_cast<const TelSmsDbMessageIdEvent &>(event);
	TelSmsMessageId msgId(idEvent.GetMessageId());
	TelSmsMessage message;

	TelephonyResult res = m_dbManager->GetMessageById(msgId, &message);
	if (res.IsSuccess()) {
		/* Status 7 = received / unread SMS ready for user notification */
		if (message.GetFirstPeer().GetStatus() == 7) {
			paf::wstring displayText = message.FormatDisplayBody();
			PostSmsNotification(displayText);
		}
	}
}

/* 0x81251778 */
void TelSmsLogicGui::OnNotify(const TelEventInfo &event)
{
	int eventId = event.GetEventId();
	if (eventId == TEL_SMS_DB_EVENT_MESSAGE_RECEIVED)
		OpenFlashSmsDialog(event);
	else if (eventId == TEL_SMS_EVENT_STATUS_UPDATED)
		HandleMessageStatusUpdated(event);
}

} /* namespace telephony */
