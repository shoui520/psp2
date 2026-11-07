/*
 * SceCrossCtlDialogPlugin (vs0:sys/external/libcdlg_cross_controller.suprx): the cross controller
 * dialog's plugin, which SceCommonDialogMain loads for libcdlg's sceCrossControllerDialog*. It
 * shows SceCommonGuiDialog's waiting dialog and broadcasts the caller's title and version to the
 * PS3 title on the local network until one answers, then closes with the PS3's address, or shows
 * what has to be updated first. The file's name and every name here are ours: nothing names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <appmgr.h>
#include <net.h>
#include <cross_controller_dialog.h>
#include <plugin.h>
#include <module/module.h>
#include <timer/timer.h>
#include <common/main_thread_call_list.h>
#include <common/char_encoding.h>
#include <versatileparam/idparam.h>
#include <std/string.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

/* the UDP port the PS3 title listens on */
#define CROSS_CONTROLLER_DIALOG_PORT		9294
/* a datagram: its magic, then records, as much as fits */
#define CROSS_CONTROLLER_DIALOG_PACKET_SIZE	1500
/* the request's magic, then the reply's */
#define CROSS_CONTROLLER_DIALOG_MAGIC		"SRC4FIN4"
#define CROSS_CONTROLLER_DIALOG_MAGIC_SIZE	4
#define CROSS_CONTROLLER_DIALOG_REQUEST_MAGIC	CROSS_CONTROLLER_DIALOG_MAGIC
#define CROSS_CONTROLLER_DIALOG_REPLY_MAGIC	(CROSS_CONTROLLER_DIALOG_MAGIC + CROSS_CONTROLLER_DIALOG_MAGIC_SIZE)
/* how often the waiting dialog asks again, in milliseconds */
#define CROSS_CONTROLLER_DIALOG_POLL_INTERVAL	1000.0f
/* how long a receive waits, in microseconds */
#define CROSS_CONTROLLER_DIALOG_RECV_TIMEOUT	500000

/*
 * A record's tag (big-endian, before its big-endian length): the protocol version (1), the
 * titles and their versions, and the PS3's name. A reply has to carry the tags in
 * CROSS_CONTROLLER_DIALOG_REPLY_TAGS.
 */
#define CROSS_CONTROLLER_DIALOG_TAG_VERSION		0
#define CROSS_CONTROLLER_DIALOG_TAG_TITLE_ID		6
#define CROSS_CONTROLLER_DIALOG_TAG_APP_VERSION		7
#define CROSS_CONTROLLER_DIALOG_TAG_PS3_TITLE_ID	8
#define CROSS_CONTROLLER_DIALOG_TAG_PS3_APP_VERSION	9
#define CROSS_CONTROLLER_DIALOG_TAG_PEER_NAME		10
#define CROSS_CONTROLLER_DIALOG_TAG_TITLE_ID_REQUEST	11
#define CROSS_CONTROLLER_DIALOG_TAG_APP_VERSION_REQUEST	12
#define CROSS_CONTROLLER_DIALOG_REPLY_TAGS		0x7c1
#define CROSS_CONTROLLER_DIALOG_RECORD_HEADER_SIZE	4
/* a title ID's length in a record */
#define CROSS_CONTROLLER_DIALOG_TITLE_ID_LEN		9

/* the application parameters the plugin reads */
#define CROSS_CONTROLLER_DIALOG_APP_PARAM_VERSION	3
#define CROSS_CONTROLLER_DIALOG_APP_PARAM_TITLE_ID	12

/* the dialog's result when the user cancelled, and when the caller aborted */
#define CROSS_CONTROLLER_DIALOG_RESULT_CANCELLED	1
#define CROSS_CONTROLLER_DIALOG_RESULT_ABORTED		2

/* the waiting dialog's buttons: cancel, and the update dialog's update */
#define CROSS_CONTROLLER_DIALOG_BUTTON_CANCEL		3
#define CROSS_CONTROLLER_DIALOG_BUTTON_UPDATE		4

/* the resources the plugin shows: its page, the waiting dialog's text and the error messages */
#define CROSS_CONTROLLER_DIALOG_PAGE_ID			0x5032be3f
#define CROSS_CONTROLLER_DIALOG_MSG_SEARCHING		0x632b6733
#define CROSS_CONTROLLER_DIALOG_MSG_NEED_UPDATE_VITA_SYS	0x6578516f
#define CROSS_CONTROLLER_DIALOG_MSG_NEED_UPDATE_VITA_APP	0x3f09c4dc
#define CROSS_CONTROLLER_DIALOG_MSG_NEED_UPDATE_PS3_SYS	0x103d21bc
#define CROSS_CONTROLLER_DIALOG_MSG_NEED_UPDATE_PS3_APP	0x87c5726c
#define CROSS_CONTROLLER_DIALOG_MSG_ERROR		0xc51a8da4

