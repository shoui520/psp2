/*
 * The job that fetches a file of the EULA (the index, the text, an image) over HTTP into a
 * buffer, on a worker thread of the dialog's job queue. The file's name is ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string>
#include <buffer/buffer.h>
#include <http/http.h>
#include "npeula_dialog_plugin.h"

using namespace paf;

/* the job's HTTP client (microseconds for the resolver, else seconds) */
#define NPEULA_HTTP_RESOLVE_TIMEOUT	1000000
#define NPEULA_HTTP_RESOLVE_RETRY	5
#define NPEULA_HTTP_TIMEOUT		10000000

/* the job's result without a URL (HTTP's Bad Request) */
#define NPEULA_HTTP_NO_URL		400
#define NPEULA_HTTP_OK			200

EulaHttpJob::EulaHttpJob(string url, common::SharedPtr<Buffer> buffer,
			 EulaHttpJobFunction onReceive, EulaHttpJobFunction onFinish,
			 EulaDialog *owner)
	: JobItem("SceNPEulaHttpJob", NULL), m_owner(owner), m_onReceive(onReceive),
	  m_onFinish(onFinish), m_canceled(false), m_check(&m_canceled)
{
	m_buffer = buffer;
	m_url = url;
	m_result = 0;
	m_received = 0;
	m_id = 0;
	m_transaction = NULL;
}

EulaHttpJob::EulaHttpJob(string url, common::SharedPtr<Buffer> buffer,
			 EulaHttpJobFunction onReceive, EulaHttpJobFunction onFinish,
			 EulaDialog *owner, int id)
	: JobItem("SceNPEulaHttpJob", NULL), m_owner(owner), m_onReceive(onReceive),
	  m_onFinish(onFinish), m_canceled(false), m_check(&m_canceled)
{
	m_buffer = buffer;
	m_url = url;
	m_id = id;
	m_result = 0;
	m_received = 0;
	m_transaction = NULL;
}

EulaHttpJob::~EulaHttpJob()
{
	if (m_transaction != NULL) {
		m_transaction->Destroy();
		delete m_transaction;
		m_transaction = NULL;
	}
}

/*
 * The file, into the job's buffer (one of the file's size when the job has none). The result
 * is the HTTP status or the error; the receive function gets each piece as it comes.
 */
int EulaHttpJob::Run()
{
	uint32_t size = 0;
	uint64_t length;
	int ret;

	if (m_url.length() == 0) {
		m_result = NPEULA_HTTP_NO_URL;
		return -1;
	}
	http::Client::Param param;

	param.m_resolve_timeout = NPEULA_HTTP_RESOLVE_TIMEOUT;
	param.m_resolve_retry = NPEULA_HTTP_RESOLVE_RETRY;
	param.m_connect_timeout = NPEULA_HTTP_TIMEOUT;
	param.m_send_timeout = NPEULA_HTTP_TIMEOUT;
	param.m_recv_timeout = NPEULA_HTTP_TIMEOUT;
	http::Client client(param);

	m_transaction = new http::Transaction(client);
	ScePafMisc_A86A5D0E(m_transaction, m_url.c_str(), "GET", &m_check);
	m_transaction->SendRequest(NULL, 0);
	ret = m_transaction->GetStatusCode(m_result);
	if (m_result != NPEULA_HTTP_OK) {
		m_result = ret != 0 ? ret : NPEULA_ERROR_HTTP_STATUS;
		m_received = 0;
		return 0;
	}
	m_result = 0;
	string url;

	m_transaction->GetURL(url);
	length = 0;
	m_transaction->GetContentLength(length);
	if (length == 0) {
		if (m_onReceive != NULL)
			m_onReceive(size, 0, this);
		m_result = 0;
		return 0;
	}
	if (m_buffer.get() == NULL) {
		m_buffer = MallocBuffer::Allocate(length);
		if (m_buffer.get() == NULL) {
			m_result = NPEULA_ERROR_NO_MEMORY;
			m_received = 0;
			return 0;
		}
	}
	m_received = 0;
	while (m_received < length && !IsCanceled()) {
		ret = m_transaction->ReceiveResponse(m_buffer, &size);
		if (ret != 0) {
			m_result = ret;
			if (m_onReceive != NULL)
				m_onReceive(size, ret, this);
			return 0;
		}
		m_received += size;
		if (m_onReceive != NULL)
			m_onReceive(size, 0, this);
	}
	return 0;
}

void EulaHttpJob::Finish(int result)
{
	if (m_onFinish != NULL)
		m_onFinish(m_result, m_received, this);
}

void EulaHttpJob::Cancel()
{
	m_canceled = true;
	JobItem::Cancel();
}
