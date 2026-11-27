/*
 * DTCPIP-SW core entry points:
 *   - SwMwContentSupportCreate (singleton factory)
 *   - SwContentSupport (content + content manager lifecycle, SRM update)
 *   - SwContent (sink & move-sink authentication and move command controller)
 *   - SwContentManager (SwTransfer lifecycle factory)
 */

#include "sw_engine.h"

#include <string.h>

static SwMutex g_swContentSupportMutex;
static SwAkeConfig g_swDefaultAkeConfig(4, 8, 1);

/* ---- SwMwContentSupportCreate ---- */

extern "C" int SwMwContentSupportCreate(struct SwMwContentSupport **support)
{
	if (support == NULL)
		return 5;
	SwScopedLock guard(g_swContentSupportMutex);
	static SwContentSupport s_instance;
	*support = &s_instance;
	return 0;
}

/* ---- SwContentSupport ---- */

SwContentSupport::SwContentSupport()
	: m_contentMutex(), m_managerMutex(), m_content(NULL), m_manager(NULL)
{
	SdiDevice *dev = NULL;
	SdiDevice::getInstance(&dev);
}

SwContentSupport::~SwContentSupport()
{
	destroy();
}

void SwContentSupport::destroy()
{
	SwScopedLock guardContent(m_contentMutex);
	SwScopedLock guardManager(m_managerMutex);
	releaseManagerLocked();
	releaseContentLocked();
}

void SwContentSupport::dispose()
{
	delete this;
}

int SwContentSupport::releaseContentLocked()
{
	if (m_manager != NULL)
		return 8;
	if (m_content != NULL) {
		delete m_content;
		m_content = NULL;
	}
	return 0;
}

int SwContentSupport::releaseManagerLocked()
{
	if (m_manager != NULL) {
		delete m_manager;
		m_manager = NULL;
	}
	return 0;
}

int SwContentSupport::createContent(void **content, void *observer)
{
	if (content == NULL)
		return 5;
	SwScopedLock guard(m_contentMutex);
	int err = 0;
	if (m_content == NULL) {
		m_content = new (std::nothrow) SwContent();
		if (m_content == NULL)
			err = 2;
	}
	if (err != 0) {
		releaseContentLocked();
		return err;
	}
	m_content->setObserver(static_cast<SwSinkObserver *>(observer));
	*content = static_cast<struct SwMwContent *>(m_content);
	return 0;
}

int SwContentSupport::releaseContent()
{
	SwScopedLock guard(m_contentMutex);
	return releaseContentLocked();
}

int SwContentSupport::createManager(void **manager)
{
	if (manager == NULL)
		return 5;
	if (m_content == NULL)
		return 4;
	SwScopedLock guard(m_managerMutex);
	int err = 0;
	if (m_manager == NULL) {
		m_manager = new (std::nothrow) SwContentManager(this);
		if (m_manager == NULL)
			err = 2;
	}
	if (err == 0) {
		*manager = static_cast<struct SwMwContentManager *>(m_manager);
	} else {
		releaseManagerLocked();
	}
	return err;
}

int SwContentSupport::releaseManager()
{
	SwScopedLock guard(m_managerMutex);
	return releaseManagerLocked();
}

int SwContentSupport::setSrm(const void *data, size_t size)
{
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
		return 1;
	SwCert cert;
	int err = dev->getDeviceCert(&cert);
	if (err != 0)
		return err;
	SwSrm srm;
	err = srm.setRaw(data, size, &cert);
	if (err == 0)
		err = dev->setSrm(&srm);
	return err;
}

SwContent *SwContentSupport::content()
{
	SwScopedLock guard(m_contentMutex);
	return m_content;
}

SwContentManager *SwContentSupport::manager()
{
	SwScopedLock guard(m_managerMutex);
	return m_manager;
}

bool SwContentSupport::isSessionInUse(uint32_t sessionId)
{
	SwScopedLock guard(m_managerMutex);
	if (m_manager == NULL)
		return false;
	return m_manager->isSessionInUse(sessionId);
}

/* ---- SwContent ---- */

SwContent::SwContent()
	: m_sessionTable(), m_observer(NULL), m_threads(), m_threadsMutex()
{
}

SwContent::~SwContent()
{
	for (;;) {
		m_threadsMutex.lock();
		bool allStopped = true;
		SwListIterator it = m_threads.begin();
		while (it != m_threads.end()) {
			SwSinkAuthThread *th = SwPtrList<SwSinkAuthThread>::deref(it);
			SwListIterator cur = it;
			++it;
			if (th == NULL) {
				m_threads.erase(cur);
			} else if (th->isStopped()) {
				delete th;
				m_threads.erase(cur);
			} else {
				th->cancel(1025);
				allStopped = false;
			}
		}
		m_threadsMutex.unlock();
		if (allStopped)
			break;
		swYield();
	}
}