#define CROSS_CONTROLLER_DIALOG_UPDATER_URI		"settings_dlg:updater"
#define CROSS_CONTROLLER_DIALOG_LAUNCH_FLAGS		0x60000

/* the system update dialog's buttons and their labels */
static const CommonGuiDialogButton s_updateButtons[] = {
	{ 12, 0x6c2b7e72, 4, 0x20413274 },
	{ 13, 0xadcd9adf, 5, 0xc17d7980 },
	{ 0, 0, 0, 0 },
};

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

static Plugin::ModuleInterface s_moduleInterface;

/* the socket's options */
static const int s_on = 1;
static const int s_recvTimeout = CROSS_CONTROLLER_DIALOG_RECV_TIMEOUT;

/* a dialog's handler that calls a function with its argument */
class CrossControllerDialogHandler : public CommonGuiDialogHandler {
public:
	CrossControllerDialogHandler(CommonGuiDialogFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button);

	CommonGuiDialogFunction m_func;
	void *m_arg;
};

static void crossControllerDialogInit(SceSize argSize, const void *argp);
static void crossControllerDialogTerm(void);
static void crossControllerDialogPluginInit(Plugin *plugin);
static void crossControllerDialogPluginStart(Plugin *plugin);
static void crossControllerDialogPluginStop(Plugin *plugin);
static void crossControllerDialogPluginExit(Plugin *plugin);
static void crossControllerDialogSetParam(Plugin::InitParam2 *param);
static int crossControllerDialogShowError(CommonDialogCrossControllerParam *work, SceInt32 error);
static void crossControllerDialogOpen(CommonDialogCrossControllerParam *work, Plugin *plugin);
static void crossControllerDialogStart(Plugin *plugin);
static void crossControllerDialogStop(Plugin *plugin);
static void crossControllerDialogUpdate(void *arg);
static int crossControllerDialogPoll(int value, void *arg);
static void crossControllerDialogButton(SceInt32 value, SceInt32 button, void *arg);
static int crossControllerDialogOpenSocket(CommonDialogCrossControllerParam *work);
static int crossControllerDialogCloseSocket(CommonDialogCrossControllerParam *work);
static SceUInt8 *crossControllerDialogBuildRequest(CommonDialogCrossControllerParam *work, SceUInt8 *buf);
static int crossControllerDialogSendRequest(CommonDialogCrossControllerParam *work);
static int crossControllerDialogParseVersion(const char *str, SceSize len);
static int crossControllerDialogParseReply(CommonDialogCrossControllerParam *work,
					   const SceNetSockaddrIn *from, const SceUInt8 *data, int len);
static int crossControllerDialogReceiveReply(CommonDialogCrossControllerParam *work);

extern "C" int module_start(SceSize argSize, const void *argp)
{
	crossControllerDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	crossControllerDialogTerm();
	return 0;
}

/* the static objects' constructors, and the plugin's functions for SceCommonDialogMain's
   paf::Module */
