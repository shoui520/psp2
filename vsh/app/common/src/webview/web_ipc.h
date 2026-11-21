/*
 * The web view's IPC with the WebKit process: the messages it sends and receives, each written
 * into and read from one packet (the names are ours: no binary names them).
 */
#ifndef WEBVIEW_WEB_IPC_H
#define WEBVIEW_WEB_IPC_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <thread/thread.h>

namespace paf {
namespace ui {

class WebView;

/* the message types (names ours) */
enum {
	WEB_IPC_UPDATE_21 = 0x21,
	WEB_IPC_CREATE_PAGE = 0x1002,
	WEB_IPC_CLOSE_PAGE = 0x1003,
	WEB_IPC_LOAD_URL = 0x1013,
	WEB_IPC_GO_BACK = 0x1017,
	WEB_IPC_GO_FORWARD = 0x1018,
	WEB_IPC_RELOAD = 0x1019,
	WEB_IPC_STOP = 0x1020,
	/* the view's settings, sent as a bool (what each sets is not known yet) */
	WEB_IPC_SELECT_REPLY = 0x1030,
	WEB_IPC_SETTING_1031 = 0x1031,
	WEB_IPC_CURSOR_POS = 0x1034,
	WEB_IPC_TAP = 0x1036,
	WEB_IPC_SETTING_1048 = 0x1048,
	WEB_IPC_REQUEST_1050 = 0x1050,
	WEB_IPC_REQUEST_1051 = 0x1051,
	WEB_IPC_IME_TEXT = 0x1052,
	WEB_IPC_SETTING_1053 = 0x1053,
	WEB_IPC_STRING_PAIR = 0x1054,
	WEB_IPC_UPDATE_1056 = 0x1056,
	WEB_IPC_SCROLL_Y = 0x1060,
	WEB_IPC_REQUEST_1070 = 0x1070,
	WEB_IPC_REQUEST_DATA = 0x1071,
	WEB_IPC_LOAD_DATA = 0x1080,
	/* received: a zoom the view drops when it cannot zoom */
	WEB_IPC_EVENT_2003 = 0x2003,
	WEB_IPC_CREATE_PAGE_WITH_VIEW = 0x2100
};

/* the packet a message is written into: the view it is for, its type and the type's fields */
struct WebIpcPacket {
	int m_reserved;			/* 0: only cleared */
	int m_viewId;
	unsigned int m_type;
	char m_text[3][2048];
	float m_float[4];
	int m_int[4];
	SceUInt8 m_reserved2[12];
	bool m_bool[2];
	int m_dataSize;
	char m_data[2048];
};

/* a message (0x810248b8): its type, and its fields written into a packet and read back */
class WebIpcMessage {
public:
	WebIpcMessage(int type) : m_type(type)
	{
	}

	virtual ~WebIpcMessage()
	{
	}

	virtual void Write(WebIpcPacket *packet) const
	{
		packet->m_type = m_type;
	}

	virtual void Read(const WebIpcPacket *packet)
	{
		m_type = packet->m_type;
	}

	int m_type;
};

/*
 * The messages of one value, each read from its packet by its own constructor: an int
 * (0x810252c8), two ints (0x810252e0), a float (0x810252f8), a bool (0x81025310) and a string
 * (0x81025328). Most messages are one of these under their own type.
 */
class WebIpcIntMessage : public WebIpcMessage {
public:
	WebIpcIntMessage(int type, int value) : WebIpcMessage(type), m_value(value)
	{
	}

	WebIpcIntMessage(const WebIpcPacket *packet);

	virtual ~WebIpcIntMessage()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	int m_value;
};

class WebIpcIntPairMessage : public WebIpcMessage {
public:
	WebIpcIntPairMessage(int type, int first, int second) :
		WebIpcMessage(type), m_first(first), m_second(second)
	{
	}

	WebIpcIntPairMessage(const WebIpcPacket *packet);

	virtual ~WebIpcIntPairMessage()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	int m_first;
	int m_second;
};

class WebIpcFloatMessage : public WebIpcMessage {
public:
	WebIpcFloatMessage(const WebIpcPacket *packet);