void SwContent::reapStoppedThreads()
{
	SwListIterator it = m_threads.begin();
	while (it != m_threads.end()) {
		SwSinkAuthThread *th = SwPtrList<SwSinkAuthThread>::deref(it);
		SwListIterator cur = it;
		++it;
		if (th == NULL) {
			m_threads.erase(cur);
		} else if (!th->isRunning()) {
			delete th;
			m_threads.erase(cur);
		}
	}
}

void SwContent::eraseThread(SwSinkAuthThread *thread)
{
	SwListIterator it = m_threads.begin();
	while (it != m_threads.end()) {
		SwSinkAuthThread *th = SwPtrList<SwSinkAuthThread>::deref(it);
		SwListIterator cur = it;
		++it;
		if (th == thread) {
			m_threads.erase(cur);
			break;
		}
	}
}

int SwContent::findActiveMoveSession(SwSinkAkeSession **outSession,
				     const SwNetAddress &addr, uint8_t label)
{
	if (outSession == NULL || !addr.isValid())
		return 5;
	*outSession = NULL;
	SwScopedLock guard(m_threadsMutex);
	reapStoppedThreads();
	for (SwListIterator it = m_threads.begin(); it != m_threads.end(); ++it) {
		SwSinkAuthThread *th = SwPtrList<SwSinkAuthThread>::deref(it);
		if (th == NULL || !th->isRunning())
			continue;
		if (th->peerAddress().host().compare(addr.host()) == 0 &&
		    th->peerAddress().port() == addr.port()) {
			SwSinkAkeSession *sess = th->session();
			uint8_t activeLabel = 0;
			if (sess != NULL && sess->isActive() &&
			    sess->getActiveMoveLabel(&activeLabel) && activeLabel == label) {
				*outSession = sess;
				return 0;
			}
		}
	}
	return 100;
}

bool SwContent::isSinkSessionInUse(const SwNetAddress &addr)
{
	SwPtrList<uint32_t> ids;
	bool inUse = false;
	if (m_sessionTable.findSinkSessionIds(addr, ids) == 0) {
		struct SwMwContentSupport *supportBase = NULL;
		if (SwMwContentSupportCreate(&supportBase) == 0 && supportBase != NULL) {
			SwContentSupport *support = static_cast<SwContentSupport *>(supportBase);
			for (SwListIterator it = ids.begin(); it != ids.end() && !inUse; ++it) {
				uint32_t *idPtr = SwPtrList<uint32_t>::deref(it);
				if (idPtr != NULL)
					inUse = support->isSessionInUse(*idPtr);
			}
		}
	}
	for (SwListIterator it = ids.begin(); it != ids.end(); ++it) {
		uint32_t *idPtr = SwPtrList<uint32_t>::deref(it);
		delete idPtr;
	}
	ids.clear();
	return inUse;
}

bool SwContent::isMoveSessionInUse(const SwNetAddress &addr, uint8_t label)
{
	SwPtrList<uint32_t> ids;
	bool inUse = false;
	if (m_sessionTable.findMoveSessionIds(addr, label, ids) == 0) {
		struct SwMwContentSupport *supportBase = NULL;
		if (SwMwContentSupportCreate(&supportBase) == 0 && supportBase != NULL) {
			SwContentSupport *support = static_cast<SwContentSupport *>(supportBase);
			for (SwListIterator it = ids.begin(); it != ids.end() && !inUse; ++it) {
				uint32_t *idPtr = SwPtrList<uint32_t>::deref(it);
				if (idPtr != NULL)
					inUse = support->isSessionInUse(*idPtr);
			}
		}
	}
	for (SwListIterator it = ids.begin(); it != ids.end(); ++it) {
		uint32_t *idPtr = SwPtrList<uint32_t>::deref(it);
		delete idPtr;
	}
	ids.clear();
	return inUse;
}

int SwContent::begin(void *uri, int value, int mode)
{
	(void)mode;
	int dummyId = 0;
	return start(&dummyId, static_cast<struct SwMwUri *>(uri), value);
}