static __attribute__((noinline)) void crossControllerDialogInit(SceSize argSize, const void *argp)
{
	void (**init)(void);

	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	s_moduleInterface.init_func = crossControllerDialogPluginInit;
	s_moduleInterface.start_func = crossControllerDialogPluginStart;
	s_moduleInterface.stop_func = crossControllerDialogPluginStop;
	s_moduleInterface.exit_func = crossControllerDialogPluginExit;
	s_moduleInterface.set_param_func = crossControllerDialogSetParam;
	(*(Module *const *)argp)->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void crossControllerDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

static void crossControllerDialogPluginInit(Plugin *plugin)
{
}

static void crossControllerDialogPluginStart(Plugin *plugin)
{
	crossControllerDialogStart(plugin);
}

static void crossControllerDialogPluginStop(Plugin *plugin)
{
	crossControllerDialogStop(plugin);
}

static void crossControllerDialogPluginExit(Plugin *plugin)
{
}

static void crossControllerDialogSetParam(Plugin::InitParam2 *param)
{
}

/* what has to be updated, or that the connection failed: the dialog's message, or the system
   update dialog */
static int crossControllerDialogShowError(CommonDialogCrossControllerParam *work, SceInt32 error)
{
	CommonGuiMessageDialogParam param;

	param.type = error;
	param.resourcePlugin = work->plugin;
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.0f;
	switch (error) {
	case SCE_CROSS_CONTROLLER_DIALOG_ERROR_NEED_UPDATE_VITA_SYS: {
		CommonGuiLayoutDialogParam update;

		update.resourcePlugin = work->plugin;
		update.dimmerColor[0] = 0.0f;
		update.dimmerColor[1] = 0.0f;
		update.dimmerColor[2] = 0.0f;
		update.dimmerColor[3] = 0.0f;
		update.layout = SceCommonGuiDialog_22B17F2B;
		update.layout.buttons = s_updateButtons;
		update.text = work->plugin->GetString(IDParam("", CROSS_CONTROLLER_DIALOG_MSG_NEED_UPDATE_VITA_SYS));
		update.handler = new CrossControllerDialogHandler(crossControllerDialogButton, work);
		return SceCommonGuiDialog_CCFCD9A6(&update);
	}
	case SCE_CROSS_CONTROLLER_DIALOG_ERROR_NEED_UPDATE_VITA_APP:
		param.messageId = CROSS_CONTROLLER_DIALOG_MSG_NEED_UPDATE_VITA_APP;
		break;
	case SCE_CROSS_CONTROLLER_DIALOG_ERROR_NEED_UPDATE_PS3_SYS:
		param.messageId = CROSS_CONTROLLER_DIALOG_MSG_NEED_UPDATE_PS3_SYS;
		break;
	case SCE_CROSS_CONTROLLER_DIALOG_ERROR_NEED_UPDATE_PS3_APP: {
		/* the message and the PS3's name under it */
		wstring text(work->plugin->GetString(IDParam("", CROSS_CONTROLLER_DIALOG_MSG_NEED_UPDATE_PS3_APP)));

		{
			string name(work->peerName);
			wstring peerName;

			common::Utf8ToUtf16(name, &peerName, 0);
			text.append(L"\n\n" + peerName);
		}
		param.title = text;
		break;
	}
	default:
		param.messageId = CROSS_CONTROLLER_DIALOG_MSG_ERROR;
		break;
	}
	param.handler = new CrossControllerDialogHandler(crossControllerDialogButton, work);
	return SceCommonGuiDialog_20B141F1(&param);
}

/* opens the socket and the waiting dialog, which asks the network every second */
static __attribute__((noinline)) void crossControllerDialogOpen(CommonDialogCrossControllerParam *work, Plugin *plugin)
{
	bool opened;

	work->plugin = plugin;
	crossControllerDialogOpenSocket(work);
	Plugin::PageOpenParam openParam;

	plugin->PageOpen(IDParam("", CROSS_CONTROLLER_DIALOG_PAGE_ID), openParam);
	{
		IDParam searching("", CROSS_CONTROLLER_DIALOG_MSG_SEARCHING);
		const CommonGuiDialogLayout *layout = work->cancelable ? &SceCommonGuiDialog_F8B2D7E1 :
									 &SceCommonGuiDialog_2A68B4A4;

		opened = (work->dialogId = SceCommonGuiDialog_6CE14F66(work->plugin, wstring(),
			wstring(work->plugin->GetString(searching)), layout, crossControllerDialogButton, work)) >= 0;
	}
	if (opened)
		SceCommonGuiDialog_F1A40838(work->dialogId, crossControllerDialogPoll, work,
					    CROSS_CONTROLLER_DIALOG_POLL_INTERVAL);
	common::MainThreadCallList::Register(crossControllerDialogUpdate, work);
	if (openParam.timer != NULL)
		delete openParam.timer;
}

static __attribute__((noinline)) void crossControllerDialogStart(Plugin *plugin)
{
	CommonDialogCrossControllerParam *work;

	work = (CommonDialogCrossControllerParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_CROSS_CONTROLLER);
	crossControllerDialogOpen(work, plugin);
}

static __attribute__((noinline)) void crossControllerDialogStop(Plugin *plugin)
{
	CommonDialogCrossControllerParam *work;

	work = (CommonDialogCrossControllerParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_CROSS_CONTROLLER);
	common::MainThreadCallList::Unregister(crossControllerDialogUpdate, work);
	plugin->PageCloseAll();
	crossControllerDialogCloseSocket(work);
}

/* on the main thread: the caller's abort closes the dialog */
static void crossControllerDialogUpdate(void *arg)
{
	CommonDialogCrossControllerParam *work = (CommonDialogCrossControllerParam *)arg;

	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_CROSS_CONTROLLER)) {
		work->result = CROSS_CONTROLLER_DIALOG_RESULT_ABORTED;
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_CROSS_CONTROLLER);
	}
}