	virtual ~WebIpcFloatMessage()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	float m_value;
};

class WebIpcBoolMessage : public WebIpcMessage {
public:
	WebIpcBoolMessage(int type, bool value) : WebIpcMessage(type), m_value(value)
	{
	}

	WebIpcBoolMessage(const WebIpcPacket *packet);

	virtual ~WebIpcBoolMessage()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	bool m_value;
};

class WebIpcStringMessage : public WebIpcMessage {
public:
	WebIpcStringMessage(int type, const string &value) : WebIpcMessage(type), m_value(value)
	{
	}

	WebIpcStringMessage(const WebIpcPacket *packet);

	virtual ~WebIpcStringMessage()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	string m_value;
};

/* two ints, of two kinds read by their own constructors (0x810248d0, 0x810248e8) */
class WebIpcIntPairMessageA : public WebIpcIntPairMessage {
public:
	WebIpcIntPairMessageA(const WebIpcPacket *packet);

	virtual ~WebIpcIntPairMessageA()
	{
	}
};

class WebIpcIntPairMessageB : public WebIpcIntPairMessage {
public:
	WebIpcIntPairMessageB(const WebIpcPacket *packet);

	virtual ~WebIpcIntPairMessageB()
	{
	}
};

/* the page made for the view, its size, the render parameters and whether it is secure
   (0x81025340, type 0x1002) */
class WebIpcCreatePage : public WebIpcMessage {
public:
	WebIpcCreatePage(int width, int height, const void *param, int paramSize, bool secure);
	WebIpcCreatePage(const WebIpcPacket *packet);
	virtual ~WebIpcCreatePage();

	virtual void Write(WebIpcPacket *packet) const;
	virtual void Read(const WebIpcPacket *packet);

	int m_width;
	int m_height;
	void *m_param;
	int m_paramSize;
	bool m_secure;
};

/* the same, for a view of the page made before (0x81025568, type 0x2100), with its URL */
class WebIpcCreatePageWithView : public WebIpcCreatePage {
public:
	WebIpcCreatePageWithView(int width, int height, int viewId, const void *param,
		int paramSize, bool secure);
	WebIpcCreatePageWithView(const WebIpcPacket *packet);
	virtual ~WebIpcCreatePageWithView()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;
	virtual void Read(const WebIpcPacket *packet);

	string m_url;
	bool m_flag;
	int m_viewId;
};

/* one of the view's settings, a bool (0x81024918) */
class WebIpcSetting : public WebIpcBoolMessage {
public:
	WebIpcSetting(int type, bool value);
	virtual ~WebIpcSetting()
	{
	}
};

/* a tap on the page at the point (0x810253D0, type 0x1036; names ours) */
class WebIpcTap : public WebIpcMessage {
public:
	WebIpcTap(int x, int y, int button);
	virtual ~WebIpcTap()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	int m_x;
	int m_y;
	int m_button;
};

/* asks the page for its data, of the size (0x81024930, type 0x1071; names ours) */
class WebIpcRequestData : public WebIpcIntPairMessage {
public:
	WebIpcRequestData(int id, int size);
	virtual ~WebIpcRequestData()
	{
	}
};

/* the item the select popup chose, and whether it closes (0x81025358, type 0x1030; names
   ours) */
class WebIpcSelectReply : public WebIpcMessage {
public:
	WebIpcSelectReply(int index, bool close);
	virtual ~WebIpcSelectReply()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	int m_index;
	bool m_close;
};

/* two strings for the page (0x81025370, type 0x1054; names ours) */
class WebIpcStringPair : public WebIpcMessage {
public:
	WebIpcStringPair(const string &first, const string &second);
	virtual ~WebIpcStringPair()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	string m_first;
	string m_second;
};

/* the IME's text, with its mode (0x81025448, type 0x1052; names ours) */
class WebIpcImeText : public WebIpcMessage {
public:
	WebIpcImeText(int mode, const string &text);
	virtual ~WebIpcImeText()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	int m_mode;
	string m_text;
};

/* the page's vertical scroll (0x81024948, type 0x1060; names ours) */
class WebIpcScrollY : public WebIpcIntMessage {
public:
	WebIpcScrollY(int y);

