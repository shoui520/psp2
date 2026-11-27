/*
 * DTCPIP-SW transfer engine (SwTransfer): connects SdiBufferCXD3705,
 * SdiDeviceCXD3705 stream decryption, and periodic SwSinkCkcThread verification.
 */

#include "sw_engine.h"

#include <string.h>

/* ---- SwMwTransfer base interface definitions ---- */

void SwMwTransfer::destroy() {}
void SwMwTransfer::dispose() {}
int SwMwTransfer::setEvent(struct SwMwStreamEvent *) { return 1; }
int SwMwTransfer::start() { return 1; }
int SwMwTransfer::reset() { return 1; }
int SwMwTransfer::isReady() { return 0; }
int SwMwTransfer::getState() { return 0; }
int SwMwTransfer::setParameter(int) { return 1; }
int SwMwTransfer::attach(int) { return 1; }
int SwMwTransfer::transfer(int) { return 1; }
int SwMwTransfer::write(int, int) { return 1; }
int SwMwTransfer::getChunkList(void **, int *) { return 1; }
int SwMwTransfer::release(int) { return 1; }
int SwMwTransfer::setSessionData(const void *) { return 0; }
int SwMwTransfer::getSessionData(int *) { return 1; }
int SwMwTransfer::isKeyExpiring() { return 0; }
int SwMwTransfer::startMove(int) { return 1; }

/* ---- SwTransfer ---- */

SwTransfer::SwTransfer(void *extMem, uint32_t bufferSize)
	: m_buffer(NULL), m_extMem(extMem), m_bufferSize(bufferSize),
	  m_ckcThread(NULL), m_hasIv(0), m_isMove(0), m_moveLabel(0),
	  m_rxStarted(0), m_aborted(0), m_sessionId(0), m_streamId(0),
	  m_mediaFmt(0), m_lastError(0), m_timeoutMs(5000), m_event(NULL),
	  m_streamParams(NULL), m_peerAddr()
{
	memset(m_iv, 0, sizeof(m_iv));
	m_reservedF[0] = 0;
	m_reservedF[1] = 0;
	m_reservedF[2] = 0;
}

SwTransfer::~SwTransfer()
{
	destroy();
}

void SwTransfer::destroy()
{
	if (m_buffer != NULL) {
		SdiDevice *dev = NULL;
		if (m_rxStarted && SdiDevice::getInstance(&dev) == 0 && dev != NULL) {
			dev->stopRxStream((int)m_sessionId, (int)m_streamId, m_streamParams);
			m_rxStarted = 0;
		}
		delete m_buffer;
		m_buffer = NULL;
	}
	if (m_ckcThread != NULL) {
		m_ckcThread->stopCkc();
		delete m_ckcThread;
		m_ckcThread = NULL;
	}
}

void SwTransfer::dispose()
{
	delete this;
}

int SwTransfer::setup(uint32_t sessionId, int mediaFmt, struct SwMwStreamEvent *event,
		      void *workOrParams, uint8_t isMove, uint8_t moveLabel)
{
	m_sessionId = sessionId;
	m_mediaFmt = mediaFmt;
	m_event = event;
	m_streamParams = static_cast<SdiStreamParams *>(workOrParams);
	m_isMove = isMove;
	m_moveLabel = moveLabel;
	return 0;
}

int SwTransfer::setEvent(struct SwMwStreamEvent *event)
{
	m_event = event;
	return 0;
}

int SwTransfer::prepareCkc(uint8_t *outNeedCkc, uint8_t *outSessionNeedCkc)
{
	if (outNeedCkc != NULL)
		*outNeedCkc = 0;
	if (outSessionNeedCkc != NULL)
		*outSessionNeedCkc = 0;

	struct SwMwContentSupport *supportBase = NULL;
	if (SwMwContentSupportCreate(&supportBase) != 0 || supportBase == NULL)
		return 1;
	SwContentSupport *support = static_cast<SwContentSupport *>(supportBase);
	SwContent *content = support->content();
	if (content == NULL)
		return 1;

	uint8_t sessionNeed = 0;
	int err = 0;
	if (m_isMove)
		err = content->checkMoveCkc(m_sessionId, m_moveLabel, &m_peerAddr, &sessionNeed);
	else
		err = content->checkSinkCkc(m_sessionId, &m_peerAddr, &sessionNeed);

	if (err == 0 && sessionNeed != 0) {
		if (outNeedCkc != NULL)
			*outNeedCkc = 1;
		if (outSessionNeedCkc != NULL)
			*outSessionNeedCkc = 1;
		if (m_ckcThread != NULL) {
			delete m_ckcThread;
			m_ckcThread = NULL;
		}
		m_ckcThread = new (std::nothrow) SwSinkCkcThread(
			m_sessionId, m_peerAddr, m_isMove, m_moveLabel);
		if (m_ckcThread == NULL)
			return 2;
		err = m_ckcThread->init();
		if (err == 0)
			m_ckcThread->start();
	}
	return err;
}

int SwTransfer::propagateBufferError()
{
	if (m_buffer == NULL)
		return 0;
	if (m_lastError != 0)
		return m_lastError;
	int err = m_buffer->getError();
	if (err != 0)
		m_lastError = err;
	return err;
}

