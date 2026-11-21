/*
 * The web view's IPC messages: each written into a packet for the WebKit process, and the
 * received ones read from theirs. Sony's file name is not known (the module has no debug
 * records); this one and every name in it are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/threadmgr.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include "web_ipc.h"
#include "web_view.h"

namespace paf {
namespace ui {

/* the string into a packet's text, cut at the text's size */
static inline void CopyText(char *text, const string &src)
{
	if (text != NULL)
		sce_paf_strncpy(text, src.c_str(),
				src.length() < WEB_IPC_TEXT_SIZE ? src.length() : WEB_IPC_TEXT_SIZE);
}

void WebIpcIntMessage::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_value;
}

WebIpcIntMessage::WebIpcIntMessage(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_value(packet->m_int[0])
{
}

void WebIpcIntPairMessage::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_first;
	packet->m_int[1] = m_second;
}

WebIpcIntPairMessage::WebIpcIntPairMessage(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_first(packet->m_int[0]), m_second(packet->m_int[1])
{
}

void WebIpcFloatMessage::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_float[0] = m_value;
}

WebIpcFloatMessage::WebIpcFloatMessage(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_value(packet->m_float[0])
{
}

void WebIpcBoolMessage::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_bool[0] = m_value;
}

WebIpcBoolMessage::WebIpcBoolMessage(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_value(packet->m_bool[0])
{
}

void WebIpcStringMessage::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_value);
}

WebIpcStringMessage::WebIpcStringMessage(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_value(packet->m_text[0])
{
}

WebIpcIntPairMessageA::WebIpcIntPairMessageA(const WebIpcPacket *packet)
	: WebIpcIntPairMessage(packet)
{
}

WebIpcIntPairMessageB::WebIpcIntPairMessageB(const WebIpcPacket *packet)
	: WebIpcIntPairMessage(packet)
{
}

WebIpcCreatePage::WebIpcCreatePage(int width, int height, const void *param, int paramSize,
	bool secure)
	: WebIpcMessage(WEB_IPC_CREATE_PAGE), m_width(width), m_height(height), m_param(NULL),
	  m_paramSize(0), m_secure(secure)
{
	if (param != NULL && paramSize != 0 && paramSize <= WEB_IPC_DATA_SIZE) {
		m_param = new char[paramSize];
		m_paramSize = paramSize;
		sce_paf_memcpy(m_param, param, paramSize);
	}
}

WebIpcCreatePage::WebIpcCreatePage(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_width(packet->m_int[0]), m_height(packet->m_int[1]),
	  m_param(NULL), m_paramSize(0), m_secure(packet->m_bool[0])
{
	int size = packet->m_dataSize;

	if (packet->m_data != NULL && size != 0 && size <= WEB_IPC_DATA_SIZE) {
		m_param = new char[size];
		m_paramSize = size;
		sce_paf_memcpy(m_param, packet->m_data, size);
	}
}

WebIpcCreatePage::~WebIpcCreatePage()
{
	if (m_param != NULL) {
		delete[] (char *)m_param;
		m_param = NULL;
	}
}

void WebIpcCreatePage::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_width;
	packet->m_int[1] = m_height;
	packet->m_bool[0] = m_secure;
	if (m_param != NULL) {
		sce_paf_memcpy(packet->m_data, m_param, m_paramSize);
		packet->m_dataSize = m_paramSize;
	}
}

WebIpcLoadUrlRequest::WebIpcLoadUrlRequest(const string &url)
	: WebIpcStringMessage(WEB_IPC_LOAD_URL, url)
{
}

WebIpcSetting::WebIpcSetting(int type, bool value)
	: WebIpcBoolMessage(type, value)
{
}

WebIpcRequestData::WebIpcRequestData(int id, int size)
	: WebIpcIntPairMessage(WEB_IPC_REQUEST_DATA, id, size)
{
}

WebIpcSelectReply::WebIpcSelectReply(int index, bool close)
	: WebIpcMessage(WEB_IPC_SELECT_REPLY), m_index(index), m_close(close)
{
}

void WebIpcSelectReply::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_index;
	packet->m_bool[0] = m_close;
}

