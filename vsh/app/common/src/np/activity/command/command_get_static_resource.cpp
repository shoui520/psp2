/*
 * The command that reads the activity service's static resource (Sony's file:
 * command/command_get_static_resource.cpp).
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_get_static_resource.h"
#include "../common/activity_debug.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_parser/component_xml_parser.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "command\\command_get_static_resource.cpp"

CommandGetStaticResource::CommandGetStaticResource(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility)
{
	m_api = UTILITY_API_STATIC_RESOURCE;
}

CommandGetStaticResource::~CommandGetStaticResource()
{
}

int CommandGetStaticResource::SetParameter(StaticResource *resource)
{
	m_resource = resource;
	if (resource == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	sceClibMemset(resource, 0, sizeof(*resource));
	return 0;
}

int CommandGetStaticResource::SendRequest()
{
	int ret;

	m_urlSize = GetUrlBufferSize();
	m_url = (char *)activityMalloc(m_urlSize);
	if (m_url == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	sceClibMemset(m_url, 0, m_urlSize);
	m_urlLength = sceClibSnprintf(m_url, m_urlSize, "%s%s%s", m_utility->GetScheme(m_api),
		m_utility->GetHost(m_api), m_utility->GetPath(m_api));
	ret = m_http->SendRequest(HTTP_METHOD_GET, m_url, m_urlLength, NULL, 0);
	activityFree(m_url);
	m_url = NULL;
	return ret;
}

int CommandGetStaticResource::BeginParse()
{
	int ret;

	ret = m_xmlParser->BeginStaticResource(m_resource);
	if (!ACTIVITY_FAILED(ret)) {
		ret = m_xmlParser->AddStaticResourceObserver(this);
		if (ret < 0)
			m_xmlParser->EndParse();
	}
	return ret;
}

int CommandGetStaticResource::EndParse()
{
	m_xmlParser->EndParse();
	return m_xmlParser->RemoveStaticResourceObserver(this);
}

#line 167
int CommandGetStaticResource::ParseStreamData(void *data, SceSize size, int *done)
{
	SceSize length;
	int ret;

	ret = m_xmlParser->Parse((const char *)data, size, done);
	if (!ACTIVITY_FAILED(ret)) {
		if (m_resource == NULL) {
			ACTIVITY_CHECK("ParseStreamData");
		} else {
			/* the reply's text, for the signature check */
			length = sceClibStrnlen(m_resource->data, sizeof(m_resource->data));
			if ((int)(length + size) >= (int)sizeof(m_resource->data))
				size = sizeof(m_resource->data) - 1 - length;
			sceClibMemcpy_safe(m_resource->data + length, data, size);
			m_resource->data[length + size] = '\0';
			m_resource->dataSize = length + size;
		}
	}
	return ret;
}

void CommandGetStaticResource::UpdateStaticResource(StaticResource *resource)
{
}
