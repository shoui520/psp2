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
	WEB_IPC_CREATE_PAGE = 0x1002,
	WEB_IPC_CLOSE_PAGE = 0x1003,
	WEB_IPC_CREATE_PAGE_WITH_VIEW = 0x2100
};

/* the packet a message is written into: its view, its type and the type's fields */
struct WebIpcPacket;

/* a message (0x810248b8): its type, and its fields written into a packet and read back */
class WebIpcMessage {
public:
	WebIpcMessage(int type) : m_type(type)
	{
	}

	virtual ~WebIpcMessage()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;
	virtual void Read(const WebIpcPacket *packet);

	int m_type;
};

/* the page made for the view, its size, the render parameters and whether it is secure
   (0x81025340, type 0x1002) */
class WebIpcCreatePage : public WebIpcMessage {
public:
	WebIpcCreatePage(int width, int height, const void *param, int paramSize, bool secure);
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
	virtual ~WebIpcCreatePageWithView()
	{
	}

	virtual void Write(WebIpcPacket *packet) const;
	virtual void Read(const WebIpcPacket *packet);

	string m_url;
	bool m_flag;
	int m_viewId;
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