WebIpcStringPair::WebIpcStringPair(const string &first, const string &second)
	: WebIpcMessage(WEB_IPC_STRING_PAIR), m_first(first), m_second(second)
{
}

void WebIpcStringPair::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_first);
	CopyText(packet->m_text[1], m_second);
}

WebIpcScrollY::WebIpcScrollY(int y)
	: WebIpcIntMessage(WEB_IPC_SCROLL_Y, y)
{
}

WebIpcImePreedit::WebIpcImePreedit(const string &text, int caret)
	: WebIpcMessage(WEB_IPC_IME_PREEDIT), m_text(text), m_caret(caret)
{
}

void WebIpcImePreedit::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_caret;
	CopyText(packet->m_text[0], m_text);
}

WebIpcImeCommit::WebIpcImeCommit(const string &text, int caret)
	: WebIpcMessage(WEB_IPC_IME_COMMIT), m_text(text), m_caret(caret)
{
}

void WebIpcImeCommit::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_caret;
	CopyText(packet->m_text[0], m_text);
}

WebIpcImeSetText::WebIpcImeSetText(const string &text)
	: WebIpcStringMessage(WEB_IPC_IME_COMMIT, text)
{
}

WebIpcString1023::WebIpcString1023(const string &value)
	: WebIpcStringMessage(WEB_IPC_STRING_1023, value)
{
}

WebIpcPointerPos::WebIpcPointerPos(int x, int y)
	: WebIpcIntPairMessage(WEB_IPC_POINTER_POS, x, y)
{
}

WebIpcCursorPress::WebIpcCursorPress(int x, int y)
	: WebIpcIntPairMessage(WEB_IPC_CURSOR_PRESS, x, y)
{
}

WebIpcCursorRelease::WebIpcCursorRelease(int x, int y)
	: WebIpcIntPairMessage(WEB_IPC_CURSOR_RELEASE, x, y)
{
}

WebIpcCursorPos::WebIpcCursorPos(int x, int y)
	: WebIpcIntPairMessage(WEB_IPC_CURSOR_POS, x, y)
{
}

WebIpcTouch::WebIpcTouch(int x, int y, int action, int port)
	: WebIpcMessage(WEB_IPC_TOUCH), m_x(x), m_y(y), m_action(action), m_port(port)
{
}

void WebIpcTouch::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_x;
	packet->m_int[1] = m_y;
	packet->m_int[2] = m_action;
	packet->m_int[3] = m_port;
}

WebIpcTap::WebIpcTap(int x, int y, int button)
	: WebIpcMessage(WEB_IPC_TAP), m_x(x), m_y(y), m_button(button)
{
}

void WebIpcTap::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_x;
	packet->m_int[1] = m_y;
	packet->m_int[2] = m_button;
}

void WebIpcKeyDown::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_keycode;
	packet->m_bool[0] = m_ime;
}

WebIpcKeyDown::WebIpcKeyDown(int keycode, bool ime)
	: WebIpcMessage(WEB_IPC_KEY_DOWN), m_keycode(keycode), m_ime(ime)
{
}

WebIpcKeyUp::WebIpcKeyUp(int keycode, bool ime)
	: WebIpcKeyDown(keycode, ime)
{
	m_type = WEB_IPC_KEY_UP;
}

WebIpcChosenFile::WebIpcChosenFile(const string &path, const string &name)
	: WebIpcMessage(WEB_IPC_CHOSEN_FILE), m_path(path), m_name(name)
{
}

void WebIpcChosenFile::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_path);
	CopyText(packet->m_text[1], m_name);
}

WebIpcViewport::WebIpcViewport(int x, int y, int width, int height, bool tapping)
	: WebIpcMessage(WEB_IPC_VIEWPORT), m_x(x), m_y(y), m_width(width), m_height(height),
	  m_tapping(tapping)
{
}

