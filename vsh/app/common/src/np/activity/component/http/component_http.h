/*
 * The HTTP component: the context's connection to the activity server and the one request
 * running on it (Sony's file: component_http.cpp's checks; the class and method names are ours).
 */
#ifndef NP_ACTIVITY_COMPONENT_HTTP_COMPONENT_HTTP_H
#define NP_ACTIVITY_COMPONENT_HTTP_COMPONENT_HTTP_H

#include <scetypes.h>
#include "../abstract_component.h"

/* SendRequest's method */
#define HTTP_METHOD_GET		0
#define HTTP_METHOD_POST	1

class ComponentHttp : public Component {
public:
	ComponentHttp();
	virtual ~ComponentHttp();

	virtual void SetAbort();
	virtual int SetBuffer(void *buffer, SceSize size);
	virtual int GetProtocol();
	virtual int GetSecureProtocol()
	{
		return m_secureProtocol;
	}
	virtual void SetSecureProtocol(int protocol)
	{
		m_secureProtocol = protocol;
	}
	virtual int Open(int protocol, const char *userAgent, const char *host,
		SceUShort16 port, int keepAlive);
	virtual int Close();
	virtual int SendRequest(int method, const char *url, SceSize urlLength,
		const void *postData, SceSize size);
	virtual int ReadData(void **data, SceSize *size, int *done);
	virtual int EndRequest();
	virtual SceUInt64 GetContentLength()
	{
		return m_contentLength;
	}

private:
	int getRequestId();
	void setRequestId(int requestId);

public:
	void *m_buffer;			/* SetBuffer's: the response is read into it */
	SceSize m_bufferSize;
	int m_protocol;			/* Open's: UTILITY_PROTOCOL_*, NONE when closed */
	int m_secureProtocol;		/* the context's OpenConnection mode */
	int m_templateId;
	int m_connectionId;
	int m_requestId;
	SceUInt64 m_contentLength;
};

#endif
