/*
 * The library's interface: the protection information of a Marlin MP4 track, starting and
 * stopping the client, and the calls that pass through to the client's services.
 */
#include <pfsmgr/pfsmgr.h>
#include <appmgr/appmgr.h>
#include "mln_internal.h"

/* the result codes of the interface */
#define MLN_RESULT_INVALID_ARGUMENT	4
#define MLN_RESULT_DATABASE		5
#define MLN_RESULT_FAILED		7
#define MLN_RESULT_NOT_INITIALIZED	8
#define MLN_RESULT_NETWORK		66
#define MLN_RESULT_EXPIRED		68
#define MLN_RESULT_BAD_FORMAT		0x101
#define MLN_RESULT_BAD_VERSION		0x102

/* the application data the client keeps its databases in */
#define MLN_APPDATA_MOUNT_ID		111

#define MLN_FOURCC(a, b, c, d)	((unsigned int)(a) << 24 | (b) << 16 | (c) << 8 | (d))
#define MLN_BE32(p)		((unsigned int)(p)[0] << 24 | (p)[1] << 16 | (p)[3] | (p)[2] << 8)

static int s_initCount;
static ScePfsRndDriveId s_mountPoint;

/*
 * The protection boxes of a track: in a 'sinf' with the scheme 'ACBC' 1.0, the 'schi' holds
 * the content ID ('8id '), the Octopus bundle ('8bdl', XML), and optionally the track's
 * security attributes ('satr') with their HMAC ('hmac').
 */
int SceMarlin_A11B25C7(const unsigned char *data, int size, MlnTrackProtection *info)
{
	const unsigned char *box, *child, *entry, *attr, *attrData;
	unsigned int fullBox;
	int remaining, content, left, childSize, childContent;
	int n, entrySize, entryContent, k, attrSize, attrContent;
	int found, seen, typeSeen, index;
	unsigned int type;

	if (data == NULL || info == NULL || size <= 0)
		return MLN_RESULT_INVALID_ARGUMENT;
	remaining = size;
	while (remaining > 0) {
		if (remaining < 8)
			return MLN_RESULT_BAD_FORMAT;
		box = data;
		if (MLN_BE32(box + 4) != MLN_FOURCC('s', 'i', 'n', 'f'))
			return MLN_RESULT_BAD_FORMAT;
		content = MLN_BE32(box) - 8;
		if ((unsigned int)content > (unsigned int)(remaining - 8))
			return MLN_RESULT_BAD_FORMAT;
		remaining = remaining - 8 - content;
		child = box + 8;
		left = content;
		found = 0;
		while (left > 0) {
			if (left < 8)
				return MLN_RESULT_BAD_FORMAT;
			childSize = MLN_BE32(child);
			left -= childSize;
			childContent = childSize - 8;
			type = MLN_BE32(child + 4);
			if (left < 0)
				return MLN_RESULT_BAD_FORMAT;
			if (type == MLN_FOURCC('s', 'c', 'h', 'i')) {
				if (found & 2)
					return MLN_RESULT_BAD_FORMAT;
				info->attributeCount = 0;
				info->bundleSize = 0;
				entry = child + 8;
				n = childContent;
				seen = 0;
				while (n > 0) {
					if (n < 8)
						return MLN_RESULT_BAD_FORMAT;
					entrySize = MLN_BE32(entry);
					n -= entrySize;
					entryContent = entrySize - 8;
					type = MLN_BE32(entry + 4);
					if (n < 0)
						return MLN_RESULT_BAD_FORMAT;
					switch (type) {
					case MLN_FOURCC('8', 'i', 'd', ' '):
						if (seen & 1)
							return MLN_RESULT_BAD_FORMAT;
						seen |= 1;
						info->contentId = entry + 8;
						info->contentIdSize = entryContent;
						entry = entry + 8 + entryContent;
						break;
					case MLN_FOURCC('8', 'b', 'd', 'l'):
						if ((seen & 2) || entryContent < 8 || entry[8] != 'x' ||
						    entry[9] != 'm' || entry[10] != 'l' || entry[11] != ' ' ||
						    entry[12] != 0 || entry[13] != 0 || entry[14] != 0 ||
						    entry[15] != 0)
							return MLN_RESULT_BAD_FORMAT;
						seen |= 2;
						info->bundle = entry + 16;
						entry = entry + 8 + entryContent;
						info->bundleSize = entryContent - 8;
						break;
					case MLN_FOURCC('h', 'm', 'a', 'c'):
						if ((seen & 8) || entryContent != 32)
							return MLN_RESULT_BAD_FORMAT;
						info->hmac = entry + 8;
						seen |= 8;
						entry += 40;
						break;
					case MLN_FOURCC('s', 'a', 't', 'r'):
						if (seen & 4)
							return MLN_RESULT_BAD_FORMAT;
						info->attributes = entry;
						info->attributesSize = entrySize;
						attr = entry + 8;
						k = entryContent;
						typeSeen = 0;
						while (k > 0) {
							if (k < 8)
								return MLN_RESULT_BAD_FORMAT;
							attrSize = MLN_BE32(attr);
							k -= attrSize;
							attrData = attr + 8;
							attrContent = attrSize - 8;
							type = MLN_BE32(attr + 4);
							attr = attrData + attrContent;
							if (k < 0)
								return MLN_RESULT_BAD_FORMAT;
							if (type == MLN_FOURCC('s', 't', 'y', 'p')) {
								if (typeSeen)
									return MLN_RESULT_BAD_FORMAT;
								typeSeen = 1;
								index = info->attributeCount++;
								if (index > 15)
									return MLN_RESULT_BAD_FORMAT;
								info->attribute[index].type = type;
								info->attribute[index].data = attrData;
								info->attribute[index].size = attrContent;
							}
						}
						seen |= 4;
						entry = attr;
						break;
					default:
						entry = entry + 8 + entryContent;
						break;
					}
				}
				if (!(seen & 1))
					return MLN_RESULT_BAD_FORMAT;
				if ((seen & 0xc) != 0xc && (seen & 0xc) != 0)
					return MLN_RESULT_BAD_FORMAT;
				child = entry;
				found |= 2;
			} else if (type == MLN_FOURCC('s', 'c', 'h', 'm')) {
				if ((found & 1) || childContent < 4)
					return MLN_RESULT_BAD_FORMAT;
				if (child[8] != 0)
					return MLN_RESULT_BAD_VERSION;
				__builtin_memcpy(&fullBox, child + 8, sizeof(fullBox));
				if ((fullBox & 0xffffff00) != 0 || childContent != 10)
					return MLN_RESULT_BAD_FORMAT;
				if (child[12] != 'A' || child[13] != 'C' || child[14] != 'B' ||
				    child[15] != 'C')
					goto next;
				if (child[16] != 1 || child[17] != 0)
					return MLN_RESULT_BAD_FORMAT;
				child += 18;
				found |= 1;
			} else {
				child = child + 8 + childContent;
			}
		}
		if (found != 3)
			return MLN_RESULT_BAD_FORMAT;
		return MLN_OK;
next:
		data = box + 8 + content;
	}
	return MLN_RESULT_BAD_FORMAT;
}