void WebIpcViewport::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_x;
	packet->m_int[1] = m_y;
	packet->m_int[2] = m_width;
	packet->m_int[3] = m_height;
	packet->m_bool[0] = m_tapping;
}

WebIpcRenderRect::WebIpcRenderRect(int x, int y, int width, int height, const void *data,
	int size)
	: WebIpcMessage(WEB_IPC_RENDER_RECT), m_x(x), m_y(y), m_width(width), m_height(height),
	  m_data(NULL)
{
	int n = size < WEB_IPC_DATA_SIZE ? size : WEB_IPC_DATA_SIZE;

	m_size = n;
	if (data != NULL && n != 0) {
		m_data = new char[n];
		sce_paf_memcpy(m_data, data, n);
	}
}

WebIpcRenderRect::~WebIpcRenderRect()
{
	if (m_data != NULL)
		delete[] (char *)m_data;
}

void WebIpcRenderRect::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_x;
	packet->m_int[1] = m_y;
	packet->m_int[2] = m_width;
	packet->m_int[3] = m_height;
	if (m_size != 0 && m_data != NULL) {
		sce_paf_memcpy(packet->m_data, m_data, m_size);
		packet->m_dataSize = m_size;
	}
}

WebIpcLoadData::WebIpcLoadData(const string &data, const string &url, const string &type,
	bool flag)
	: WebIpcMessage(WEB_IPC_LOAD_DATA), m_data(data), m_url(url), m_type(type), m_flag(flag)
{
}

void WebIpcLoadData::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_data);
	CopyText(packet->m_text[1], m_url);
	CopyText(packet->m_text[2], m_type);
	packet->m_bool[0] = m_flag;
}

WebIpcImeText::WebIpcImeText(int mode, const string &text)
	: WebIpcMessage(WEB_IPC_IME_TEXT), m_mode(mode), m_text(text)
{
}

void WebIpcImeText::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_text);
	packet->m_int[0] = m_mode;
}

WebIpcEvent2019::WebIpcEvent2019(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type)
{
	m_int[0] = packet->m_int[0];
	m_int[1] = packet->m_int[1];
	m_bool = packet->m_bool[0];
}

void WebIpcEvent2019::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_int[0];
	packet->m_int[1] = m_int[1];
	packet->m_bool[0] = m_bool;
}

WebIpcEvent2020::WebIpcEvent2020(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_text(packet->m_text[0])
{
	m_bool[0] = packet->m_bool[0];
	m_bool[1] = packet->m_bool[1];
}

void WebIpcEvent2020::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_text);
	packet->m_bool[0] = m_bool[0];
	packet->m_bool[1] = m_bool[1];
}

WebIpcEvent2035::WebIpcEvent2035(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_int(packet->m_int[0]), m_bool(packet->m_bool[0]),
	  m_text0(packet->m_text[0]), m_text1(packet->m_text[1])
{
}

void WebIpcEvent2035::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_int;
	packet->m_bool[0] = m_bool;
	CopyText(packet->m_text[0], m_text0);
	CopyText(packet->m_text[1], m_text1);
}

WebIpcEvent2013::WebIpcEvent2013(const string &text)
	: WebIpcStringMessage(WEB_IPC_EVENT_2013, text)
{
}

WebIpcEvent2014::WebIpcEvent2014(const string &text)
	: WebIpcStringMessage(WEB_IPC_EVENT_2014, text), m_accepted(false)
{
}

WebIpcEvent2014::WebIpcEvent2014(const WebIpcPacket *packet)
	: WebIpcStringMessage(packet), m_accepted(packet->m_bool[0])
{
}

void WebIpcEvent2014::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_value);
	packet->m_bool[0] = m_accepted;
}

void WebIpcEvent2014::Read(const WebIpcPacket *packet)
{
	m_accepted = packet->m_bool[0];
}

WebIpcEvent2015::WebIpcEvent2015(const string &text, const string &defaultText,
	const string &result)
	: WebIpcMessage(WEB_IPC_EVENT_2015), m_text(text), m_defaultText(defaultText),
	  m_result(result), m_accepted(false)
{
}