	virtual ~WebIpcScrollY()
	{
	}
};

/* the cursor's point on the page (0x810249D8, type 0x1034; names ours) */
class WebIpcCursorPos : public WebIpcIntPairMessage {
public:
	WebIpcCursorPos(int x, int y);
	virtual ~WebIpcCursorPos()
	{
	}
};

/* the load the view asks for (0x81024900, type 0x1013) */
class WebIpcLoadUrlRequest : public WebIpcStringMessage {
public:
	WebIpcLoadUrlRequest(const string &url);
	virtual ~WebIpcLoadUrlRequest()
	{
	}
};

/* the page loads the data, as from the URL (0x81025430, type 0x1080) */
class WebIpcLoadData : public WebIpcMessage {
public:
	WebIpcLoadData(const string &data, const string &url, const string &type, bool flag);
	virtual ~WebIpcLoadData()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	string m_data;
	string m_url;
	string m_type;
	bool m_flag;
};

/*
 * The messages the page sends the view, named by their type until what each reports is known
 * (names ours). Those of one value are the value's message under their own vtable.
 */
#define WEB_IPC_EVENT(base, type)					\
	class WebIpcEvent##type : public base {				\
	public:								\
		WebIpcEvent##type(const WebIpcPacket *packet) : base(packet) \
		{							\
		}							\
									\
		virtual ~WebIpcEvent##type()				\
		{							\
		}							\
	}

WEB_IPC_EVENT(WebIpcStringMessage, 2001);	/* 0x81024b40 */
WEB_IPC_EVENT(WebIpcStringMessage, 2004);	/* 0x81024a20 */
WEB_IPC_EVENT(WebIpcStringMessage, 2005);	/* 0x81024a38 */
WEB_IPC_EVENT(WebIpcFloatMessage, 2008);	/* 0x81024a68 */
WEB_IPC_EVENT(WebIpcIntPairMessageB, 2011);	/* 0x81024a98 */
WEB_IPC_EVENT(WebIpcStringMessage, 2013);	/* 0x81024a80 */
WEB_IPC_EVENT(WebIpcIntPairMessage, 2016);	/* 0x81024ab0 */
WEB_IPC_EVENT(WebIpcBoolMessage, 2024);		/* 0x81024ae0 */
WEB_IPC_EVENT(WebIpcIntPairMessageA, 2026);	/* 0x81024af8 */
WEB_IPC_EVENT(WebIpcStringMessage, 2028);	/* 0x81024b28 */
WEB_IPC_EVENT(WebIpcBoolMessage, 2031);		/* 0x81024b10 */
WEB_IPC_EVENT(WebIpcIntMessage, 2032);		/* 0x81024a50 */
WEB_IPC_EVENT(WebIpcIntPairMessage, 2203);	/* 0x81024ac8 */
WEB_IPC_EVENT(WebIpcIntPairMessageA, 2204);	/* 0x81024b58 */
WEB_IPC_EVENT(WebIpcIntMessage, 2205);		/* 0x81024b70 */
WEB_IPC_EVENT(WebIpcStringMessage, 2210);	/* 0x81024b88 */
WEB_IPC_EVENT(WebIpcStringMessage, 2211);	/* 0x81024ba0 */

#undef WEB_IPC_EVENT

/* the received messages of several fields, each read by its own constructor */
class WebIpcEvent2009 : public WebIpcMessage {	/* 0x810254d8 */
public:
	WebIpcEvent2009(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2009()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	int m_int[4];
	string m_text[2];
	bool m_bool;
};

class WebIpcEvent2012 : public WebIpcMessage {	/* 0x810254f0 */
public:
	WebIpcEvent2012(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2012()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	float m_float[3];
	bool m_bool;
};

class WebIpcEvent2014 : public WebIpcStringMessage {	/* 0x810254a8 */
public:
	WebIpcEvent2014(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2014()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;
	virtual void Read(const WebIpcPacket *packet);

	bool m_bool;
};

class WebIpcEvent2015 : public WebIpcMessage {	/* 0x810254c0 */
public:
	WebIpcEvent2015(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2015()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;
	virtual void Read(const WebIpcPacket *packet);

	string m_text[3];
	bool m_bool;
};

class WebIpcEvent2019 : public WebIpcMessage {	/* 0x81025460 */
public:
	WebIpcEvent2019(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2019()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	int m_int[2];
	bool m_bool;
};

class WebIpcEvent2020 : public WebIpcMessage {	/* 0x81025478 */
public:
	WebIpcEvent2020(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2020()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	string m_text;
	bool m_bool[2];
};

class WebIpcEvent2021 : public WebIpcMessage {	/* 0x81025508 */
public:
	WebIpcEvent2021(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2021()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	string m_text[2];
};

class WebIpcEvent2023 : public WebIpcMessage {	/* 0x81025520 */
public:
	WebIpcEvent2023(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2023()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	string m_text[2];
	int m_int;
	bool m_bool[2];
	/* the packet's data, when it is this size */
	SceUInt8 m_data[44];
};

class WebIpcEvent2025 : public WebIpcMessage {	/* 0x81025598 */
public:
	WebIpcEvent2025(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2025()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;
	virtual void Read(const WebIpcPacket *packet);

	string m_text[2];
	/* the third text, split at m_split */
	string m_head;
	string m_tail;
	int m_split;
	bool m_bool;
};

class WebIpcEvent2027 : public WebIpcMessage {	/* 0x81025580 */
public:
	WebIpcEvent2027(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2027()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;
	virtual void Read(const WebIpcPacket *packet);

	string m_text;
	int m_int;
	bool m_bool;
};

class WebIpcEvent2029 : public WebIpcMessage {	/* 0x81025538 */
public:
	WebIpcEvent2029(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2029()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	string m_text;
	int m_int;
};

class WebIpcEvent2030 : public WebIpcMessage {	/* 0x81025550 */
public:
	WebIpcEvent2030(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2030()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	char m_data[2048];
	int m_dataSize;
};

class WebIpcEvent2033 : public WebIpcMessage {	/* 0x810255c8 */
public:
	WebIpcEvent2033(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2033()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;
	virtual void Read(const WebIpcPacket *packet);

	int m_int;
	string m_text[2];
	float m_float[2];
	string m_text3;
	bool m_bool;
};

class WebIpcEvent2034 : public WebIpcIntMessage {	/* 0x810255b0 */
public:
	WebIpcEvent2034(const WebIpcPacket *packet) : WebIpcIntMessage(packet)
	{
	}

