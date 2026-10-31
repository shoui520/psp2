#include "session.h"
#include "session_impl.h"

namespace IPMI {

Session::~Session()
{
}

/* Destroys the session through its implementation. */
int Session::destroy(Session *pSession)
{
	return pSession->destroy();
}

SceSize Session::getBufferSize(const Config *pConfig)
{
	return impl::SessionImpl::getBufferSize(pConfig);
}

}