WebIpcEvent2015::WebIpcEvent2015(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_text(packet->m_text[0]),
	  m_defaultText(packet->m_text[1]), m_result(packet->m_text[2]),
	  m_accepted(packet->m_bool[0])
{
}

void WebIpcEvent2015::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_text);
	CopyText(packet->m_text[1], m_defaultText);
	CopyText(packet->m_text[2], m_result);
	packet->m_bool[0] = m_accepted;
}

void WebIpcEvent2015::Read(const WebIpcPacket *packet)
{
	m_result = packet->m_text[2];
	m_accepted = packet->m_bool[0];
}

WebIpcEvent2009::WebIpcEvent2009(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_x(packet->m_int[0]), m_y(packet->m_int[1]),
	  m_width(packet->m_int[2]), m_height(packet->m_int[3]), m_text0(packet->m_text[0]),
	  m_text1(packet->m_text[1]), m_bool(packet->m_bool[0])
{
}

void WebIpcEvent2009::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_x;
	packet->m_int[1] = m_y;
	packet->m_int[2] = m_width;
	packet->m_int[3] = m_height;
	CopyText(packet->m_text[0], m_text0);
	CopyText(packet->m_text[1], m_text1);
	packet->m_bool[0] = m_bool;
}

WebIpcEvent2012::WebIpcEvent2012(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type)
{
	m_float[0] = packet->m_float[0];
	m_float[1] = packet->m_float[1];
	m_float[2] = packet->m_float[2];
	m_bool = packet->m_bool[0];
}

void WebIpcEvent2012::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_float[0] = m_float[0];
	packet->m_float[1] = m_float[1];
	packet->m_float[2] = m_float[2];
	packet->m_bool[0] = m_bool;
}

WebIpcEvent2021::WebIpcEvent2021(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_text0(packet->m_text[0]), m_text1(packet->m_text[1])
{
}

void WebIpcEvent2021::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_text0);
	CopyText(packet->m_text[1], m_text1);
}

WebIpcEvent2023::WebIpcEvent2023(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_linkUrl(packet->m_text[0]),
	  m_imageUrl(packet->m_text[1]), m_action(packet->m_int[0]),
	  m_show(packet->m_bool[0]), m_contextMenuShown(packet->m_bool[1])
{
	m_hit.x = 0;
	m_hit.y = 0;
	m_hit.width = 0;
	m_hit.height = 0;
	m_hit.pointX = 0;
	m_hit.pointY = 0;
	m_hit.imageSize = 0;
	sce_paf_memset(m_hit.mimeType, 0, sizeof(m_hit.mimeType));
	if (packet->m_dataSize == sizeof(m_hit) && packet->m_data != NULL)
		sce_paf_memcpy(&m_hit, packet->m_data, sizeof(m_hit));
}

void WebIpcEvent2023::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_linkUrl);
	CopyText(packet->m_text[1], m_imageUrl);
	packet->m_int[0] = m_action;
	packet->m_bool[0] = m_show;
	packet->m_bool[1] = m_contextMenuShown;
	sce_paf_memcpy(packet->m_data, &m_hit, sizeof(m_hit));
	packet->m_dataSize = sizeof(m_hit);
}

WebIpcEvent2029::WebIpcEvent2029(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_text(packet->m_text[0]), m_int(packet->m_int[0])
{
}

void WebIpcEvent2029::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_text);
	packet->m_int[0] = m_int;
}

WebIpcEvent2030::WebIpcEvent2030(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type)
{
	m_dataSize = packet->m_dataSize < sizeof(m_data) ? packet->m_dataSize : sizeof(m_data);
	if (m_dataSize != 0 && packet->m_data != NULL)
		sce_paf_memcpy(m_data, packet->m_data, m_dataSize);
}

void WebIpcEvent2030::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	if (m_dataSize != 0) {
		sce_paf_memcpy(packet->m_data, m_data, m_dataSize);
		packet->m_dataSize = m_dataSize;
	}
}

