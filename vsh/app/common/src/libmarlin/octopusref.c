/*
 * An Octopus reference: the Uid of the object it names and, when given, the Digest that
 * binds it (its transforms, digest method and value).
 */
#include "mln_internal.h"

#define MLN_OCTOPUS_ARRAY_TRANSFORMS	13	/* the kind MlnImpl_8103C57C frees a transform list as */

int MlnImpl_81082614(MlnXmlNode *node, MlnOctopusReference **out)
{
	MlnXmlNodeList *list = NULL;
	MlnOctopusArray *transforms = NULL;
	MlnBuffer *uid = NULL;
	MlnBuffer *digestMethod = NULL;
	MlnBuffer *digestValue = NULL;
	MlnOctopusReference *reference = NULL;
	int ret;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "Uid", MLN_OCTOPUS_NAMESPACE, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto format;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, list->nodes[0], &uid);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "Digest", MLN_OCTOPUS_NAMESPACE, &list) == 0) {
		if (list->count != 1)
			goto format;
		ret = MlnImpl_8103D628(list->nodes[0], &transforms, &digestMethod, &digestValue);
		if (ret != 0)
			goto out;
	}
	reference = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*reference));
	if (reference == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	reference->uid = uid;
	reference->transforms = transforms;
	reference->digestMethod = digestMethod;
	reference->digestValue = digestValue;
	uid = NULL;
	transforms = NULL;
	digestMethod = NULL;
	digestValue = NULL;
	*out = reference;
	reference = NULL;
	goto out;
format:
	ret = MLN_OCTOPUS_ERROR_FORMAT;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
	MlnBufferDestroy(MLN_HEAP_GENERAL, digestMethod);
	MlnBufferDestroy(MLN_HEAP_GENERAL, digestValue);
	MlnImpl_8103C028(reference);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnImpl_8103C57C(transforms, MLN_OCTOPUS_ARRAY_TRANSFORMS);
	return ret;
}