/* the interface's result for one of the client's */
int MlnConvertResult(unsigned int result)
{
	switch (result) {
	case MLN_OK:
	case 2:
		return result;
	case 3:
	case 0x10010003:
	case 0x10010005:
	case 0x10010008:
	case 0x1001000a:
		return MLN_RESULT_INVALID_ARGUMENT;
	case 6:
		return MLN_RESULT_EXPIRED;
	case 0x30000001:
	case 0x30000002:
		return MLN_RESULT_NETWORK;
	}
	if ((result & 0xffff0000) == 0x11060000)
		return MLN_RESULT_DATABASE;
	return MLN_RESULT_FAILED;
}

/* starts the client on its first call: mounts its application data, sets up its memory and
 * hands its saved state to the service */
int SceMarlin_83B44D33(void *memory, unsigned int memorySize, void *secureMemory,
		       unsigned int secureMemorySize)
{
	unsigned char session[12] = { 0 };
	MlnServiceContext context;
	MlnBuffer *state = NULL;
	int ret, first;

	first = s_initCount++ == 0;
	context.id = -1;
	context.session = session;
	if (!first)
		return MLN_OK;
	ret = sceAppMgrAppDataMount(MLN_APPDATA_MOUNT_ID, &s_mountPoint);
	if (ret != (int)0x80800003 && ret != 0)
		return MLN_RESULT_FAILED;
	ret = MlnConvertResult(MlnMemoryInit(MLN_HEAP_SECURE, secureMemory, secureMemorySize));
	if (ret == MLN_OK) {
		ret = MlnCoreInit(memory, memorySize);
		if (ret == MLN_OK) {
			ret = MlnPlatformInit();
			if (ret == MLN_OK) {
				ret = MlnConvertResult(MlnStateSerialize(&state));
				if (ret == MLN_OK) {
					ret = MlnServiceSetState(&context, state);
					if (ret == MLN_OK) {
						MlnBufferDestroy(MLN_HEAP_SECURE, state);
						return ret;
					}
				}
				if (state != NULL)
					MlnBufferDestroy(MLN_HEAP_SECURE, state);
				MlnCoreTerm();
			}
		}
		MlnMemoryTerm(MLN_HEAP_SECURE);
	}
	s_initCount--;
	return ret;
}