	virtual ~WebIpcEvent2034()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;
	virtual void Read(const WebIpcPacket *packet);

	bool m_bool;
};

class WebIpcEvent2035 : public WebIpcMessage {	/* 0x81025490 */
public:
	WebIpcEvent2035(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2035()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	int m_int;
	bool m_bool;
	string m_text[2];
};

class WebIpcEvent2206 : public WebIpcMessage {	/* 0x810255e0 */
public:
	WebIpcEvent2206(const WebIpcPacket *packet);
	virtual ~WebIpcEvent2206()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;

	bool m_bool;
	int m_int[2];
};

/* the IPC with the web UI process, one for every web view: each view's slot is its view ID with
   the process (it holds 16; names ours) */
class WebIpc {
public:
	enum {
		VIEW_NUM = 16
	};

	static WebIpc *s_instance;

	static WebIpc *GetInstance()
	{
		if (s_instance == NULL)
			s_instance = new WebIpc();
		return s_instance;
	}

	WebIpc();
	virtual ~WebIpc();

	/* the view's slot, the next free one after the last given */
	int RegisterView(WebView *view);
	/* the message sent to the view's page, waiting for its reply (0x810100e0), or not
	   (0x810101cc) */
	int Send(const WebIpcMessage *message, int viewId);
	int Post(const WebIpcMessage *message, int viewId);
	/* the view's slot freed */
	void UnregisterView(int viewId);

	WebView *m_views[VIEW_NUM];
	thread::RMutex *m_lock;
	/* the connection's lock, its channel and state (1: connected) */
	thread::RMutex *m_connectionLock;
	int m_lastView;
	void *m_channel;
	int m_connectionState;
	/* the process ID, in hex */
	string m_processId;
};

}
}

#endif