/* every second while the waiting dialog is open: asks again and reads an answer; an answer, or
   an error other than none yet, closes it */
static int crossControllerDialogPoll(int value, void *arg)
{
	CommonDialogCrossControllerParam *work = (CommonDialogCrossControllerParam *)arg;
	int ret;

	ret = crossControllerDialogSendRequest(work);
	if (ret != SCE_OK)
		return ret;
	ret = crossControllerDialogReceiveReply(work);
	if (ret == SCE_NET_ERROR_EAGAIN || ret == (int)SCE_CROSS_CONTROLLER_DIALOG_ERROR_MISMATCH_APP)
		return ret;
	work->result = ret;
	if (ret >= 0)
		SceCommonGuiDialog_01702008(work->dialogId, 0);
	else
		crossControllerDialogShowError(work, ret);
	SceCommonGuiDialog_BC311DC0(work->dialogId);
	work->dialogId = -1;
	return -1;
}

/* a button of the dialogs: cancel, or the system update; either closes the dialog */
static void crossControllerDialogButton(SceInt32 value, SceInt32 button, void *arg)
{
	CommonDialogCrossControllerParam *work = (CommonDialogCrossControllerParam *)arg;

	if (button == CROSS_CONTROLLER_DIALOG_BUTTON_CANCEL)
		work->result = CROSS_CONTROLLER_DIALOG_RESULT_CANCELLED;
	else if (button == CROSS_CONTROLLER_DIALOG_BUTTON_UPDATE)
		sceAppMgrLaunchAppByUri(CROSS_CONTROLLER_DIALOG_LAUNCH_FLAGS, CROSS_CONTROLLER_DIALOG_UPDATER_URI);
	SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_CROSS_CONTROLLER);
}

void CrossControllerDialogHandler::operator()(SceInt32 value, SceInt32 button)
{
	if (m_func != NULL)
		m_func(value, button, m_arg);
}