WebIpcCreatePageWithView::WebIpcCreatePageWithView(const string &url, int width, int height,
	bool flag)
	: WebIpcCreatePage(width, height, NULL, 0, false), m_url(url), m_flag(flag), m_viewId(-1)
{
	m_type = WEB_IPC_CREATE_PAGE_WITH_VIEW;
}

WebIpcCreatePageWithView::WebIpcCreatePageWithView(int width, int height, int viewId,
	const void *param, int paramSize, bool secure)
	: WebIpcCreatePage(width, height, param, paramSize, secure), m_flag(false),
	  m_viewId(viewId)
{
	m_type = WEB_IPC_CREATE_PAGE_WITH_VIEW;
}

WebIpcCreatePageWithView::WebIpcCreatePageWithView(const WebIpcPacket *packet)
	: WebIpcCreatePage(packet), m_url(packet->m_text[0]), m_flag(packet->m_bool[1]),
	  m_viewId(packet->m_int[2])
{
}

void WebIpcCreatePageWithView::Write(WebIpcPacket *packet) const
{
	WebIpcCreatePage::Write(packet);
	CopyText(packet->m_text[0], m_url);
	packet->m_bool[1] = m_flag;
	packet->m_int[2] = m_viewId;
}

void WebIpcCreatePageWithView::Read(const WebIpcPacket *packet)
{
	int size = packet->m_dataSize;

	if (packet->m_data != NULL && size != 0 && size <= WEB_IPC_DATA_SIZE) {
		if (m_param != NULL) {
			delete[] (char *)m_param;
			m_param = NULL;
		}
		m_param = new char[size];
		m_paramSize = size;
		sce_paf_memcpy(m_param, packet->m_data, size);
	}
	m_viewId = packet->m_int[2];
	m_width = packet->m_int[0];
	m_height = packet->m_int[1];
}

WebIpcEvent2027::WebIpcEvent2027(const string &text, int value)
	: WebIpcMessage(WEB_IPC_EVENT_2027), m_text(text), m_int(value), m_bool(false)
{
}

WebIpcEvent2027::WebIpcEvent2027(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_text(packet->m_text[0]), m_int(packet->m_int[0]),
	  m_bool(packet->m_bool[0])
{
}

void WebIpcEvent2027::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_text);
	packet->m_int[0] = m_int;
	packet->m_bool[0] = m_bool;
}

void WebIpcEvent2027::Read(const WebIpcPacket *packet)
{
	m_bool = packet->m_bool[0];
}

WebIpcEvent2025::WebIpcEvent2025(const string &text0, const string &text1, const string &user,
	const string &password)
	: WebIpcMessage(WEB_IPC_EVENT_2025), m_text0(text0), m_text1(text1), m_user(user),
	  m_password(password), m_userLength(user.length())
{
}

WebIpcEvent2025::WebIpcEvent2025(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_text0(packet->m_text[0]), m_text1(packet->m_text[1]),
	  m_userLength(packet->m_int[0]), m_accepted(packet->m_bool[0])
{
	string credentials(packet->m_text[2]);

	m_user = credentials.substr(0, m_userLength);
	m_password = credentials.substr(m_userLength, credentials.length());
}

void WebIpcEvent2025::SetCredentials(bool accepted, const string &user, const string &password)
{
	m_accepted = accepted;
	m_user = user;
	m_password = password;
	m_userLength = user.length();
}

void WebIpcEvent2025::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	CopyText(packet->m_text[0], m_text0);
	CopyText(packet->m_text[1], m_text1);

	string credentials = m_user + m_password;

	CopyText(packet->m_text[2], credentials);
	packet->m_int[0] = m_userLength;
	packet->m_bool[0] = m_accepted;
}

void WebIpcEvent2025::Read(const WebIpcPacket *packet)
{
	string credentials(packet->m_text[2]);

	m_userLength = packet->m_int[0];
	m_user = credentials.substr(0, m_userLength);
	m_password = credentials.substr(m_userLength, credentials.length());
	m_accepted = packet->m_bool[0];
}