/* stops the client on the call that balances the first start */
int SceMarlin_C76DA6C0(void)
{
	unsigned char session[12] = { 0 };
	MlnServiceContext context;
	int ret;

	context.id = -1;
	context.session = session;
	if (s_initCount == 0)
		return MLN_RESULT_NOT_INITIALIZED;
	if (--s_initCount > 0)
		return MLN_OK;
	MlnServiceClearState(&context);
	ret = MlnConvertResult(MlnPlatformTerm());
	if (ret == MLN_OK)
		ret = MlnCoreTerm();
	MlnMemoryTerm(MLN_HEAP_SECURE);
	sceAppMgrUmount(s_mountPoint.drive_id);
	return ret;
}

/*
 * Calls that pass through to the client. PLACEHOLDER names: each MlnApi_/MlnImpl_ name is
 * replaced by a name of what it does when its callee is written.
 */
int MlnApi_810023F4(void *a0, void *a1)
{
	return MlnImpl_810147D8(a0, a1);
}

int MlnApi_81002400(void *a0, void *a1, void *a2, void *a3)
{
	return MlnImpl_81014860(a0, a1, a2, a3);
}

int MlnApi_8100240C(void *a0, void *a1, void *a2)
{
	return MlnImpl_810148F8(a0, a1, a2);
}

int SceMarlin_9609E2DF(MlnBuffer *buffer)
{
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return MLN_OK;
}

int MlnApi_81002430(void)
{
	MlnImpl_810136AC();
	return MLN_OK;
}

int MlnApi_81002440(void *a0, void *a1, void *a2, void *a3, unsigned char a4, void *a5)
{
	return MlnImpl_810136E0(a0, a1, a2, a3, a4, a5);
}

int SceMarlin_F6B7B9F4(void *a0, void *a1, void *a2, void *a3, void *a4)
{
	return MlnImpl_81014A10(a0, a1, a2, a3, a4);
}

int MlnApi_81002480(void *a0, void *a1)
{
	return MlnImpl_81014B8C(a0, a1);
}

int MlnApi_8100248C(void *a0, void *a1, void *a2)
{
	return MlnImpl_81014C20(a0, a1, a2);
}