int SwTransfer::startRxWithCkc(int *outLifetimeSec)
{
	if (m_buffer != NULL)
		return 8;
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
		return 1;

	SdiBufferFactoryBase *factory = swGetBufferFactory();
	if (factory == NULL)
		return 1;

	SwString label("SdiBuffer");
	SdiBuffer *rawBuf = factory->create(m_extMem, m_bufferSize, label);
	m_buffer = static_cast<SdiBufferCXD3705 *>(rawBuf);
	if (m_buffer == NULL)
		return 1;

	int err = 0;
	if ((uint32_t)m_buffer->size() != m_bufferSize) {
		err = 2;
	} else {
		uint8_t needCkc = 0;
		uint8_t sessionCkc = 0;
		err = prepareCkc(&needCkc, &sessionCkc);
		if (err == 0) {
			uint32_t lifetime = 0;
			const void *ivPtr = m_hasIv ? (const void *)m_iv : NULL;
			err = dev->startRxStream(&m_streamId, (uint8_t)m_sessionId,
						 m_buffer, m_event, &lifetime,
						 (unsigned int)m_mediaFmt, ivPtr,
						 NULL, 0, m_streamParams);
			if (err == 0) {
				m_rxStarted = 1;
				if (outLifetimeSec != NULL)
					*outLifetimeSec = (int)lifetime;
			}
		}
	}

	if (err != 0) {
		if (m_buffer != NULL) {
			delete m_buffer;
			m_buffer = NULL;
		}
		if (m_ckcThread != NULL) {
			delete m_ckcThread;
			m_ckcThread = NULL;
		}
	}
	return err;
}

int SwTransfer::start()
{
	int lifetime = 0;
	return startRxWithCkc(&lifetime);
}

int SwTransfer::reset()
{
	if (m_buffer == NULL)
		return 4;
	m_buffer->setEndOfStream();
	if (m_ckcThread != NULL) {
		m_ckcThread->stopCkc();
		delete m_ckcThread;
		m_ckcThread = NULL;
	}
	return 0;
}

int SwTransfer::isReady()
{
	if (m_buffer == NULL)
		return 0;
	propagateBufferError();
	if (m_buffer->available() == 0)
		return m_buffer->isEndOfStream() != 0 ? 1 : 0;
	return 0;
}

int SwTransfer::getState()
{
	return m_lastError;
}

int SwTransfer::setParameter(int parameter)
{
	m_timeoutMs = parameter;
	if (m_buffer != NULL)
		m_buffer->setTimeout(parameter);
	return 0;
}

int SwTransfer::attach(int value)
{
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
		return 1;
	return dev->getKeyLifetime((uint8_t)m_sessionId,
				   reinterpret_cast<uint32_t *>((uintptr_t)value));
}

int SwTransfer::transfer(int value)
{
	return startRxWithCkc(reinterpret_cast<int *>((uintptr_t)value));
}

int SwTransfer::write(int offset, int size)
{
	void *buf = reinterpret_cast<void *>((uintptr_t)offset);
	if (buf == NULL)
		return 5;
	if (m_buffer == NULL)
		return 4;
	if (m_aborted) {
		int err = m_buffer->getError();
		return err != 0 ? err : 1;
	}
	m_buffer->setTimeout(m_timeoutMs);
	int written = m_buffer->write(buf, (unsigned int)size);
	if (written == -1) {
		propagateBufferError();
		return m_lastError != 0 ? m_lastError : 102;
	}
	return 0;
}

int SwTransfer::getChunkList(void **chunks, int *count)
{
	if (count != NULL)
		*count = 0;
	if (chunks == NULL)
		return 5;
	if (m_buffer == NULL)
		return 4;
	if (m_aborted) {
		int err = m_buffer->getError();
		return err != 0 ? err : 1;
	}
	m_buffer->setTimeout(m_timeoutMs);
	SdiBufferChunk *list = NULL;
	int n = m_buffer->getWriteChunkList(&list);
	if (n == -1) {
		propagateBufferError();
		return m_lastError != 0 ? m_lastError : 102;
	}
	*chunks = list;
	if (count != NULL)
		*count = n;
	return 0;
}

int SwTransfer::release(int size)
{
	if (m_buffer == NULL)
		return 4;
	int err = m_buffer->append((unsigned int)size);
	if (err == 0) {
		propagateBufferError();
		return m_lastError != 0 ? m_lastError : 1;
	}
	return 0;
}

int SwTransfer::setSessionData(const void *data)
{
	if (data != NULL) {
		memcpy(m_iv, data, sizeof(m_iv));
		m_hasIv = 1;
		return 1;
	}
	memset(m_iv, 0, sizeof(m_iv));
	m_hasIv = 0;
	return 0;
}

int SwTransfer::getSessionData(int *out)
{
	if (out == NULL)
		return 5;
	if (m_buffer == NULL)
		return 4;
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
		return 1;
	return dev->getStreamIv(out, (int)m_streamId, m_buffer);
}

int SwTransfer::isKeyExpiring()
{
	if (m_buffer == NULL)
		return 0;
	return m_buffer->isKeyExpiring();
}

int SwTransfer::startMove(int value)
{
	if (m_buffer == NULL)
		return 4;
	int ok = m_buffer->setError(value);
	m_aborted = 1;
	return (ok == 0) ? 1 : 0;
}
