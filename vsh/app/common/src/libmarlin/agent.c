/*
 * An agent a license transfer carries: its controls, checked against the signatures of the
 * bundle that carries them, run, and its result as an AgentResultBlock (0x81068C90-). The
 * file's name is ours.
 */
#include "mln_internal.h"

#define MLN_AGENT_DIGEST_SHA1_SIZE	20

/*
 * Check the controls against one signature of the bundle: each control a reference of the
 * signature names (by the Id of its node) is digested and the signature verified over it; the
 * signer is kept in the control.
 */
static inline unsigned int MlnAgentSignatureVerify(MlnXmlNode *signature, MlnXmlNode *reference,
						   int noControls, int count,
						   MlnOctopusControl **controls, MlnBuffer **ids,
						   int usage)
{
	MlnBuffer *digestData = NULL;
	MlnBuffer *uri = NULL;
	MlnBuffer *signer = NULL;
	MlnBuffer *referenceId;
	unsigned char digest[32];
	unsigned char *digestPointer;
	int digestType = 4;
	int digestSize;
	int verified;
	const MlnBuffer *id;
	unsigned int ret;
	int i;

	if (noControls || reference == NULL) {
		ret = 3;
		goto out;
	}
	if (MlnXmlGetAttribute(MLN_HEAP_GENERAL, reference, NULL, "URI", &uri) != 0) {
		ret = MLN_EXCHANGE_ERROR_FORMAT;
		goto out;
	}
	if (uri->data == NULL) {
		ret = 5;
		goto out;
	}
	if (uri->data[0] != '#') {
		ret = MLN_EXCHANGE_ERROR_FORMAT;
		goto out;
	}
	ret = MlnImpl_8105BF78(signature, &digestSize);
	if (ret != 0)
		goto out;
	for (i = 0; i < count; i++) {
		id = ids[i];
		if (id == NULL) {
			ret = 5;
			goto out;
		}
		if (id->size + 1 == uri->size) {
			if (id->data == NULL) {
				ret = 5;
				goto out;
			}
			if (MlnMemcmp(id->data, uri->data + 1, id->size) == 0) {
				referenceId = ids[i];
				digestPointer = digest;
				ret = MlnImpl_81060F80(controls[i], &digestData);
				if (ret != 0)
					goto out;
				if (digestSize == MLN_AGENT_DIGEST_SHA1_SIZE)
					ret = MlnImpl_810570A8(digestData->data, digestData->size,
							       digestPointer);
				else
					ret = MlnImpl_8105713C(digestData->data, digestData->size,
							       digestPointer);
				if (ret != 0)
					goto out;
				ret = MlnImpl_8105B568(signature, usage, 1, &referenceId,
						       &digestPointer, &digestType, NULL, &signer,
						       &verified);
				if (ret != 0)
					goto out;
				MlnBufferDestroy(MLN_HEAP_GENERAL, digestData);
				digestData = NULL;
				MlnFree(MLN_HEAP_GENERAL, controls[i]->signer);
				controls[i]->signer = signer;
				signer = NULL;
				continue;
			}
			id = ids[i];
			if (id == NULL) {
				ret = 5;
				goto out;
			}
		}
		if (id->data == NULL) {
			ret = 5;
			goto out;
		}
	}
	ret = 0;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, digestData);
	MlnBufferDestroy(MLN_HEAP_GENERAL, signer);
	MlnBufferDestroy(MLN_HEAP_GENERAL, uri);
	return ret;
}

/*
 * The controls of a carrier's bundle, the one of that id first, checked against the bundle's
 * signatures, and the carrier's parameters.
 */