WebIpcEvent2034::WebIpcEvent2034(int kind)
	: WebIpcIntMessage(WEB_IPC_EVENT_2034, kind), m_accepted(false)
{
}

void WebIpcEvent2034::Read(const WebIpcPacket *packet)
{
	m_accepted = packet->m_bool[0];
}

void WebIpcEvent2034::Write(WebIpcPacket *packet) const
{
	WebIpcIntMessage::Write(packet);
	packet->m_bool[0] = m_accepted;
}

WebIpcEvent2033::WebIpcEvent2033(int inputType, const string &title, const string &initialText,
	float minDate, float maxDate, const string &defaultText)
	: WebIpcMessage(WEB_IPC_EVENT_2033), m_inputType(inputType), m_title(title),
	  m_initialText(initialText), m_minDate(minDate), m_maxDate(maxDate),
	  m_defaultText(defaultText), m_accepted(false)
{
}

WebIpcEvent2033::WebIpcEvent2033(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_inputType(packet->m_int[0]),
	  m_title(packet->m_text[0]), m_initialText(packet->m_text[1]),
	  m_minDate(packet->m_float[0]), m_maxDate(packet->m_float[1]),
	  m_defaultText(packet->m_text[2]), m_accepted(packet->m_bool[0])
{
}

void WebIpcEvent2033::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_int[0] = m_inputType;
	CopyText(packet->m_text[0], m_title);
	CopyText(packet->m_text[1], m_initialText);
	packet->m_float[0] = m_minDate;
	packet->m_float[1] = m_maxDate;
	CopyText(packet->m_text[2], m_defaultText);
	packet->m_bool[0] = m_accepted;
}

void WebIpcEvent2033::Read(const WebIpcPacket *packet)
{
	m_accepted = packet->m_bool[0];
}

WebIpcEvent2206::WebIpcEvent2206(const WebIpcPacket *packet)
	: WebIpcMessage(packet->m_type), m_bool(packet->m_bool[0])
{
	m_int[0] = packet->m_int[0];
	m_int[1] = packet->m_int[1];
}

void WebIpcEvent2206::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_bool[0] = m_bool;
	packet->m_int[0] = m_int[0];
	packet->m_int[1] = m_int[1];
}

WebIpcGeolocationPermission::WebIpcGeolocationPermission(const string &origin, bool allowed)
	: WebIpcMessage(WEB_IPC_GEOLOCATION_PERMISSION), m_origin(origin), m_allowed(allowed)
{
}

void WebIpcGeolocationPermission::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	packet->m_bool[0] = m_allowed != 0;
	CopyText(packet->m_text[0], m_origin);
}

WebIpcGeolocationPositionMessage::WebIpcGeolocationPositionMessage(
	const WebIpcGeolocationPosition &position)
	: WebIpcMessage(WEB_IPC_GEOLOCATION_POSITION), m_position(position)
{
}

void WebIpcGeolocationPositionMessage::Write(WebIpcPacket *packet) const
{
	WebIpcMessage::Write(packet);
	sce_paf_memcpy(packet->m_data, &m_position, sizeof(m_position));
	packet->m_dataSize = sizeof(m_position);
}

int WebIpc::RegisterView(WebView *view)
{
	thread::RMutexLock lock(m_lock);
	int next = ++m_lastView;
	int slot = next % VIEW_NUM;

	for (int i = 0; i < VIEW_NUM; i++) {
		if (m_views[slot] == NULL)
			break;
		slot = ++next % VIEW_NUM;
	}
	m_lastView = slot;
	m_views[slot] = view;
	return slot;
}

void WebIpc::UnregisterView(int viewId)
{
	thread::RMutexLock lock(m_lock);

	if (viewId >= 0 && viewId < VIEW_NUM && m_views[viewId] != NULL)
		m_views[viewId] = NULL;
}

