#include "client.h"
#include "client_impl.h"
#include "ipmi_error.h"

void *operator new(size_t size, void *where);

namespace IPMI {

int Client::create(Client **ppClient, const Config *pConfig, void *pUserData, void *pMemory)
{
	impl::ClientImpl *pClient;
	int ret;

	if (ppClient == NULL || pConfig == NULL || pMemory == NULL)
		return IPMI_ERROR_INVALID_ARGUMENT;
	pClient = new (pMemory) impl::ClientImpl;
	ret = pClient->initialize(pConfig, pUserData);
	if (ret != 0)
		return ret;
	*ppClient = pClient;
	return 0;
}

Client::Client()
{
}

Client::~Client()
{
}

SceSize Client::getBufferSize(const Config *pConfig)
{
	return impl::ClientImpl::getBufferSize(pConfig);
}

}