static inline unsigned int MlnAgentControls(MlnXmlNode *carrier, const MlnBuffer *controlId,
					    int usage, int *outCount,
					    MlnOctopusControl ***outControls,
					    MlnPlanktonParameterBlock **outParameters)
{
	MlnXmlNodeList *controlNodes = NULL;
	MlnXmlNode *bundle = NULL;
	MlnXmlNodeList *signatures = NULL;
	MlnXmlNodeList *parameterBlocks = NULL;
	MlnXmlNode *reference = NULL;
	MlnPlanktonParameterBlock *parameters = NULL;
	MlnOctopusControl **controls = NULL;
	MlnBuffer **ids = NULL;
	MlnOctopusControl *control;
	MlnBuffer *uid;
	MlnBuffer *id;
	MlnXmlNode *node;
	MlnXmlNode *signature;
	unsigned int ret;
	int signatureCount;
	int noControls;
	int filled;
	int count = 0;
	int i;

	if (controlId == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, carrier, "Bundle/Control", MLN_OCTOPUS_NAMESPACE,
			      &controlNodes);
	if (ret != 0)
		goto out;

	/* each control and the Id of its node; the control of that id first */
	count = controlNodes->count;
	control = NULL;
	uid = NULL;
	id = NULL;
	controls = MlnAllocate(MLN_HEAP_GENERAL, count * sizeof(MlnOctopusControl));
	if (controls == NULL) {
		ret = 2;
		goto parsed;
	}
	MlnMemset(controls, 0, count * sizeof(MlnOctopusControl));
	ids = MlnAllocate(MLN_HEAP_GENERAL, count * sizeof(*ids));
	if (ids == NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
		MlnBufferDestroy(MLN_HEAP_GENERAL, id);
		MlnImpl_8103C284(control);
		ret = 2;
		goto unparse;
	}
	MlnMemset(ids, 0, count * sizeof(*ids));
	filled = 0;
	for (i = 0; i < count; i++) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
		uid = NULL;
		node = controlNodes->nodes[i];
		if (MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "uid", &uid) != 0)
			continue;
		ret = MlnImpl_81042B38(node, &control);
		if (ret == 0)
			ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "Id", &id);
		if (ret != 0) {
			MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
			MlnBufferDestroy(MLN_HEAP_GENERAL, id);
			MlnImpl_8103C284(control);
			goto unparse;
		}
		if (MlnBufferCompare(MLN_HEAP_GENERAL, controlId, uid) == 0) {
			if (controls[0] != NULL)
				MlnImpl_8103C284(controls[0]);
			if (ids[0] != NULL)
				MlnBufferDestroy(MLN_HEAP_GENERAL, ids[0]);
			controls[0] = control;
			ids[0] = id;
		} else {
			if (++filled == count) {
				MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
				MlnBufferDestroy(MLN_HEAP_GENERAL, id);
				ret = MLN_EXCHANGE_ERROR_FORMAT;
				MlnImpl_8103C284(control);
				goto unparse;
			}
			controls[filled] = control;
			ids[filled] = id;
		}
		id = NULL;
		control = NULL;
	}
	ret = 0;
parsed:
	MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnImpl_8103C284(control);
	if (ret != 0)
		goto out;
	goto checked;
unparse:
	for (i = 0; i < count; i++)
		MlnImpl_8103C284(controls[i]);
	MlnFree(MLN_HEAP_GENERAL, controls);
	if (ids != NULL) {
		for (i = 0; i < count; i++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, ids[i]);
		MlnFree(MLN_HEAP_GENERAL, ids);
		ids = NULL;
	}
	controls = NULL;
	goto out;

checked:
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, carrier, "ParameterBlock", MLN_PLANKTON_NAMESPACE,
			      &parameterBlocks);
	if (ret != MLN_XML_ERROR_NOT_FOUND) {
		if (ret != 0)
			goto out;
		ret = MlnImpl_81042E1C(parameterBlocks, &parameters);
		if (ret != 0)
			goto out;
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, controlNodes);
	controlNodes = NULL;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, carrier, "Bundle", MLN_OCTOPUS_NAMESPACE, &bundle);
	if (ret == 0)
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, bundle, "Signature", MLN_XMLDSIG_NAMESPACE,
				      &signatures);
	if (ret != 0)
		goto out;
	signatureCount = signatures->count;
	if (count > signatureCount) {
		ret = MLN_EXCHANGE_ERROR_FORMAT;
		goto out;
	}
	noControls = controls == NULL || ids == NULL;
	for (i = 0; i < signatureCount; i++) {
		MlnXmlNodeFree(MLN_HEAP_GENERAL, reference);
		reference = NULL;
		signature = signatures->nodes[i];
		if (MlnXmlFindNode(MLN_HEAP_GENERAL, signature, "SignedInfo/Reference",
				   MLN_XMLDSIG_NAMESPACE, &reference) != 0 || signature == NULL) {
			ret = MLN_EXCHANGE_ERROR_FORMAT;
			goto out;
		}
		ret = MlnAgentSignatureVerify(signature, reference, noControls, count, controls, ids,
					      usage);
		if (ret != 0)
			goto out;
	}
	*outCount = count;
	*outControls = controls;
	controls = NULL;
	*outParameters = parameters;
	parameters = NULL;
	ret = 0;
