/*
 * SceTwLoginDialogPlugin / SceTwitterDialogPlugin: Lx HTTP client wrapper over SceHttp.
 */
#ifndef TW_LX_HTTP_H
#define TW_LX_HTTP_H

#include <stdint.h>
#include <scetypes.h>
#include "tw_lx.h"

/* Single SceHttp connection handle */
class LxHttpConnection : public LxRefCounted {
public:
	LxHttpConnection();
	virtual ~LxHttpConnection();

	SceInt32 m_connId;
	LxString m_host;
	LxString m_scheme;
	uint16_t m_port;
};

/* Connection factory/pool interface */
class LxHttpConnectionPool : public LxRefCounted {
public:
	virtual ~LxHttpConnectionPool() {}
	virtual int Acquire(LxRefPtr<LxHttpConnection> &outConn, SceInt32 tmplId, const char *url);
	virtual void ReleaseConnection(LxRefPtr<LxHttpConnection> &conn) {}
};

/* Parameters for creating LxHttpTemplate */
struct LxHttpTemplateParam {
	LxString userAgent;
	SceInt32 httpVersion;
	uint8_t autoProxy;
	LxRefPtr<LxHttpConnectionPool> pool;
	uint8_t flushCookies;
};

/* Wrapper over sceHttpCreateTemplate */
class LxHttpTemplate : public LxRefCounted {
public:
	explicit LxHttpTemplate(const LxHttpTemplateParam &param);
	virtual ~LxHttpTemplate();

	SceInt32 m_tmplId;
	LxRefPtr<LxHttpConnectionPool> m_pool;
	uint8_t m_flushCookies;
	volatile unsigned int m_activeRequests;
};

/* Global default HTTP template */
void LxHttpInit();
void LxHttpTerm();
LxRefPtr<LxHttpTemplate> LxHttpGetDefaultTemplate();

/* Single HTTP request lifecycle */
class LxHttpRequest {
public:
	explicit LxHttpRequest(const LxRefPtr<LxHttpTemplate> &tmpl);
	~LxHttpRequest();

	bool CanReuseConnection();
	int Close();
	int Open(const char *url, const char *method, uint64_t contentLength,
		 LxAbortCondition *abortCond);
	int AddHeader(const char *name, const char *value);
	int GetStatusCode(int *statusCode);
	int Send(const void *postData, SceSize size);
	int Read(void *buf, SceSize size, SceSize *bytesRead);
	int Abort();

	static int OnRedirect(SceInt32 reqId, SceInt32 statusCode, SceInt32 *method,
			      const char *location, void *userArg);

	LxRefPtr<LxHttpTemplate> m_tmpl;
	LxRefPtr<LxHttpConnection> m_conn;
	LxAbortWatcher *m_abortWatcher;
	SceInt32 m_method;
	SceInt32 m_reqId;
	LxString m_url;
	LxString m_redirectUrl;
	uint8_t m_sent;
};

#endif /* TW_LX_HTTP_H */