int WebIpc::Send(const WebIpcMessage *message, int viewId)
{
	int ret = WEB_VIEW_ERROR_FAILED;

	if (GetConnectionState() == 1) {
		WebIpcPacket packet;

		message->Write(&packet);
		packet.m_viewId = viewId;
		ret = m_channel->Send(&packet);
		if (ret != 0)
			sce_paf_printf_dummy();
	} else {
		sce_paf_printf_dummy();
	}
	return ret;
}

int WebIpc::Post(const WebIpcMessage *message, int viewId)
{
	int ret = 0;

	if (GetConnectionState() == 1) {
		WebIpcPacket packet;

		message->Write(&packet);
		packet.m_viewId = viewId;
		ret = m_channel->Post(&packet);
	}
	return ret;
}

WebIpc::WebIpc()
	: m_lock(NULL), m_connectionLock(NULL), m_lastView(-1), m_channel(NULL),
	  m_connectionState(0)
{
	char processId[12];

	sce_paf_memset(m_views, 0, sizeof(m_views));
	sce_paf_snprintf(processId, 9, "%08x", sceKernelGetProcessId());
	m_processId = processId;

	string name = "SceWebUIProcess" + m_processId;
	const char *channelName = name.c_str();

	m_channel = channelName != NULL ? new WebIpcChannel(channelName, true, this) : NULL;
	m_lock = new thread::RMutex("SceWebProcessProxyVLMutex", 0);
	m_connectionLock = new thread::RMutex("SceWebProcessProxySTMutex", 0);
}

WebIpc::~WebIpc()
{
	if (m_lock != NULL) {
		delete m_lock;
		m_lock = NULL;
	}
	if (m_connectionLock != NULL) {
		delete m_connectionLock;
		m_connectionLock = NULL;
	}
	if (m_channel != NULL) {
		if (GetConnectionState() == 1)
			m_channel->Close();
		delete m_channel;
		m_channel = NULL;
	}
}

int WebIpc::Connect()
{
	int ret = 0;

	if (m_channel != NULL)
		ret = SCE_ERROR_ERRNO_EINVAL;
	if (GetConnectionState() != 1 && m_channel != NULL) {
		string name = "SceWebKitProcess" + m_processId;

		ret = m_channel->Open(name.c_str());
		if (ret == 0) {
			thread::RMutexLock lock(m_connectionLock);

			m_connectionState = 1;
		}
	}
	return ret;
}

void WebIpc::OnConnect()
{
	Connect();

	thread::RMutexLock lock(m_lock);

	for (int i = 0; i < VIEW_NUM; i++) {
		if (m_views[i] != NULL)
			m_views[i]->QueueMessage21();
	}
}

void WebIpc::OnDisconnect()
{
	m_channel->Close();
	{
		thread::RMutexLock lock(m_connectionLock);

		m_connectionState = 0;
	}

	thread::RMutexLock lock(m_lock);

	for (int i = 0; i < VIEW_NUM; i++) {
		if (m_views[i] != NULL)
			m_views[i]->QueueMessage1056();
	}
}

void WebIpc::OnPost(WebIpcPacket *packet)
{
	thread::RMutexLock lock(m_lock);

	if (packet->m_type == WEB_IPC_PROCESS_EXIT) {
		thread::RMutexLock views(m_lock);

		for (int i = 0; i < VIEW_NUM; i++) {
			if (m_views[i] != NULL)
				m_views[i]->OnProcessExit();
		}
	} else {
		WebView *view = GetView(packet->m_viewId);

		if (view != NULL)
			view->ReceiveMessage(packet);
	}
}

void WebIpc::OnRequest(WebIpcPacket *packet)
{
	thread::RMutexLock lock(m_lock);
	int viewId = packet->m_viewId;

	if (viewId >= 0 && viewId < VIEW_NUM && m_views[viewId] != NULL)
		GetView(viewId)->ReceiveMessage(packet);
	else
		m_channel->Post(packet);
}

WebIpc *WebIpc::s_instance = NULL;

}
}