int SwContent::start(int *uriSet, struct SwMwUri *uri, int mode)
{
	if (uri == NULL)
		return 5;
	const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
	if (addr->isAnyHost()) {
		SdiDevice *dev = NULL;
		if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
			return 1;
		uint32_t sid = 0;
		int err = dev->openSinkSession(&sid, *addr, 2, 0);
		if (err == 0)
			err = m_sessionTable.addSinkSession(*addr, sid, 0);
		if (err == 0 && uriSet != NULL)
			*uriSet = (int)sid;
		return err;
	}
	if (!addr->isValid())
		return 5;

	SwScopedLock guard(m_threadsMutex);
	reapStoppedThreads();
	for (SwListIterator it = m_threads.begin(); it != m_threads.end(); ++it) {
		SwSinkAuthThread *th = SwPtrList<SwSinkAuthThread>::deref(it);
		if (th != NULL && th->isRunning() && !th->isMoveAuth() &&
		    th->peerAddress().host().compare(addr->host()) == 0 &&
		    th->peerAddress().port() == addr->port()) {
			return 101;
		}
	}

	SwSinkAuthThread *th = new (std::nothrow) SwSinkAuthThread(0, 0, 0);
	if (th == NULL)
		return 2;
	uint32_t sid = 0;
	int err = th->configureWithId(&sid, this, *addr, (uint32_t)mode, g_swDefaultAkeConfig);
	if (err == 0) {
		err = th->start();
		if (err == 0) {
			m_threads.pushBackPtr(th);
			if (uriSet != NULL)
				*uriSet = (int)sid;
		}
	}
	if (err != 0)
		delete th;
	return err;
}

int SwContent::close(struct SwMwUri *uri)
{
	if (uri == NULL)
		return 5;
	const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
	if (isSinkSessionInUse(*addr))
		return 8;

	SwScopedLock guard(m_threadsMutex);
	bool canceledAny = false;
	SwListIterator it = m_threads.begin();
	while (it != m_threads.end()) {
		SwSinkAuthThread *th = SwPtrList<SwSinkAuthThread>::deref(it);
		SwListIterator cur = it;
		++it;
		if (th != NULL &&
		    th->peerAddress().host().compare(addr->host()) == 0 &&
		    th->peerAddress().port() == addr->port()) {
			th->cancel(1027);
			delete th;
			m_threads.erase(cur);
			canceledAny = true;
		}
	}
	int err = m_sessionTable.removeSinkSession(*addr);
	if (err == 100 && canceledAny)
		err = 0;
	return err;
}

int SwContent::list(void *out)
{
	(void)out;
	return m_sessionTable.closeAllSessions();
}

int SwContent::setUri(int *uriSet, struct SwMwUri *uri, int mode)
{
	if (uri == NULL)
		return 5;
	const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
	if (!addr->isValid())
		return 5;

	SwScopedLock guard(m_threadsMutex);
	SwSinkAuthThread *th = new (std::nothrow) SwSinkAuthThread(1, 0, 0);
	if (th == NULL)
		return 2;
	uint32_t sid = 0;
	int err = th->configureWithId(&sid, this, *addr, (uint32_t)mode, g_swDefaultAkeConfig);
	if (err == 0) {
		err = th->start();
		if (err == 0) {
			m_threads.pushBackPtr(th);
			if (uriSet != NULL)
				*uriSet = (int)sid;
		}
	}
	if (err != 0)
		delete th;
	return err;
}

int SwContent::operation(struct SwMwUri *uri, int *result)
{
	if (uri == NULL)
		return 5;
	const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
	uint8_t label = (uint8_t)(uintptr_t)result;
	if (isMoveSessionInUse(*addr, label))
		return 8;

	SwScopedLock guard(m_threadsMutex);
	bool canceledAny = false;
	SwListIterator it = m_threads.begin();
	while (it != m_threads.end()) {
		SwSinkAuthThread *th = SwPtrList<SwSinkAuthThread>::deref(it);
		SwListIterator cur = it;
		++it;
		if (th != NULL && th->isMoveAuth() &&
		    th->peerAddress().host().compare(addr->host()) == 0 &&
		    th->peerAddress().port() == addr->port()) {
			th->cancel(1027);
			delete th;
			m_threads.erase(cur);
			canceledAny = true;
		}
	}
	int err = m_sessionTable.removeMoveSession(*addr, label);
	if (err == 100 && canceledAny)
		err = 0;
	return err;
}

int SwContent::request(struct SwMwUri *uri, unsigned char label,
		       struct SwMwMoveMessage *message, int value)
{
	(void)value;
	if (uri == NULL || message == NULL)
		return 5;
	const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
	if (!addr->isValid())
		return 5;
	SwSinkAkeSession *active = NULL;
	if (findActiveMoveSession(&active, *addr, label) == 0 && active != NULL)
		return active->sendMoveFinalizeOnActive(message);
	return finalize(uri, message);
}