out:
	MlnXmlListFree(MLN_HEAP_GENERAL, controlNodes);
	MlnXmlListFree(MLN_HEAP_GENERAL, parameterBlocks);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, bundle);
	MlnXmlListFree(MLN_HEAP_GENERAL, signatures);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, reference);
	MlnImpl_8103CFC0(parameters);
	if (controls != NULL) {
		for (i = 0; i < count; i++)
			MlnImpl_8103C284(controls[i]);
		MlnFree(MLN_HEAP_GENERAL, controls);
	}
	if (ids != NULL) {
		for (i = 0; i < count; i++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, ids[i]);
		MlnFree(MLN_HEAP_GENERAL, ids);
	}
	return ret;
}

/*
 * Run the agent of an AgentCarrier: its controls checked, the control of its controlId
 * performed with its parameters, and the result given as an AgentResultBlock, with the keys
 * still to be stored.
 */
unsigned int MlnImpl_81068C90(MlnXmlNode *carrier, MlnArray *pair, int usage, MlnXmlNode **node,
			      MlnArray **pending)
{
	MlnBuffer *controlId = NULL;
	MlnBuffer *name = NULL;
	MlnBuffer *contextId = NULL;
	MlnArray *result = NULL;
	unsigned int status = 0;
	MlnBuffer *data = NULL;
	MlnBuffer *encoded = NULL;
	MlnXmlNode *block = NULL;
	MlnPlanktonParameterBlock *parameters = NULL;
	MlnOctopusControl **controls = NULL;
	char code[16];
	char *text = NULL;
	unsigned int ret;
	int count = 0;
	int length;
	int i;

	if (carrier == NULL || node == NULL || pending == NULL) {
		ret = 3;
		goto done;
	}
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, carrier, NULL, "controlId", &controlId);
	if (ret != 0)
		goto done;
	ret = MlnAgentControls(carrier, controlId, usage, &count, &controls, &parameters);
	if (ret == 0)
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, carrier, NULL, "name", &name);
	if (ret == 0)
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, carrier, NULL, "contextId", &contextId);
	if (ret == 0)
		ret = MlnImpl_8106340C(count, controls, name, contextId, parameters, pair, &status,
				       &data, &result);
	if (ret != 0)
		goto done;
	MlnMemset(code, 0, sizeof(code));
	length = MlnImpl_81052CA8(status, 1);
	if (length < 0) {
		ret = 3;
		goto done;
	}
	ret = MlnImpl_810529E8(status, 1, length, code);
	if (ret != 0)
		goto done;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "AgentResultBlock", "mc",
				 "urn:marlin:core:1-3:schemas", 0, NULL, 0, NULL, 0, &block);
	if (ret != 0)
		goto done;
	ret = MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, block, "resultCode", code);
	if (ret != 0)
		goto done;
	if (data != NULL) {
		ret = MlnImpl_81026814(MLN_HEAP_GENERAL, data, 0, 0, &encoded);
		if (ret != 0)
			goto done;
		MlnBufferDestroy(MLN_HEAP_GENERAL, data);
		data = NULL;
		text = MlnAllocate(MLN_HEAP_GENERAL, encoded->size + 1);
		if (text == NULL) {
			ret = 2;
			goto done;
		}
		MlnMemcpy(text, encoded->data, encoded->size);
		text[encoded->size] = '\0';
		MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
		encoded = NULL;
		ret = MlnXmlNodeAddText(MLN_HEAP_GENERAL, block, text);
		if (ret != 0)
			goto done;
	}
	*node = block;
	block = NULL;
	*pending = result;
	result = NULL;
done:
	MlnBufferDestroy(MLN_HEAP_GENERAL, controlId);
	MlnBufferDestroy(MLN_HEAP_GENERAL, name);
	MlnBufferDestroy(MLN_HEAP_GENERAL, contextId);
	MlnArrayDestroy(MLN_HEAP_GENERAL, result);
	MlnImpl_8103CFC0(parameters);
	if (controls != NULL) {
		for (i = 0; i < count; i++)
			MlnImpl_8103C284(controls[i]);
		MlnFree(MLN_HEAP_GENERAL, controls);
	}
	if (ret != 0)
		ret = MlnImpl_810685FC(ret);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
	MlnFree(MLN_HEAP_GENERAL, text);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, block);
	return ret;
}
