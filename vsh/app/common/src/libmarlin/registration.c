/*
 * A registration service's context and requests: an element wrapped in a new element, the
 * context of a session, the stored credentials and the registration request taken out of
 * the store (0x81064CD8-). The file's name is ours.
 */
#include "mln_internal.h"

/* what a registration session keeps between its steps (12 bytes) */
typedef struct MlnRegistrationContext {
	MlnBuffer *buffer;
	MlnXmlNode *request;
	MlnXmlNode *response;
} MlnRegistrationContext;

/* an element wrapped in a new element of that name, without a namespace prefix */
int MlnImpl_81064CD8(MlnXmlNode *node, const char *name, MlnXmlNode **out)
{
	MlnXmlNode *element = NULL;
	MlnXmlBuildContent content;
	int ret;

	if (node == NULL || name == NULL || out == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, node, &element);
	if (ret == 0) {
		content.kind = MLN_XML_BUILD_NODE;
		content.value.node = element;
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, name, "", NULL, 0, NULL, 1, &content, 0,
					 out);
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, element);
	return ret;
}

/* a new, empty context */
unsigned int MlnImpl_81064D8C(void **context)
{
	MlnRegistrationContext *new;

	if (context == NULL)
		return 3;
	new = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*new));
	if (new == NULL)
		return 2;
	MlnMemset(new, 0, sizeof(*new));
	*context = new;
	return 0;
}

/* free a context */
unsigned int MlnImpl_81064DD4(void *context)
{
	MlnRegistrationContext *registration = context;

	if (registration != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, registration->buffer);
		MlnXmlNodeFree(MLN_HEAP_GENERAL, registration->response);
		MlnXmlNodeFree(MLN_HEAP_GENERAL, registration->request);
		MlnFree(MLN_HEAP_GENERAL, registration);
	}
	return 0;
}

/* the stored credentials (MlnImpl_81064340) */
unsigned int MlnImpl_81064E18(MlnBufferGroups **info)
{
	MlnBufferGroups *groups = NULL;
	unsigned int ret;

	if (info == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81064340(&groups);
	if (ret == 0) {
		*info = groups;
		groups = NULL;
	}
out:
	MlnImpl_81042A88(groups);
	return ret;
}

/* the stored registration request (MlnImpl_81043AE4 step 1) */
unsigned int MlnImpl_81064E68(MlnBuffer **request)
{
	MlnBuffer *buffer = NULL;
	unsigned int ret;

	if (request == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81043AE4(1, &buffer);
	if (ret == 0) {
		*request = buffer;
		buffer = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}