int SwContent::finalize(struct SwMwUri *uri, struct SwMwMoveMessage *message)
{
	if (uri == NULL || message == NULL)
		return 5;
	const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
	if (!addr->isValid())
		return 5;

	SwScopedLock guard(m_threadsMutex);
	SwSinkAuthThread *th = new (std::nothrow) SwSinkAuthThread(0, 1, 1);
	if (th == NULL)
		return 2;
	int err = th->configureMoveFinalize(this, *addr, 0, g_swDefaultAkeConfig, message);
	if (err == 0) {
		err = th->start();
		if (err == 0)
			m_threads.pushBackPtr(th);
	}
	if (err != 0)
		delete th;
	return err;
}

int SwContent::complete(struct SwMwUri *uri, unsigned char state)
{
	if (uri == NULL)
		return 5;
	const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
	if (!addr->isValid())
		return 5;

	SwSinkAkeSession *active = NULL;
	if (findActiveMoveSession(&active, *addr, state) == 0 && active != NULL)
		return active->sendMoveCompleteOnActive(state);

	SwScopedLock guard(m_threadsMutex);
	SwSinkAuthThread *th = new (std::nothrow) SwSinkAuthThread(0, 1, 0);
	if (th == NULL)
		return 2;
	int err = th->configureMoveComplete(this, *addr, 0, g_swDefaultAkeConfig, state);
	if (err == 0) {
		err = th->start();
		if (err == 0)
			m_threads.pushBackPtr(th);
	}
	if (err != 0)
		delete th;
	return err;
}

int SwContent::abort(struct SwMwUri *uri, unsigned char state)
{
	if (uri == NULL)
		return 5;
	const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
	if (!addr->isValid())
		return 5;

	SwScopedLock guard(m_threadsMutex);
	SwListIterator it = m_threads.begin();
	while (it != m_threads.end()) {
		SwSinkAuthThread *th = SwPtrList<SwSinkAuthThread>::deref(it);
		SwListIterator cur = it;
		++it;
		if (th == NULL) {
			m_threads.erase(cur);
			continue;
		}
		if (!th->isRunning()) {
			delete th;
			m_threads.erase(cur);
			continue;
		}
		if (th->peerAddress().host().compare(addr->host()) == 0 &&
		    th->peerAddress().port() == addr->port()) {
			SwSinkAkeSession *sess = th->session();
			uint8_t activeLabel = 0;
			if (sess != NULL && sess->getActiveMoveLabel(&activeLabel) &&
			    activeLabel == state) {
				delete th;
				m_threads.erase(cur);
				break;
			}
		}
	}
	return 0;
}

int SwContent::getState()
{
	return 0;
}

int SwContent::getStateResult(struct SwMwUri *uri, int mode)
{
	if (uri == NULL)
		return 5;
	const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
	if (!addr->isValid())
		return 5;

	const unsigned char *labelPtr = reinterpret_cast<const unsigned char *>((uintptr_t)mode);
	uint8_t label = (labelPtr != NULL) ? *labelPtr : 0;

	SwSinkAkeSession *sess = NULL;
	int err = findActiveMoveSession(&sess, *addr, label);
	if (err == 100) {
		SwSinkAuthThread *th = new (std::nothrow) SwSinkAuthThread(0, 1, 0);
		if (th == NULL)
			return 2;
		err = th->configure(this, *addr, 0, g_swDefaultAkeConfig);
		if (err == 0) {
			err = th->start();
			if (err == 0) {
				SwScopedLock guard(m_threadsMutex);
				m_threads.pushBackPtr(th);
				sess = th->session();
			}
		}
		if (err != 0)
			delete th;
	}
	if (sess != NULL)
		return sess->sendMoveCancel(label);
	return err;
}

int SwContent::getSessionIdForUri(const SwNetAddress &addr, uint32_t *outSessionId)
{
	return m_sessionTable.getSinkSessionId(addr, outSessionId);
}

int SwContent::registerAuthenticatedSink(const SwNetAddress &addr, uint32_t sessionId,
					 SwSinkAuthThread *thread)
{
	SwScopedLock guard(m_threadsMutex);
	if (thread != NULL && thread->isCanceled())
		return 8;
	int err = m_sessionTable.addSinkSession(addr, sessionId, 0);
	if (err == 0 && thread != NULL)
		eraseThread(thread);
	return err;
}

int SwContent::registerAuthenticatedMove(const SwNetAddress &addr, uint32_t sessionId,
					 uint8_t label, SwSinkAuthThread *thread)
{
	SwScopedLock guard(m_threadsMutex);
	if (thread != NULL && thread->isCanceled())
		return 8;
	int err = m_sessionTable.addMoveSession(addr, sessionId, label, 0, 0);
	if (err == 0 && thread != NULL)
		eraseThread(thread);
	return err;
}