/* the caller's version and title ID, and the non-blocking broadcast socket */
static __attribute__((noinline)) int crossControllerDialogOpenSocket(CommonDialogCrossControllerParam *work)
{
	int ret;

	ret = sceAppMgrAppParamGetInt(0, CROSS_CONTROLLER_DIALOG_APP_PARAM_VERSION, &work->appVersion);
	if (ret < 0)
		return ret;
	ret = sceAppMgrAppParamGetString(0, CROSS_CONTROLLER_DIALOG_APP_PARAM_TITLE_ID, work->titleId,
					 sizeof(work->titleId));
	if (ret < 0)
		return ret;
	ret = sceNetSocket("PAF_socket_t", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	work->socketId = ret;
	if (ret < 0)
		return ret;
	ret = sceNetSetsockopt(work->socketId, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &s_on, sizeof(s_on));
	if (ret < 0)
		return ret;
	ret = sceNetSetsockopt(work->socketId, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &s_on, sizeof(s_on));
	if (ret < 0)
		return ret;
	ret = sceNetSetsockopt(work->socketId, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &s_recvTimeout,
			       sizeof(s_recvTimeout));
	if (ret < 0)
		return ret;
	return SCE_OK;
}

static __attribute__((noinline)) int crossControllerDialogCloseSocket(CommonDialogCrossControllerParam *work)
{
	int ret;

	ret = sceNetSocketClose(work->socketId);
	if (ret >= 0)
		ret = SCE_OK;
	return ret;
}

/* a record: its tag and length, big-endian, then the value; the next record */
static inline SceUInt8 *crossControllerDialogPutRecord(SceUInt8 *p, int tag, const void *value, int len)
{
	p[0] = tag >> 8;
	p[1] = tag;
	p[2] = len >> 8;
	p[3] = len;
	sceClibMemcpy_safe(p + CROSS_CONTROLLER_DIALOG_RECORD_HEADER_SIZE, value, len);
	return p + CROSS_CONTROLLER_DIALOG_RECORD_HEADER_SIZE + len;
}

/* the request: the protocol version, the PS3 title and its version the caller needs, and the
   caller's own title ID and version; the end of the request */
static __attribute__((noinline)) SceUInt8 *crossControllerDialogBuildRequest(CommonDialogCrossControllerParam *work, SceUInt8 *buf)
{
	char version[6];

	sceClibSnprintf(version, sizeof(version), "%d.%02d", work->appVersion / 100, work->appVersion % 100);
	buf[0] = CROSS_CONTROLLER_DIALOG_TAG_VERSION >> 8;
	buf[1] = CROSS_CONTROLLER_DIALOG_TAG_VERSION;
	buf[2] = 0;
	buf[3] = 2;
	buf[4] = 0;
	buf[5] = 1;
	buf += 6;
	buf = crossControllerDialogPutRecord(buf, CROSS_CONTROLLER_DIALOG_TAG_PS3_TITLE_ID, work->ps3TitleId,
					     sceClibStrnlen(work->ps3TitleId, sizeof(work->ps3TitleId)));
	buf = crossControllerDialogPutRecord(buf, CROSS_CONTROLLER_DIALOG_TAG_PS3_APP_VERSION, work->ps3AppMinVer,
					     sceClibStrnlen(work->ps3AppMinVer, sizeof(work->ps3AppMinVer)));
	buf = crossControllerDialogPutRecord(buf, CROSS_CONTROLLER_DIALOG_TAG_TITLE_ID_REQUEST, work->titleId,
					     sceClibStrnlen(work->titleId, sizeof(work->titleId)));
	buf = crossControllerDialogPutRecord(buf, CROSS_CONTROLLER_DIALOG_TAG_APP_VERSION_REQUEST, version,
					     sceClibStrnlen(version, sizeof(version)));
	return buf;
}

/* broadcasts the request */
static __attribute__((noinline)) int crossControllerDialogSendRequest(CommonDialogCrossControllerParam *work)
{
	SceNetSockaddrIn addr;
	SceUInt8 buf[CROSS_CONTROLLER_DIALOG_PACKET_SIZE];
	SceUInt8 *end;
	int ret;

	sceClibMemset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_port = sceNetHtons(CROSS_CONTROLLER_DIALOG_PORT);
	addr.sin_addr.s_addr = SCE_NET_INADDR_BROADCAST;
	sceClibMemcpy_safe(buf, CROSS_CONTROLLER_DIALOG_REQUEST_MAGIC, CROSS_CONTROLLER_DIALOG_MAGIC_SIZE);
	end = crossControllerDialogBuildRequest(work, buf + CROSS_CONTROLLER_DIALOG_MAGIC_SIZE);
	ret = sceNetSendto(work->socketId, buf, end - buf, SCE_NET_MSG_DONTWAIT, (const SceNetSockaddr *)&addr,
			   sizeof(addr));
	if (ret >= 0)
		ret = SCE_OK;
	return ret;
}

/* a digit's value, 0 for anything else */
#define CROSS_CONTROLLER_DIALOG_DIGIT(c)	((sceClibLookCtypeTable(c) & 4) ? (c) - '0' : 0)

/* a version, "X.YZ" or "XY.ZW", as 100 * major + minor; 0 for anything else */
static int crossControllerDialogParseVersion(const char *str, SceSize len)
{
	if (str[1] == '.' && len == 4)
		return 100 * CROSS_CONTROLLER_DIALOG_DIGIT(str[0]) + 10 * CROSS_CONTROLLER_DIALOG_DIGIT(str[2]) +
		       CROSS_CONTROLLER_DIALOG_DIGIT(str[3]);
	if (str[2] == '.' && len == 5)
		return 1000 * CROSS_CONTROLLER_DIALOG_DIGIT(str[0]) + 100 * CROSS_CONTROLLER_DIALOG_DIGIT(str[1]) +
		       10 * CROSS_CONTROLLER_DIALOG_DIGIT(str[3]) + CROSS_CONTROLLER_DIALOG_DIGIT(str[4]);
	return 0;
}

/* an answer's records: whether the PS3 runs the caller's PS3 title, and whether either side has
   to be updated; the PS3's address and name once it does */
static __attribute__((noinline)) int crossControllerDialogParseReply(CommonDialogCrossControllerParam *work,
					   const SceNetSockaddrIn *from, const SceUInt8 *data, int len)
{
	SceUInt32 seen = 0;
	int error = 0;

	while (len >= CROSS_CONTROLLER_DIALOG_RECORD_HEADER_SIZE) {
		SceUInt32 tag = data[0] << 8 | data[1];
		int size = data[2] << 8 | data[3];
		const SceUInt8 *value = data + CROSS_CONTROLLER_DIALOG_RECORD_HEADER_SIZE;
		int valueLen = len;
		int ret = 0;

		if (valueLen > size)
			valueLen = size;
		switch (tag) {
		case CROSS_CONTROLLER_DIALOG_TAG_VERSION:
			if (valueLen < sizeof(SceUInt16))
				ret = SCE_NET_ERROR_EAGAIN;
			else if (value[0] != 0)
				ret = SCE_CROSS_CONTROLLER_DIALOG_ERROR_NEED_UPDATE_VITA_SYS;
			break;
		case CROSS_CONTROLLER_DIALOG_TAG_TITLE_ID:
			if (valueLen != CROSS_CONTROLLER_DIALOG_TITLE_ID_LEN)
				ret = SCE_NET_ERROR_EAGAIN;
			else if (sceClibMemcmp(value, work->titleId, CROSS_CONTROLLER_DIALOG_TITLE_ID_LEN) != 0)
				ret = SCE_CROSS_CONTROLLER_DIALOG_ERROR_MISMATCH_APP;
			break;
		case CROSS_CONTROLLER_DIALOG_TAG_APP_VERSION:
			if (work->appVersion < crossControllerDialogParseVersion((const char *)value, valueLen))
				ret = SCE_CROSS_CONTROLLER_DIALOG_ERROR_NEED_UPDATE_VITA_APP;
			break;
		case CROSS_CONTROLLER_DIALOG_TAG_PS3_TITLE_ID:
			if (valueLen != CROSS_CONTROLLER_DIALOG_TITLE_ID_LEN)
				ret = SCE_NET_ERROR_EAGAIN;
			else if (sceClibMemcmp(value, work->ps3TitleId, CROSS_CONTROLLER_DIALOG_TITLE_ID_LEN) != 0)
				ret = SCE_CROSS_CONTROLLER_DIALOG_ERROR_MISMATCH_APP;
			break;
		case CROSS_CONTROLLER_DIALOG_TAG_PS3_APP_VERSION:
			if (crossControllerDialogParseVersion(work->ps3AppMinVer,
							      sceClibStrnlen(work->ps3AppMinVer, sizeof(work->ps3AppMinVer))) >
			    crossControllerDialogParseVersion((const char *)value, valueLen))
				ret = SCE_CROSS_CONTROLLER_DIALOG_ERROR_NEED_UPDATE_PS3_APP;
			break;
		case CROSS_CONTROLLER_DIALOG_TAG_PEER_NAME:
			sceClibStrncpy(work->peerName, (const char *)value,
				       valueLen < (int)sizeof(work->peerName) - 1 ? valueLen : (int)sizeof(work->peerName) - 1);
			break;
		}
		if (ret < 0)
			error = ret;
		if (tag < 32)
			seen |= 1 << tag;
		len -= size + CROSS_CONTROLLER_DIALOG_RECORD_HEADER_SIZE;
		data += size + CROSS_CONTROLLER_DIALOG_RECORD_HEADER_SIZE;
	}
	if (len != 0 || (seen & CROSS_CONTROLLER_DIALOG_REPLY_TAGS) != CROSS_CONTROLLER_DIALOG_REPLY_TAGS)
		return SCE_NET_ERROR_EAGAIN;
	if (error < 0)
		return error;
	work->inaddr = from->sin_addr;
	return SCE_OK;
}

/* reads an answer, if one came */
static __attribute__((noinline)) int crossControllerDialogReceiveReply(CommonDialogCrossControllerParam *work)
{
	SceNetSocklen_t fromLen;
	SceNetSockaddrIn from;
	SceUInt8 buf[CROSS_CONTROLLER_DIALOG_PACKET_SIZE];
	int ret;

	fromLen = sizeof(from);
	ret = sceNetRecvfrom(work->socketId, buf, sizeof(buf), SCE_NET_MSG_DONTWAIT, (SceNetSockaddr *)&from,
			     &fromLen);
	if (ret >= 0) {
		if (ret - CROSS_CONTROLLER_DIALOG_MAGIC_SIZE < 0 ||
		    sceClibMemcmp(buf, CROSS_CONTROLLER_DIALOG_REPLY_MAGIC, CROSS_CONTROLLER_DIALOG_MAGIC_SIZE) != 0)
			ret = SCE_NET_ERROR_EAGAIN;
		else
			ret = crossControllerDialogParseReply(work, &from, buf + CROSS_CONTROLLER_DIALOG_MAGIC_SIZE,
							      ret - CROSS_CONTROLLER_DIALOG_MAGIC_SIZE);
	}
	return ret;
}
