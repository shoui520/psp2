/*
 * What a core handle checks before it acts: the stored link of an id, the time limit of the
 * rights, and the buffer groups of a license request.
 */
#include "mln_internal.h"

#define MLN_STORE_LINK_TABLE	7	/* the store's table of link records */

/* the stored record of the link whose id digest is id (a 20-byte digest) */
int MlnImpl_810827B0(const unsigned char *id)
{
	MlnStoreLink *links = NULL;
	MlnStoreLink *found = NULL;
	MlnBuffer *record = NULL;
	MlnBuffer digest;
	int ret;

	digest.size = 20;
	digest.data = (unsigned char *)id;
	ret = MlnImpl_81049874(&links);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81049D54(links, &digest, &found);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8105DEDC(MLN_STORE_LINK_TABLE, found->key, &record);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, record);
	MlnImpl_81049918(links);
	return ret;
}

/*
 * the rights of the handle's content: when they limit how long it may be used, the
 * response's entry for the controller is looked up and its pending data written
 */
unsigned int MlnImpl_81082834(MlnCoreContext *context, MlnCoreHandle *core)
{
	MlnBuffer *rights = NULL;
	MlnRightsInfo *info;
	MlnCoreResponse *response;
	unsigned int ret;
	int limited;

	ret = MlnImpl_81062F20(context, 0, NULL, &rights);
	if (ret != 0 && ret != 0x2302000C) {
		ret = ret == MLN_ERROR_NO_MEMORY ? MLN_ERROR_NO_MEMORY : 23;
		goto out;
	}
	limited = 0;
	info = NULL;
	ret = 0;
	if (rights != NULL) {
		ret = MlnRightsInfoParse(MLN_HEAP_GENERAL, rights->size, rights->data, &info);
		if (ret == 0)
			limited = (info->constraintFlags & MLN_RIGHTS_NOT_LONGER_THAN) != 0;
	}
	MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	if (ret != 0) {
		ret = ret == MLN_ERROR_NO_MEMORY ? MLN_ERROR_NO_MEMORY : 7;
		goto out;
	}
	if (!limited)
		goto out;
	response = core->response;
	if (response == NULL || response->data == NULL) {
		ret = 69;
		goto out;
	}
	ret = MlnImpl_81069AE4(response, core->bundle->bundle->controller->uid);
	if (ret != 0) {
		ret = ret == MLN_ERROR_NO_MEMORY ? MLN_ERROR_NO_MEMORY : 7;
		goto out;
	}
	response = core->response;
	if (response == NULL || response->entryOffset == 0) {
		ret = 69;
		goto out;
	}
	if (response->pending != 0) {
		ret = MlnImpl_81045EE0(response->data);
		if (ret != 0)
			ret = ret == MLN_ERROR_NO_MEMORY ? MLN_ERROR_NO_MEMORY : 7;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
	return ret;
}

/* the buffer groups of a license request, with the store's errors as the API's */
unsigned int MlnImpl_8108299C(MlnBufferGroups **out)
{
	MlnBufferGroups *groups = NULL, *made;
	unsigned int ret;

	ret = MlnImpl_81064340(&groups);
	if (ret == 0) {
		made = groups;
		groups = NULL;
		*out = made;
		goto done;
	}
	switch (ret) {
	case MLN_ERROR_NO_MEMORY:
		break;
	case 6:
		ret = 68;
		break;
	case 0x230A0002:
		ret = 15;
		break;
	case 0x230A0004:
	case MLN_OCTOPUS_ERROR_FORMAT:
	case 0x23110004:
		ret = 9;
		break;
	default:
		ret = 7;
		break;
	}
done:
	MlnImpl_81042A88(groups);
	return ret;
}
