/*
 * A NEMO message's SOAP fault (0x81079594-): the Fault read with the
 * exceptions of its detail and of a FaultDetails header, and the header
 * left of a message read. Sony's object starts here: its strings are a pool
 * of their own, with a second copy of the SOAP namespace. The file's name is
 * ours.
 */
#include "mln_internal.h"

#define MLN_NEMO_NS_SOAP	"http://schemas.xmlsoap.org/soap/envelope/"
#define MLN_NEMO_NS_CORE	"http://nemo.intertrust.com/2005/10/core"

/*
 * A SOAP Fault read: its faultcode and faultstring, its faultactor if any,
 * and the exceptions of its detail and of a FaultDetails header.
 */
__attribute__((noinline))
unsigned int MlnImpl_81079594(MlnXmlNode *fault, MlnXmlNode *header, MlnNemoFaults **out)
{
	MlnXmlNodeList *list = NULL;
	MlnXmlNodeList *details = NULL;
	MlnXmlNode *detail;
	MlnXmlNode *faultDetails;
	MlnNemoFaults *faults;
	unsigned int ret;

	faults = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*faults));
	if (faults == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(faults, 0, sizeof(*faults));
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, fault, "faultcode", NULL, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto format;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, list->nodes[0], &faults->code);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, fault, "faultstring", NULL, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto format;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, list->nodes[0], &faults->string);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, fault, "faultactor", NULL, &list);
	if (ret == 0) {
		if (list->count != 1)
			goto format;
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, list->nodes[0], &faults->actor);
	}
	if (ret != 0 && ret != MLN_XML_ERROR_NOT_FOUND)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	detail = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, fault, "detail", MLN_NEMO_NS_SOAP, &list);
	if (ret == 0) {
		if (list->count != 1)
			goto format;
		detail = list->nodes[0];
	} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
		goto out;
	}
	faultDetails = NULL;
	if (header != NULL) {
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, header, "FaultDetails", MLN_NEMO_NS_CORE,
				      &details);
		if (ret == 0)
			faultDetails = details->nodes[0];
		else if (ret != MLN_XML_ERROR_NOT_FOUND)
			goto out;
	}
	ret = MlnImpl_810792B4(detail, faultDetails, &faults->items, &faults->count);
	if (ret == 0) {
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
		list = NULL;
		MlnXmlListFree(MLN_HEAP_GENERAL, details);
		*out = faults;
		faults = NULL;
		details = NULL;
	}
	goto out;
format:
	ret = MLN_NEMO_ERROR_FORMAT;
out:
	MlnImpl_810794DC(faults);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnXmlListFree(MLN_HEAP_GENERAL, details);
	return ret;
}

/* the fault a message read gave */
__attribute__((noinline))
unsigned int MlnImpl_81079814(const MlnNemoMessage *message, MlnNemoFaults **faults)
{
	if (message == NULL || faults == NULL)
		return 3;
	if (message->faults == NULL)
		return MLN_NEMO_ERROR_NO_FAULT;
	*faults = message->faults;
	return 0;
}

/* a header's nodes freed (a node that cannot be freed stops it) */
__attribute__((noinline))
unsigned int MlnImpl_81079854(MlnNemoHeaderNodes *nodes)
{
	unsigned int ret;
	int i;

	if (nodes == NULL)
		return 0;
	if (nodes->items != NULL)
		for (i = 0; i < nodes->count; i++) {
			ret = MlnXmlNodeFree(MLN_HEAP_GENERAL, nodes->items[i].node);
			if (ret != 0)
				return ret;
		}
	MlnFree(MLN_HEAP_GENERAL, nodes->items);
	MlnFree(MLN_HEAP_GENERAL, nodes);
	return 0;
}

/* a header emptied but for a request's MessageID or originatesFrom kept */
__attribute__((noinline))
unsigned int MlnImpl_810798CC(MlnNemoHeader *header)
{
	int i;

	for (i = 0; i < 8; i++) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, header->parts[i]);
		header->parts[i] = NULL;
	}
	if (header->present & MLN_NEMO_PART_REQUEST_ID) {
		header->count = 1;
		header->present = MLN_NEMO_PART_REQUEST_ID;
	} else if (header->present & MLN_NEMO_PART_EXPECTED_ORIGIN) {
		header->count = 1;
		header->present = MLN_NEMO_PART_EXPECTED_ORIGIN;
	} else {
		header->count = 0;
		header->present = 0;
	}
	return 0;
}

/*
 * An error of the XML, cipher or key code as a NEMO message's: malformed
 * input is the message's format, a failed operation its failure.
 */
__attribute__((noinline))
unsigned int MlnImpl_8107A340(unsigned int ret)
{
	switch (ret & 0xFFFF0000) {
	case 0x10010000:
	case 0x22010000:
	case 0x22090000:
	case 0x220A0000:
		return MLN_NEMO_ERROR_FORMAT;
	case 0x22020000:
	case 0x22030000:
	case 0x22060000:
		return MLN_NEMO_ERROR_FAILED;
	}
	return ret;
}