int MlnApi_81002498(void *a0, void *a1, MlnBuffer **out)
{
	MlnBuffer *buffer = NULL;
	int ret;

	ret = MlnImpl_81013804(a0, a1, &buffer);
	if (ret == MLN_OK) {
		*out = buffer;
		buffer = NULL;
	}
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

int MlnApi_810024E4(void *a0, void *a1)
{
	return MlnImpl_81014D30(a0, a1);
}

int MlnApi_810024F0(void *a0, void *a1)
{
	return MlnImpl_81014DC4(a0, a1);
}

int MlnApi_810024FC(void *a0, void *a1, void *a2)
{
	return MlnImpl_81014E28(a0, a1, a2);
}

int MlnApi_81002508(void *a0, void *a1)
{
	return MlnImpl_81014F24(a0, a1);
}

int MlnApi_81002514(void *a0, void *a1)
{
	return MlnImpl_81014FA8(a0, a1);
}

int MlnApi_81002520(void *a0, void *a1, void *a2)
{
	return MlnImpl_8101503C(a0, a1, a2);
}

int MlnApi_8100252C(void *a0)
{
	return MlnImpl_81015168(a0);
}

int MlnApi_81002538(void *a0, void *a1, void *a2, void *a3, void *a4)
{
	return MlnImpl_8101521C(a0, a1, a2, a3, a4);
}

int MlnApi_81002554(void *a0, void *a1, void *a2)
{
	return MlnImpl_81015374(a0, a1, a2);
}

int MlnApi_81002560(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
{
	return MlnImpl_8101549C(a0, a1, a2, a3, a4, a5, a6);
}

int MlnApi_81002588(void *a0, void *a1, void *a2, void *a3)
{
	return MlnImpl_81015620(a0, a1, a2, a3);
}

int MlnApi_81002594(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5)
{
	return MlnImpl_81015798(a0, a1, a2, a3, a4, a5);
}

int MlnApi_810025B8(void *a0, void *a1, void *a2)
{
	return MlnImpl_810158FC(a0, a1, a2);
}

int MlnApi_810025C4(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6, void *a7)
{
	return MlnImpl_81015A24(a0, a1, a2, a3, a4, a5, a6, a7);
}

int MlnApi_810025F4(void *a0, void *a1, void *a2)
{
	return MlnImpl_81015BB0(a0, a1, a2);
}

int MlnApi_81002600(void *a0)
{
	MlnImpl_81006D14(MLN_HEAP_SECURE, a0);
	return MLN_OK;
}

int MlnApi_81002618(void *a0, void *a1)
{
	return MlnImpl_81015CD8(a0, a1);
}

int MlnApi_81002624(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
{
	return MlnImpl_81015DDC(a0, a1, a2, a3, a4, a5, a6);
}

int MlnApi_8100264C(void *a0, void *a1, void *a2)
{
	return MlnImpl_81015EA4(a0, a1, a2);
}

int SceMarlin_A43BC777(void *a0)
{
	return MlnImpl_81015F3C(a0);
}

int SceMarlin_23838449(void *a0)
{
	return MlnImpl_81016034(a0);
}

int SceMarlin_FB95B259(void *a0, void *a1, void *a2)
{
	return MlnImpl_810160E8(a0, a1, a2);
}

int SceMarlin_102593F5(void *a0, void *a1, void *a2)
{
	return MlnImpl_8101617C(a0, a1, a2);
}

int SceMarlin_49752AFE(void *a0, void *a1)
{
	return MlnImpl_81016270(a0, a1);
}

int SceMarlin_92A6A48F(void *a0, void *a1, void *a2)
{
	return MlnImpl_810162F4(a0, a1, a2);
}

int SceMarlin_F450D554(void *a0, void *a1, void *a2)
{
	return MlnImpl_8101632C(a0, a1, a2);
}

int SceMarlin_A2E65A7E(void *a0, void *a1, void *a2)
{
	return MlnImpl_81016364(a0, a1, a2);
}

int MlnApi_810026B8(void *a0, void *a1)
{
	return MlnImpl_810163A8(a0, a1);
}

int SceMarlin_778408A7(void *a0, void *a1, void *a2, void *a3)
{
	return MlnImpl_810163E4(a0, a1, a2, a3);
}

int MlnApi_810026D0(void *a0, void *a1, void *a2)
{
	return MlnImpl_810166AC(a0, a1, a2);
}

int SceMarlin_2FD092BD(void *a0)
{
	return MlnConvertResult(MlnImpl_81010068(MLN_HEAP_SECURE, a0));
}

int SceMarlin_2F1E4047(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5)
{
	return MlnImpl_81016844(a0, a1, a2, a3, a4, a5);
}

int SceMarlin_EB4A1345(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
{
	return MlnImpl_8101698C(a0, a1, a2, a3, a4, a5, a6);
}

#define MLN_OCTOPUS_NAMESPACE	"http://www.octopus-drm.com/profiles/base/1.0"

static void MlnContentTargetsFree(MlnContentTarget *targets, unsigned int count)
{
	unsigned int i;

	if (targets == NULL)
		return;
	for (i = 0; i != count; i++)
		MlnFree(MLN_HEAP_SECURE, targets[i].data);
	MlnFree(MLN_HEAP_SECURE, targets);
}

static void MlnProtectorsFree(MlnProtector *protectors, unsigned int count)
{
	unsigned int i;

	if (protectors == NULL)
		return;
	for (i = 0; i != count; i++) {
		if (protectors[i].contentKeyId != NULL)
			MlnBufferDestroy(MLN_HEAP_SECURE, protectors[i].contentKeyId);
		MlnContentTargetsFree(protectors[i].targets, protectors[i].targetCount);
	}
	MlnFree(MLN_HEAP_SECURE, protectors);
}

/* the IDs of the content a protector's key protects */
static int MlnProtectorReadTargets(MlnXmlNode *node, MlnProtector *protector)
{
	MlnXmlNodeList *list = NULL;
	MlnBuffer *uid = NULL;
	MlnContentTarget *targets = NULL;
	unsigned int count = 0, i;
	int ret;

	if (node == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto out;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_SECURE, node, "ProtectedTargets/ContentReference/Uid",
			      MLN_OCTOPUS_NAMESPACE, &list);
	if (ret != MLN_OK) {
		ret = MlnConvertResult(ret);
		goto fail;
	}
	count = list->count;
	if (count == 0) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto fail;
	}
	targets = MlnAllocate(MLN_HEAP_SECURE, count * sizeof(*targets));
	if (targets == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto fail;
	}
	for (i = 0; i != count; i++) {
		targets[i].data = NULL;
		targets[i].size = 0;
	}
	for (i = 0; i != count; i++) {
		ret = MlnXmlGetText(MLN_HEAP_SECURE, list->nodes[i], &uid);
		if (ret != MLN_OK) {
			ret = MlnConvertResult(ret);
			goto fail;
		}
		targets[i].size = uid->size;
		targets[i].data = MlnAllocate(MLN_HEAP_SECURE, uid->size);
		if (targets[i].data == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto fail;
		}
		MlnMemcpy(targets[i].data, uid->data, uid->size);
		MlnBufferDestroy(MLN_HEAP_SECURE, uid);
		uid = NULL;
	}
	protector->targetCount = count;
	protector->targets = targets;
	goto out;
fail:
	if (uid != NULL)
		MlnBufferDestroy(MLN_HEAP_SECURE, uid);
	MlnContentTargetsFree(targets, count);
out:
	MlnXmlListFree(MLN_HEAP_SECURE, list);
	return ret;
}

/* the controller and the protectors of an Octopus bundle */
int SceMarlin_D09A4419(const void *xml, unsigned int size, MlnBundleInfo **out)
{
	MlnXmlNode *document = NULL, *controller = NULL, *keyReference = NULL;
	MlnXmlNodeList *protectorList = NULL;
	MlnBuffer *uid = NULL, *contentKeyId = NULL;
	MlnProtector *protectors = NULL;
	MlnBundleInfo *info;
	MlnXmlNode *node;
	unsigned int count = 0, i;
	int ret;

	if (xml == NULL || size == 0 || out == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto out;
	}
	info = MlnAllocate(MLN_HEAP_SECURE, sizeof(*info));
	if (info == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	info->controllerUid = NULL;
	info->protectorCount = 0;
	info->protectors = NULL;
	ret = MlnXmlParse(MLN_HEAP_SECURE, xml, size, &document);
	if (ret != MLN_OK) {
		ret = MlnConvertResult(ret);
		goto fail;
	}
	ret = MlnXmlFindNode(MLN_HEAP_SECURE, document, "/Bundle/Controller", MLN_OCTOPUS_NAMESPACE,
			     &controller);
	if (ret != MLN_OK) {
		ret = MlnConvertResult(ret);
		goto fail;
	}
	ret = MlnXmlGetAttribute(MLN_HEAP_SECURE, controller, NULL, "uid", &uid);
	if (ret != MLN_OK) {
		ret = MlnConvertResult(ret);
		goto fail;
	}
	info->controllerUid = uid;
	uid = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_SECURE, document, "/Bundle/Protector", MLN_OCTOPUS_NAMESPACE,
			      &protectorList);
	if (ret != MLN_OK) {
		ret = MlnConvertResult(ret);
		goto fail;
	}
	count = protectorList->count;
	if (count == 0) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto fail;
	}
	protectors = MlnAllocate(MLN_HEAP_SECURE, count * sizeof(*protectors));
	if (protectors == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto fail;
	}
	for (i = 0; i != count; i++) {
		protectors[i].contentKeyId = NULL;
		protectors[i].targetCount = 0;
		protectors[i].targets = NULL;
	}
	for (i = 0; i != count; i++) {
		node = protectorList->nodes[i];
		ret = MlnXmlFindNode(MLN_HEAP_SECURE, node, "ContentKeyReference/Uid",
				     MLN_OCTOPUS_NAMESPACE, &keyReference);
		if (ret == MLN_OK)
			ret = MlnXmlGetText(MLN_HEAP_SECURE, keyReference, &contentKeyId);
		if (ret != MLN_OK) {
			ret = MlnConvertResult(ret);
			goto fail;
		}
		protectors[i].contentKeyId = contentKeyId;
		contentKeyId = NULL;
		MlnXmlNodeFree(MLN_HEAP_SECURE, keyReference);
		keyReference = NULL;
		ret = MlnProtectorReadTargets(node, &protectors[i]);
		if (ret != MLN_OK)
			goto fail;
	}
	info->protectorCount = count;
	info->protectors = protectors;
	*out = info;
	protectors = NULL;
	goto out;
fail:
	SceMarlin_73C12840(info);
out:
	MlnXmlNodeFree(MLN_HEAP_SECURE, document);
	MlnXmlNodeFree(MLN_HEAP_SECURE, controller);
	MlnBufferDestroy(MLN_HEAP_SECURE, uid);
	MlnXmlListFree(MLN_HEAP_SECURE, protectorList);
	if (keyReference != NULL)
		MlnXmlNodeFree(MLN_HEAP_SECURE, keyReference);
	if (contentKeyId != NULL)
		MlnBufferDestroy(MLN_HEAP_SECURE, contentKeyId);
	MlnProtectorsFree(protectors, count);
	return ret;
}

int SceMarlin_73C12840(MlnBundleInfo *info)
{
	if (info != NULL) {
		MlnProtectorsFree(info->protectors, info->protectorCount);
		if (info->controllerUid != NULL)
			MlnBufferDestroy(MLN_HEAP_SECURE, info->controllerUid);
		MlnFree(MLN_HEAP_SECURE, info);
	}
	return MLN_OK;
}

int MlnApi_81003248(void *a0, void *a1)
{
	return MlnImpl_81013350(a0, a1);
}

int MlnApi_81003254(void *a0, void *a1)
{
	return MlnImpl_81016A5C(a0, a1);
}

int MlnApi_81003260(void *a0, void *a1)
{
	return MlnImpl_81016B4C(a0, a1);
}

/* moves a value's data into a new node at the end of a list */
int MlnApi_8100326C(MlnApiNode **list, MlnApiValue **value)
{
	MlnApiNode *node = NULL, *last;
	int ret;

	if (list == NULL || value == NULL || *value == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto out;
	}
	node = MlnAllocate(MLN_HEAP_SECURE, sizeof(*node));
	if (node == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	node->next = NULL;
	node->data[0] = (*value)->data[0];
	node->data[1] = (*value)->data[1];
	(*value)->data[0] = NULL;
	(*value)->data[1] = NULL;
	switch ((*value)->type) {
	case 1:
		node->type = 1;
		node->data[2] = (*value)->data[2];
		(*value)->data[2] = NULL;
		break;
	case 2:
		node->type = 2;
		node->data[2] = (*value)->data[2];
		node->data[3] = (*value)->data[3];
		(*value)->data[2] = NULL;
		(*value)->data[3] = NULL;
		break;
	case 3:
		node->type = 3;
		node->data[2] = (*value)->data[2];
		node->data[3] = (*value)->data[3];
		(*value)->data[2] = NULL;
		(*value)->data[3] = NULL;
		break;
	case 4:
		node->type = 4;
		node->data[2] = (*value)->data[2];
		(*value)->data[2] = NULL;
		break;
	case 5:
		node->type = 5;
		node->data[2] = (*value)->data[2];
		node->data[3] = (*value)->data[3];
		(*value)->data[3] = NULL;
		break;
	case 6:
		node->type = 6;
		node->data[2] = (*value)->data[2];
		node->data[3] = (*value)->data[3];
		(*value)->data[3] = NULL;
		break;
	case 16:
		node->type = 16;
		node->data[2] = (*value)->data[2];
		(*value)->data[2] = NULL;
		break;
	default:
		ret = MLN_RESULT_FAILED;
		goto out;
	}
	if (*list == NULL) {
		*list = node;
	} else {
		for (last = *list; last->next != NULL; last = last->next)
			;
		last->next = node;
	}
	node = NULL;
	ret = MLN_OK;
out:
	MlnFree(MLN_HEAP_SECURE, node);
	return ret;
}

static const char s_sneServiceProvider[] = "urn:marlin:organization:sne:service-provider:1";

/*
 * Asks the service for a service provider's items. For Sony's own service provider the token
 * carries an account ID ('ACID') and an entitlement ID ('ETID') in boxes.
 */
int MlnApi_81003464(MlnBuffer *serviceProvider, MlnBuffer *token, unsigned int *count,
		    MlnBuffer ***items)
{
	unsigned char session[12] = { 0 };
	MlnServiceContext context;
	MlnBuffer sne;
	MlnBuffer *ids = NULL, *accountId = NULL, *entitlementId = NULL;
	MlnBuffer **found = NULL;
	unsigned int foundCount = 0, total, boxSize, boxType, i;
	const unsigned char *box;
	int ret;

	context.id = -1;
	context.session = session;
	if (serviceProvider == NULL || serviceProvider->size == 0 || serviceProvider->data == NULL ||
	    token == NULL || token->size == 0 || token->data == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto out;
	}
	sne.size = sizeof(s_sneServiceProvider) - 1;
	sne.data = (unsigned char *)s_sneServiceProvider;
	if (MlnBufferCompare(MLN_HEAP_SECURE, serviceProvider, &sne) == 0 && token->size != 0 &&
	    token->data != NULL) {
		box = token->data;
		total = 0;
		do {
			MlnMemcpy(&boxSize, box, sizeof(boxSize));
			boxSize = MlnNtohl(boxSize);
			total += boxSize;
			if (total > token->size || boxSize < 8) {
				ret = MLN_RESULT_INVALID_ARGUMENT;
				SceMarlin_9609E2DF(accountId);
				SceMarlin_9609E2DF(entitlementId);
				goto fail;
			}
			MlnMemcpy(&boxType, box + 4, sizeof(boxType));
			boxType = MlnNtohl(boxType);
			if (boxType == MLN_FOURCC('A', 'C', 'I', 'D')) {
				accountId = MlnBufferCreate(MLN_HEAP_SECURE, boxSize - 8, box + 8);
			} else if (boxType == MLN_FOURCC('E', 'T', 'I', 'D')) {
				entitlementId = MlnBufferCreate(MLN_HEAP_SECURE, boxSize - 8, box + 8);
			} else {
				ret = MLN_RESULT_INVALID_ARGUMENT;
				SceMarlin_9609E2DF(accountId);
				SceMarlin_9609E2DF(entitlementId);
				goto fail;
			}
			box += boxSize;
		} while (total < token->size);
		ret = MlnImpl_81013804(accountId, entitlementId, &ids);
		SceMarlin_9609E2DF(accountId);
		SceMarlin_9609E2DF(entitlementId);
		if (ret != MLN_OK)
			goto fail;
	} else {
		SceMarlin_9609E2DF(NULL);
		SceMarlin_9609E2DF(NULL);
	}
	ret = MlnImpl_81018B8C(&context, serviceProvider, token, ids, &foundCount, &found);
	if (ret == MLN_OK) {
		if (!(foundCount & 1)) {
			*count = foundCount;
			*items = found;
			found = NULL;
			goto out;
		}
		ret = MLN_RESULT_FAILED;
	}
fail:
	if (found != NULL) {
		for (i = 0; i < foundCount; i++)
			SceMarlin_9609E2DF(found[i]);
		MlnFree(MLN_HEAP_SECURE, found);
	}
out:
	SceMarlin_9609E2DF(ids);
	return ret;
}

int MlnApi_8100370C(MlnApiObject *object, void *a1, MlnApiResult *result, void *a3, void *a4)
{
	MlnApiValue *value;
	void *next;

	if (result != NULL) {
		value = result->value;
		next = result->next;
		if (value != NULL && value->type == 16) {
			MlnImpl_81006D14(MLN_HEAP_SECURE, object->data);
			object->data = value->data[2];
			value->data[2] = NULL;
		}
	} else {
		next = NULL;
		value = NULL;
	}
	if (object->kind->type != 1)
		return 3;
	return MlnImpl_81020094(object->session, a1, value, next, a3, a4);
}

int MlnApi_810037B0(void *a0, MlnApiInfo *info)
{
	MlnApiInfo found;

	if (MlnImpl_810072B0(a0, &found) != MLN_OK)
		return MLN_RESULT_INVALID_ARGUMENT;
	MlnMemset(info, 0, sizeof(*info));
	*info = found;
	return MLN_OK;
}

int MlnApi_81003818(MlnApiObject *object, void *a1, MlnApiResult **result)
{
	int ret;

	if (object == NULL || object->kind == NULL)
		return 3;
	if (object->kind->type == 0) {
		ret = MlnImpl_8101A030(object->engine);
	} else if (object->kind->type == 1) {
		if (object->kind->remote)
			return MlnImpl_8101E658(object->client);
		ret = MlnImpl_8101A030(object->engine);
		if (ret == MLN_OK) {
			if (*result != NULL && (*result)->value != NULL)
				return (*result)->value->type == 16 ? 41 : MLN_OK;
			return ret;
		}
	} else {
		return 3;
	}
	if (ret == 0x1f0001)
		return 82;
	return ret;
}

int MlnApi_810039A8(MlnApiObject *object, void *a1, void *a2, void *a3)
{
	int ret;

	if (object == NULL || object->kind == NULL)
		return 3;
	if (object->kind->type == 0) {
		MlnImpl_8101A020(object->engine);
		object->engine = NULL;
	} else if (object->kind->type == 1) {
		if (!object->kind->remote) {
			MlnImpl_8101A020(object->engine);
			object->engine = NULL;
		} else {
			MlnImpl_8101E628(object->client);
		}
	}
	if (object->kind->type == 0) {
		ret = MlnImpl_81019B44(0, a1, a2, a3, 0, &object->engine);
	} else if (object->kind->type == 1) {
		if (!object->kind->remote)
			ret = MlnImpl_81019B44(0, a1, a2, a3, 0, &object->engine);
		else
			ret = MlnImpl_8101E380(object->client, a1, a2, a3, 1);
	} else {
		return 3;
	}
	if (ret == 0x1f0001)
		return 82;
	return ret;
}

/* the same ID: both missing, or the same bytes */
static inline int MlnBufferEquals(const MlnBuffer *a, const MlnBuffer *b)
{
	if (a == NULL || b == NULL)
		return a == NULL && b == NULL;
	return MlnBytesCompare(MLN_HEAP_SECURE, a->size, a->data, b->size, b->data) == 0;
}

/* adds a copy of an ID to a set of IDs, unless the set has it */
int MlnIdSetAdd(MlnIdSet **set, const MlnBuffer *id)
{
	MlnIdSet *entry;
	int ret;

	if (id == NULL) {
		entry = NULL;
		ret = MLN_RESULT_FAILED;
		goto out;
	}
	for (entry = *set; entry != NULL; entry = entry->next) {
		if (entry->id != NULL && MlnBytesCompare(MLN_HEAP_SECURE, entry->id->size,
							 entry->id->data, id->size, id->data) == 0) {
			entry = NULL;
			ret = MLN_OK;
			goto out;
		}
	}
	entry = MlnAllocate(MLN_HEAP_SECURE, sizeof(*entry));
	if (entry == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(entry, 0, sizeof(*entry));
	entry->id = MlnBufferCreate(MLN_HEAP_SECURE, id->size, id->data);
	if (entry->id == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	if (*set != NULL)
		entry->next = *set;
	*set = entry;
	entry = NULL;
	ret = MLN_OK;
out:
	MlnFree(MLN_HEAP_SECURE, entry);
	return ret;
}

/* adds to a set the IDs of every node the links lead to from a node, the node's own excepted */
int MlnNodesCollectReachable(MlnOctopusNode *nodes, const MlnBuffer *id, MlnIdSet **set)
{
	MlnOctopusNode *node;
	MlnOctopusLink *link;
	MlnIdSet *entry, *next;
	int ret;

	if (nodes == NULL)
		return MLN_OK;
	for (node = nodes; !MlnBufferEquals(node->id, id); ) {
		node = node->next;
		if (node == NULL)
			return MLN_OK;
	}
	for (link = node->links; link != NULL; link = link->next) {
		for (entry = *set; entry != NULL; entry = entry->next) {
			if (MlnBufferEquals(entry->id, link->to))
				break;
		}
		if (entry != NULL)
			continue;
		ret = MlnIdSetAdd(set, link->to);
		if (ret != MLN_OK)
			goto fail;
		ret = MlnNodesCollectReachable(nodes, link->to, set);
		if (ret != MLN_OK)
			goto fail;
	}
	return MLN_OK;
fail:
	for (entry = *set; entry != NULL; entry = next) {
		next = entry->next;
		SceMarlin_9609E2DF(entry->id);
		MlnFree(MLN_HEAP_SECURE, entry);
	}
	*set = NULL;
	return ret;
}