int SwContent::checkSinkCkc(uint32_t sessionId, SwNetAddress *outAddr, uint8_t *outNeedCkc)
{
	return m_sessionTable.checkSinkSessionCkc(sessionId, outAddr, outNeedCkc);
}

int SwContent::checkMoveCkc(uint32_t sessionId, uint8_t label, SwNetAddress *outAddr,
			    uint8_t *outNeedCkc)
{
	return m_sessionTable.checkMoveSessionCkc(sessionId, label, outAddr, outNeedCkc);
}

/* ---- SwContentManager ---- */

SwContentManager::SwContentManager(SwContentSupport *support)
	: m_transfers(), m_support(support), m_mutex()
{
}

SwContentManager::~SwContentManager()
{
	destroy();
}

void SwContentManager::destroy()
{
	releaseAllOrOne(NULL);
}

void SwContentManager::dispose()
{
	delete this;
}

void SwContentManager::releaseAllOrOne(struct SwMwTransfer *target)
{
	SwScopedLock guard(m_mutex);
	SwListIterator it = m_transfers.begin();
	while (it != m_transfers.end()) {
		SwTransfer *tr = SwPtrList<SwTransfer>::deref(it);
		SwListIterator cur = it;
		++it;
		if (target != NULL) {
			if (static_cast<struct SwMwTransfer *>(tr) == target) {
				m_transfers.erase(cur);
				delete tr;
				return;
			}
		} else {
			delete tr;
		}
	}
	if (target == NULL)
		m_transfers.clear();
}

int SwContentManager::createTransfer(struct SwMwTransfer **transfer, struct SwMwUri *uri,
				     int mode, struct SwMwStreamEvent *event,
				     int receiveSize, struct SwMwSinkWork *work,
				     int sendBase)
{
	if (transfer == NULL)
		return 5;
	SwTransfer *tr = new (std::nothrow) SwTransfer(
		reinterpret_cast<void *>((uintptr_t)sendBase), (uint32_t)receiveSize);
	if (tr == NULL)
		return 2;

	int err = 0;
	SwContent *content = (m_support != NULL) ? m_support->content() : NULL;
	if (content == NULL)
		err = 4;
	uint32_t sessionId = 0;
	if (err == 0) {
		const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
		err = content->getSessionIdForUri(*addr, &sessionId);
		if (err != 0)
			err = 5;
		if (err == 0)
			err = tr->setup(sessionId, mode, event, work, 0, 0);
	}
	if (err == 0) {
		SwScopedLock guard(m_mutex);
		m_transfers.pushBackPtr(tr);
		*transfer = static_cast<struct SwMwTransfer *>(tr);
	} else {
		delete tr;
	}
	return err;
}

int SwContentManager::releaseTransfer(struct SwMwTransfer *transfer)
{
	releaseAllOrOne(transfer);
	return 0;
}

int SwContentManager::createTransferOn(struct SwMwTransfer **transfer, struct SwMwUri *uri,
				       int mode, struct SwMwStreamEvent *event,
				       unsigned char label, int receiveSize,
				       struct SwMwSinkWork *work, int sendBase)
{
	if (transfer == NULL)
		return 5;
	SwTransfer *tr = new (std::nothrow) SwTransfer(
		reinterpret_cast<void *>((uintptr_t)sendBase), (uint32_t)receiveSize);
	if (tr == NULL)
		return 2;

	int err = 0;
	SwContent *content = (m_support != NULL) ? m_support->content() : NULL;
	if (content == NULL)
		err = 4;
	uint32_t sessionId = 0;
	if (err == 0) {
		const SwNetAddress *addr = reinterpret_cast<const SwNetAddress *>(uri);
		err = content->sessionTable().getMoveSessionId(*addr, label, &sessionId);
		if (err != 0)
			err = 1018;
		if (err == 0)
			err = tr->setup(sessionId, mode, event, work, 1, label);
	}
	if (err == 0) {
		SwScopedLock guard(m_mutex);
		m_transfers.pushBackPtr(tr);
		*transfer = static_cast<struct SwMwTransfer *>(tr);
	} else {
		delete tr;
	}
	return err;
}

bool SwContentManager::isSessionInUse(uint32_t sessionId)
{
	SwScopedLock guard(m_mutex);
	for (SwListIterator it = m_transfers.begin(); it != m_transfers.end(); ++it) {
		SwTransfer *tr = SwPtrList<SwTransfer>::deref(it);
		if (tr != NULL && tr->sessionId() == sessionId)
			return true;
	}
	return false;
}
