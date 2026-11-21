/*
 * The library's interface: the protection information of a Marlin MP4 track, starting and
 * stopping the client, and the calls that pass through to the client's services.
 */
#include <pfsmgr/pfsmgr.h>
#include <appmgr/appmgr.h>
#include <kernel/openpsid.h>
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
static unsigned char s_serviceSession[12];
static unsigned char s_transferSession[12];

static MlnServiceContext s_serviceContext = { -1, s_serviceSession };
static MlnServiceContext s_transferContext = { -1, s_transferSession };

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
	case MLN_XML_ERROR_SYNTAX:
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
int MlnApi_810023F4(void *a0, int a1)
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

__attribute__((noinline)) int SceMarlin_9609E2DF(MlnBuffer *buffer)
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

__attribute__((noinline)) int SceMarlin_A43BC777(void *a0)
{
	return MlnImpl_81015F3C(a0);
}

__attribute__((noinline)) int SceMarlin_23838449(void *a0)
{
	return MlnImpl_81016034(a0);
}

__attribute__((noinline)) int SceMarlin_FB95B259(void *a0, void *a1, void *a2)
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

__attribute__((noinline)) int SceMarlin_92A6A48F(void *a0, void *a1, void *a2)
{
	return MlnImpl_810162F4(a0, a1, a2);
}

int SceMarlin_F450D554(void *a0, void *a1, void *a2)
{
	return MlnImpl_8101632C(a0, a1, a2);
}

__attribute__((noinline)) int SceMarlin_A2E65A7E(void *a0, void *a1, void *a2)
{
	return MlnImpl_81016364(a0, a1, a2);
}

__attribute__((noinline)) int MlnApi_810026B8(void *a0, void *a1)
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

int SceMarlin_2FD092BD(MlnRightsInfo *info)
{
	return MlnConvertResult(MlnRightsInfoFree(MLN_HEAP_SECURE, info));
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
int SceMarlin_D09A4419(unsigned int size, const void *xml, MlnBundleInfo **out)
{
	MlnXmlNode *document = NULL, *controller = NULL, *keyReference = NULL;
	MlnXmlNodeList *protectorList = NULL;
	MlnBuffer *uid = NULL, *contentKeyId = NULL;
	MlnProtector *protectors = NULL;
	MlnBundleInfo *info;
	MlnXmlNode *node;
	unsigned int count = 0, i;
	int ret;

	if (size == 0 || xml == NULL || out == NULL) {
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
	ret = MlnXmlParse(MLN_HEAP_SECURE, size, xml, &document);
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

int MlnApi_81003254(void *a0, void *a1, void *a2)
{
	return MlnImpl_81016A5C(a0, a1, a2);
}

int MlnApi_81003260(void *a0, void *a1)
{
	return MlnImpl_81016B4C(a0, a1);
}

/* moves a value's data into a new node at the end of a list */
__attribute__((noinline)) int MlnApi_8100326C(MlnApiNode **list, MlnApiValue **value)
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

static const MlnBuffer s_sneServiceProvider = {
	sizeof("urn:marlin:organization:sne:service-provider:1") - 1,
	(unsigned char *)"urn:marlin:organization:sne:service-provider:1"
};

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
	sne = s_sneServiceProvider;
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

__attribute__((noinline)) int MlnApi_8100370C(MlnApiObject *object, int status, MlnApiResult *result, MlnApiReport *report,
		    MlnBuffer **response)
{
	MlnApiValue *value;
	MlnHttpRequest *request;

	if (result != NULL) {
		value = result->value;
		request = result->request;
		if (value != NULL && value->type == 16) {
			MlnImpl_81006D14(MLN_HEAP_SECURE, object->data);
			object->data = value->data[2];
			value->data[2] = NULL;
		}
	} else {
		request = NULL;
		value = NULL;
	}
	if (object->kind->type != 1)
		return 3;
	return MlnImpl_81020094(object->session, status, value, request, report, response);
}

/* a time in minutes as a date */
__attribute__((noinline)) int MlnApi_810037B0(const int *minutes, MlnApiDate *date)
{
	MlnDateTime time;

	if (MlnTimeFromMinutes((const unsigned int *)minutes, &time) != MLN_OK)
		return MLN_RESULT_INVALID_ARGUMENT;
	MlnMemset(date, 0, sizeof(*date));
	date->year = time.year;
	date->month = time.month;
	date->day = time.day;
	date->hour = time.hour;
	date->minute = time.minute;
	date->second = time.second;
	date->microsecond = time.microsecond;
	return MLN_OK;
}

__attribute__((noinline)) int MlnApi_81003818(MlnApiObject *object, MlnBuffer **response, MlnApiResult **result)
{
	int ret;

	if (object == NULL || object->kind == NULL)
		return 3;
	if (object->kind->type == 0) {
		ret = MlnImpl_8101A030(object->engine, response, result);
	} else if (object->kind->type == 1) {
		if (object->kind->remote)
			return MlnImpl_8101E658(object->client, response, result);
		ret = MlnImpl_8101A030(object->engine, response, result);
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

__attribute__((noinline)) int MlnApi_810039A8(MlnApiObject *object, void *a1, void *a2, void *a3)
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

static inline void MlnRightsSummaryFree(MlnRightsSummary *summary)
{
	unsigned int i;

	if (summary == NULL)
		return;
	if (summary->idCount != 0) {
		for (i = 0; i < summary->idCount; i++)
			MlnFree(MLN_HEAP_SECURE, summary->ids[i]);
		MlnFree(MLN_HEAP_SECURE, summary->ids);
	}
	MlnFree(MLN_HEAP_SECURE, summary);
}

/* the time a license constraint gives, the later or the earlier of two by its kind */
static inline MlnDateTime *MlnRightsTime(unsigned int kind, MlnDateTime *const times[2], int pick)
{
	if (kind == 2 && MlnImpl_81081400(times[0], times[1]) == pick)
		return times[1];
	return times[0];
}

/*
 * Evaluates a license on the library's own session and sums up its rights: the IDs it
 * names, its validity in minutes, and two further limits.
 */
__attribute__((noinline)) int MlnApi_81003A9C(MlnBuffer *license, MlnRightsSummary **out)
{
	MlnRightsInfo *info = NULL;
	MlnRightsSummary *summary;
	void *time;
	int ret, i;

	if (license == NULL || license->size == 0 || license->data == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto out;
	}
	ret = SceMarlin_A43BC777(&s_serviceContext);
	if (ret != MLN_OK)
		goto out;
	ret = SceMarlin_FB95B259(&s_serviceContext, (void *)license->size, license->data);
	if (ret != MLN_OK)
		goto out;
	i = SceMarlin_92A6A48F(&s_serviceContext, NULL, &info);
	if (info != NULL) {
		MlnRightsInfoFree(MLN_HEAP_SECURE, info);
		info = NULL;
	}
	if (i != MLN_OK) {
		if (i != 29) {
			ret = i;
			goto out;
		}
		summary = NULL;
		goto done;
	}
	i = SceMarlin_A2E65A7E(&s_serviceContext, NULL, &info);
	if (i != MLN_OK) {
		ret = i;
		goto out;
	}
	if (info == NULL) {
		summary = NULL;
		goto done;
	}
	summary = MlnAllocate(MLN_HEAP_SECURE, sizeof(*summary));
	if (summary == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(summary, 0, sizeof(*summary));
	summary->notBefore = -1;
	summary->notAfter = -1;
	summary->notMoreThan = -1;
	summary->notLongerThan = -1;
	if (info->idFlags & 1) {
		summary->state = 0;
		summary->ids = MlnAllocate(MLN_HEAP_SECURE, info->idCount * sizeof(MlnBuffer *));
		if (summary->ids == NULL)
			goto nomem;
		MlnMemset(summary->ids, 0, info->idCount * sizeof(MlnBuffer *));
		summary->idCount = info->idCount;
		for (i = 0; i < info->idCount; i++) {
			summary->ids[i] = MlnBufferCreate(MLN_HEAP_SECURE, info->ids[i]->size,
							  info->ids[i]->data);
			if (summary->ids[i] == NULL)
				goto nomem;
		}
	}
	if (info->constraintFlags & MLN_RIGHTS_NOT_BEFORE) {
		time = MlnRightsTime(info->notBeforeKind, info->notBefore, -1);
		if (time == NULL || MlnImpl_81081498(time, &summary->notBefore) != 0)
			goto invalid;
	}
	if (info->constraintFlags & MLN_RIGHTS_NOT_AFTER) {
		time = MlnRightsTime(info->notAfterKind, info->notAfter, 1);
		if (time == NULL || MlnImpl_81081498(time, &summary->notAfter) != 0)
			goto invalid;
	}
	if (info->constraintFlags & MLN_RIGHTS_NOT_LONGER_THAN)
		summary->notLongerThan = info->notLongerThan;
	if (info->constraintFlags & MLN_RIGHTS_NOT_MORE_THAN)
		summary->notMoreThan = info->notMoreThan;
done:
	*out = summary;
out:
	MlnRightsInfoFree(MLN_HEAP_SECURE, info);
	SceMarlin_23838449(&s_serviceContext);
	return ret;
invalid:
	ret = MLN_RESULT_INVALID_ARGUMENT;
	goto fail;
nomem:
	ret = MLN_ERROR_NO_MEMORY;
fail:
	MlnRightsInfoFree(MLN_HEAP_SECURE, info);
	MlnRightsSummaryFree(summary);
	SceMarlin_23838449(&s_serviceContext);
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

/* releases an object's engine: the local engine, or the remote client's */
static inline void MlnApiObjectReleaseEngine(MlnApiObject *object)
{
	if (object->kind == NULL)
		return;
	if (object->kind->type == 0 || (object->kind->type == 1 && !object->kind->remote)) {
		MlnImpl_8101A020(object->engine);
		object->engine = NULL;
	} else if (object->kind->type == 1) {
		MlnImpl_8101E628(object->client);
	}
}

/* closes an object's engine and frees the object */
static inline void MlnApiObjectDestroy(MlnApiObject *object)
{
	if (object == NULL)
		return;
	if (object->kind != NULL) {
		if (object->kind->type == 1) {
			if (object->kind->remote) {
				MlnImpl_8101DF70(object->client);
				goto free;
			}
			MlnImpl_8101F92C(object->session);
		} else if (object->kind->type != 0) {
			goto free;
		}
		MlnApiObjectReleaseEngine(object);
	}
free:
	MlnImpl_81006D14(MLN_HEAP_SECURE, object->data);
	MlnFree(MLN_HEAP_SECURE, object);
}

/* creates an object on the engine its kind names; a remote client also reports its version */
int SceMarlin_9CD4CE1E(MlnApiKind *kind, MlnApiObject **out, MlnApiStatus **status)
{
	MlnApiVersion version = { 0 };
	MlnApiObject *object;
	MlnApiStatus *report;
	int ret;

	if (kind == NULL || out == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto out;
	}
	object = MlnAllocate(MLN_HEAP_SECURE, sizeof(*object));
	if (object == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(object, 0, sizeof(*object));
	object->kind = kind;
	object->status = 0;
	if (kind->type == 0) {
		object->engine = NULL;
	} else if (kind->type != 1) {
		ret = 6;
		goto fail;
	} else if (kind->address != NULL && kind->address->size != 0 && kind->address->data == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto fail;
	} else if (kind->remote) {
		ret = MlnImpl_8101DD24(kind, kind->address, &object->client, &version);
		if (ret != MLN_OK)
			goto fail;
		if (status != NULL) {
			report = MlnAllocate(MLN_HEAP_SECURE, sizeof(*report));
			if (report == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				goto fail;
			}
			report->type = 1;
			report->major = version.major;
			report->minor = version.minor;
			report->name = version.name;
			report->build = version.build;
			version.name = NULL;
			report->id = version.id;
			version.id = NULL;
			*status = report;
		}
	} else {
		ret = MlnImpl_8101F870(kind, kind->address, &object->session);
		if (ret != MLN_OK)
			goto fail;
		object->engine = NULL;
	}
	*out = object;
	ret = MLN_OK;
	goto out;
fail:
	SceMarlin_9609E2DF(version.id);
	SceMarlin_9609E2DF(version.name);
	MlnApiObjectDestroy(object);
	return ret;
out:
	SceMarlin_9609E2DF(version.id);
	SceMarlin_9609E2DF(version.name);
	return ret;
}

int SceMarlin_8FEB4F94(MlnApiObject *object)
{
	MlnApiObjectDestroy(object);
	return MLN_OK;
}

int SceMarlin_87DF7B98(MlnApiStatus *status)
{
	if (status != NULL) {
		if (status->type != 1)
			return MLN_RESULT_FAILED;
		SceMarlin_9609E2DF(status->id);
		SceMarlin_9609E2DF(status->name);
		MlnFree(MLN_HEAP_SECURE, status);
	}
	return MLN_OK;
}

/*
 * Runs a transaction: each step of the engine either gives a value, added to the caller's
 * results, or a request, sent with the application's transport; its response feeds the next.
 */
int SceMarlin_88AC64AD(MlnApiObject *object, void *a1, MlnBuffer *a2, MlnBuffer *a3,
		       MlnTransport *transport, MlnApiNode **results)
{
	MlnBuffer *response = NULL;
	void *connection = NULL;
	MlnApiResult *step = NULL;
	MlnHttpRequest *request;
	MlnHttpHeader *header;
	unsigned int responseSize, i;
	void *responseData;
	char *url = NULL, *copy;
	int ret, post;

	if (object == NULL) {
		SceMarlin_9609E2DF(NULL);
		MlnImpl_81013420(step);
		return 3;
	}
	if (object->kind == NULL || (object->kind->type == 1 && !object->kind->remote)) {
		ret = 3;
		goto fail;
	}
	if (a1 == NULL || results == NULL || transport == NULL || transport->open == NULL ||
	    transport->close == NULL || transport->setHeader == NULL || transport->send == NULL ||
	    transport->release == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto fail;
	}
	if (a3 != NULL && a3->size != 0 && a3->data != NULL &&
	    (a2 == NULL || a2->size == 0 || a2->data == NULL)) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto fail;
	}
	ret = MlnApi_810039A8(object, a1, a2, a3);
	if (ret != MLN_OK)
		goto fail;
	for (;;) {
		ret = MlnApi_81003818(object, &response, &step);
		if (ret != MLN_OK)
			break;
		SceMarlin_9609E2DF(response);
		response = NULL;
		if (step->value != NULL) {
			ret = MlnApi_8100326C(results, &step->value);
			if (ret != MLN_OK)
				break;
			if (step->value->type == 16) {
				ret = 41;
				break;
			}
		}
		request = step->request;
		if (request == NULL) {
			if (step->value == NULL)
				break;
			goto next;
		}
		if (request->url == NULL || MlnStrlen(request->url) == 0) {
			ret = MLN_RESULT_FAILED;
			break;
		}
		if (url != NULL && MlnStrcmp(request->url, url) != 0) {
			MlnFree(MLN_HEAP_SECURE, url);
			if (transport->close == NULL) {
				ret = MLN_RESULT_INVALID_ARGUMENT;
				goto fail;
			}
			if (transport->close(connection) != 0) {
				ret = 85;
				goto fail;
			}
			url = NULL;
		}
		responseSize = 0;
		responseData = NULL;
		if (request->url == NULL || transport->open == NULL || transport->send == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto release;
		}
		if (transport->release == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			break;
		}
		if (url == NULL) {
			copy = MlnStrdup(MLN_HEAP_SECURE, request->url);
			if (copy == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				goto release;
			}
			if (transport->open(copy, &connection) != 0) {
				MlnFree(MLN_HEAP_SECURE, copy);
				ret = 85;
				response = NULL;
				goto release;
			}
			url = copy;
		}
		if (transport->setHeader == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto release;
		}
		if (request->headerCount != 0) {
			header = request->headers[0];
			if (header == NULL) {
				ret = 85;
				goto release;
			}
			for (i = 1; ; i++) {
				if (transport->setHeader(connection, header->name, header->value) != 0) {
					ret = 85;
					goto release;
				}
				if (i >= request->headerCount)
					break;
				header = request->headers[i];
				if (header == NULL) {
					ret = 85;
					goto release;
				}
			}
		}
		if (request->method == MLN_HTTP_GET) {
			post = 0;
		} else if (request->method == MLN_HTTP_POST) {
			post = 1;
		} else {
			ret = MLN_RESULT_FAILED;
			goto release;
		}
		if (transport->send(connection, post, request->body != NULL ? request->body->size : 0,
				    request->body != NULL ? request->body->data : NULL,
				    &responseSize, &responseData) != 0) {
			response = NULL;
			goto release;
		}
		response = MlnBufferCreate(MLN_HEAP_SECURE, responseSize, responseData);
		if (response == NULL)
			ret = MLN_ERROR_NO_MEMORY;
release:
		if (transport->release != NULL && responseData != NULL)
			transport->release(responseData);
		if (ret != MLN_OK)
			break;
next:
		MlnImpl_81013420(step);
		step = NULL;
	}
	SceMarlin_9609E2DF(response);
	MlnImpl_81013420(step);
	if (url != NULL) {
		if (transport->close != NULL)
			transport->close(connection);
		MlnFree(MLN_HEAP_SECURE, url);
	}
	goto out;
fail:
	SceMarlin_9609E2DF(response);
	MlnImpl_81013420(step);
out:
	MlnApiObjectReleaseEngine(object);
	return ret;
}

/* frees a list of results */
int SceMarlin_45B29121(MlnApiNode *list)
{
	MlnApiNode *next;

	if (list == NULL)
		return MLN_OK;
	do {
		next = list->next;
		SceMarlin_9609E2DF(list->data[0]);
		SceMarlin_9609E2DF(list->data[1]);
		switch (list->type) {
		case 1:
		case 4:
			SceMarlin_9609E2DF(list->data[2]);
			break;
		case 2:
		case 3:
			SceMarlin_9609E2DF(list->data[2]);
			SceMarlin_9609E2DF(list->data[3]);
			break;
		case 5:
		case 6:
			SceMarlin_9609E2DF(list->data[3]);
			break;
		case 16:
			MlnImpl_81006D14(MLN_HEAP_SECURE, list->data[2]);
			break;
		}
		MlnFree(MLN_HEAP_SECURE, list);
	} while ((list = next) != NULL);
	return MLN_OK;
}

/* the two messages a configuration request may come from */
static const MlnBuffer s_requestSources[2] = {
	{
		sizeof("http://source/ConfigurationToken.xml") - 1,
		(unsigned char *)"http://source/ConfigurationToken.xml"
	}, {
		sizeof("http://source/services/LicenseTransferService") - 1,
		(unsigned char *)"http://source/services/LicenseTransferService"
	}
};

static inline void MlnServiceContextInit(MlnServiceContext *context, unsigned int *session)
{
	session[0] = 0;
	session[1] = 0;
	session[2] = 0;
	context->id = -1;
	context->session = session;
}

/*
 * Processes a message from a remote engine: decodes the request it carries, runs it, and
 * gives the results and the engine's response to send back.
 */
int SceMarlin_0166ACC1(MlnApiObject *object, MlnBuffer *message, MlnApiNode **results,
		       MlnBuffer **output)
{
	MlnApiNode *list = NULL;
	MlnApiResult *step = NULL;
	MlnBuffer *response = NULL;
	MlnBuffer *reply = NULL;
	MlnRightsSummary *rights = NULL;
	MlnApiReport report;
	MlnApiRequest request, decoded;
	MlnServiceContext context;
	unsigned int session[3];
	MlnBuffer configurationToken, licenseTransferService;
	MlnBuffer *handle, *buffer;
	MlnApiResult *created;
	MlnApiValue *value;
	MlnApiKind *kind;
	int ret, status;

	MlnMemset(&report, 0, sizeof(report));
	MlnMemset(&request, 0, sizeof(request));
	if (object == NULL || object->kind == NULL) {
		ret = 3;
		goto fail;
	}
	if (message == NULL || message->size == 0 || message->data == NULL ||
	    results == NULL || output == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto fail;
	}
	MlnMemset(&decoded, 0, sizeof(decoded));
	if (object->kind->type == 1) {
		status = MlnImpl_8101F954(object->session, message, object->status, object->data,
					  &decoded);
		if (status == MLN_OK) {
			MlnImpl_81006D14(MLN_HEAP_SECURE, object->data);
			object->data = NULL;
			object->status = 0;
			request = decoded;
		}
	} else {
		status = 3;
	}

	switch (request.type) {
	case 0:
		if (status == MLN_OK) {
			status = MlnApi_810039A8(object, request.arg[0], request.arg[1], request.arg[2]);
			if (status == MLN_OK)
				status = MlnApi_81003818(object, &reply, &step);
		}
		ret = MlnApi_8100370C(object, status, step, NULL, &response);
		break;
	case 1:
		if (status == MLN_OK) {
			kind = object->kind;
			handle = NULL;
			if (kind != NULL && !kind->remote) {
				if ((unsigned int)kind->type < 2) {
					MlnImpl_8101A020(object->engine);
					object->engine = NULL;
				}
				status = MlnImpl_81019B44(1, NULL, NULL, NULL, 0, &object->engine);
				if (status == MLN_OK)
					status = MlnImpl_8101A59C(&handle);
				if (status == MLN_OK) {
					report.type = 0;
					report.data.buffer = handle;
					handle = NULL;
				} else if (status == 0x1f0001) {
					status = 82;
				}
			} else {
				status = 3;
			}
			SceMarlin_9609E2DF(handle);
		}
		ret = MlnApi_8100370C(object, status, NULL, &report, &response);
		break;
	case 2:
		if (request.arg[0] == NULL) {
			reply = request.arg[1];
			if (status == MLN_OK)
				status = MlnApi_81003818(object, &reply, &step);
			ret = MlnApi_8100370C(object, status, step, NULL, &response);
			break;
		}
		if (status != MLN_OK)
			goto results;
		configurationToken = s_requestSources[0];
		licenseTransferService = s_requestSources[1];
		if (MlnBufferCompare(MLN_HEAP_SECURE, request.arg[0], &configurationToken) == 0) {
			status = MlnImpl_8101A360(object->engine, &step);
		} else if (MlnBufferCompare(MLN_HEAP_SECURE, request.arg[0],
					    &licenseTransferService) != 0) {
			status = 6;
		} else {
			reply = request.arg[1];
			status = MlnApi_81003818(object, &reply, &step);
		}
		report.type = 0;
		if (step != NULL && step->request != NULL) {
			report.data.buffer = step->request->body;
			step->request->body = NULL;
		}
		MlnImpl_81013420(step);
		step = NULL;
		ret = MlnApi_8100370C(object, status, NULL, &report, &response);
		break;
	case 3:
		ret = 6;
		goto out;
	case 4:
		report.type = 3;
		if (status == MLN_OK) {
			status = MlnApi_81003A9C(request.arg[0], &rights);
			if (rights != NULL) {
				MlnMemcpy(&report.data.rights, rights, sizeof(report.data.rights));
				MlnFree(MLN_HEAP_SECURE, rights);
			}
		}
		ret = MlnApi_8100370C(object, status, step, &report, &response);
		break;
	case 5:
		response = request.arg[0];
		request.arg[0] = NULL;
		goto results;
	case 6:
		if (status == MLN_OK) {
			MlnServiceContextInit(&context, session);
			status = MlnImpl_81018E08(&context, request.arg[0], request.arg[1]);
		}
		ret = MlnApi_8100370C(object, status, NULL, NULL, &response);
		break;
	case 7:
		report.type = 1;
		if (status == MLN_OK) {
			MlnServiceContextInit(&context, session);
			status = MlnImpl_810188E8(&context, request.arg[0], request.arg[1],
						  &report.data.times.first,
						  &report.data.times.second);
		}
		ret = MlnApi_8100370C(object, status, NULL, &report, &response);
		break;
	case 8:
		report.type = 2;
		if (status == MLN_OK) {
			MlnServiceContextInit(&context, session);
			status = MlnImpl_81018A2C(&context, &report.data.items.count,
						  &report.data.items.items);
		}
		ret = MlnApi_8100370C(object, status, NULL, &report, &response);
		break;
	case 9:
		report.type = 4;
		if (status == MLN_OK) {
			MlnServiceContextInit(&context, session);
			status = MlnImpl_810147D8(&context, 1);
			if (status == MLN_OK) {
				MlnServiceContextInit(&context, session);
				status = MlnImpl_810147D8(&context, 2);
				if (status == MLN_OK) {
					MlnServiceContextInit(&context, session);
					status = MlnImpl_810147D8(&context, 0);
					if (status == 16)
						status = 15;
				}
			}
		}
		ret = MlnApi_8100370C(object, status, step, &report, &response);
		break;
	case 10:
		report.type = 5;
		if (status == MLN_OK) {
			handle = NULL;
			MlnServiceContextInit(&context, session);
			status = MlnImpl_81016A5C(&context, request.arg[0], &handle);
			if (status == MLN_OK) {
				report.data.buffer = handle;
				handle = NULL;
			}
			SceMarlin_9609E2DF(handle);
		}
		ret = MlnApi_8100370C(object, status, NULL, &report, &response);
		break;
	case 11:
		report.type = 6;
		if (status == MLN_OK)
			status = MlnApi_81003464(request.arg[0], request.arg[1],
						 &report.data.items.count, &report.data.items.items);
		ret = MlnApi_8100370C(object, status, NULL, &report, &response);
		break;
	case 12:
		report.type = 7;
		if (status == MLN_OK) {
			MlnServiceContextInit(&context, session);
			status = MlnImpl_81018D00(&context, &report.data.value);
		}
		ret = MlnApi_8100370C(object, status, NULL, &report, &response);
		break;
	case 13:
		report.type = 8;
		if (status == MLN_OK) {
			MlnServiceContextInit(&context, session);
			buffer = request.arg[0];
			if (buffer != NULL && buffer->size != 0 && buffer->data != NULL)
				status = MlnImpl_81018DA0(&context, buffer);
			else
				status = MLN_RESULT_INVALID_ARGUMENT;
		}
		ret = MlnApi_8100370C(object, status, NULL, NULL, &response);
		break;
	case 14:
		report.type = 9;
		if (status == MLN_OK) {
			buffer = request.arg[0];
			handle = NULL;
			if (buffer != NULL && buffer->size != 0 && buffer->data != NULL) {
				status = SceMarlin_A43BC777(&s_transferContext);
				if (status == MLN_OK)
					status = SceMarlin_FB95B259(&s_transferContext,
								    (void *)buffer->size, buffer->data);
				if (status == MLN_OK)
					status = MlnApi_810026B8(&s_transferContext, &handle);
				if (status == MLN_OK) {
					report.data.buffer = handle;
					handle = NULL;
				}
			} else {
				status = MLN_RESULT_INVALID_ARGUMENT;
			}
			SceMarlin_23838449(&s_transferContext);
		}
		ret = MlnApi_8100370C(object, status, step, &report, &response);
		break;
	case 15:
		if (status == MLN_OK) {
			value = NULL;
			created = MlnAllocate(MLN_HEAP_SECURE, sizeof(MlnApiValue));
			if (created != NULL) {
				MlnMemset(created, 0, sizeof(MlnApiValue));
				value = MlnAllocate(MLN_HEAP_SECURE, sizeof(MlnApiValue));
			}
			if (value != NULL) {
				MlnMemset(value, 0, sizeof(MlnApiValue));
				value->type = 6;
				value->data[2] = (void *)1;
				value->data[3] = request.arg[0];
				request.arg[0] = NULL;
				created->value = value;
				step = created;
				created = NULL;
			} else {
				status = MLN_ERROR_NO_MEMORY;
			}
			MlnFree(MLN_HEAP_SECURE, created);
			MlnFree(MLN_HEAP_SECURE, NULL);
		}
		ret = MlnApi_8100370C(object, status, step, NULL, &response);
		break;
	default:
		ret = MlnApi_8100370C(object, MLN_RESULT_FAILED, NULL, NULL, &response);
		break;
	}
	if (ret != MLN_OK)
		goto out;
results:
	if (step == NULL || step->value == NULL ||
	    (ret = MlnApi_8100326C(&list, &step->value)) == MLN_OK) {
		ret = MLN_OK;
		*results = list;
		*output = response;
		response = NULL;
	}
out:
	SceMarlin_9609E2DF(response);
	MlnImpl_81013420(step);
	MlnImpl_810134E0(&request);
	MlnImpl_8101359C(&report);
	if (status != MLN_OK)
		object->status = status;
	return ret;
fail:
	SceMarlin_9609E2DF(response);
	MlnImpl_81013420(step);
	MlnImpl_810134E0(&request);
	MlnImpl_8101359C(&report);
	return ret;
}

/* frees a list of pairs */
static inline void MlnApiPairsFree(MlnApiPair *list)
{
	MlnApiPair *next;

	if (list == NULL)
		return;
	do {
		next = list->next;
		SceMarlin_9609E2DF(list->key);
		SceMarlin_9609E2DF(list->value);
		MlnFree(MLN_HEAP_SECURE, list);
	} while ((list = next) != NULL);
}

/* makes a list of pairs of an engine's items, taking them out of the array */
static inline int MlnApiPairsCreate(MlnBuffer **items, unsigned int count, MlnApiPair **list)
{
	MlnApiPair *pair;
	int i;

	for (i = (int)(count >> 1) - 1; i >= 0; i--) {
		pair = MlnAllocate(MLN_HEAP_SECURE, sizeof(*pair));
		if (pair == NULL)
			return MLN_ERROR_NO_MEMORY;
		MlnMemset(pair, 0, sizeof(*pair));
		pair->next = *list;
		*list = pair;
		pair->key = items[2 * i];
		pair->value = items[2 * i + 1];
		items[2 * i] = NULL;
		items[2 * i + 1] = NULL;
	}
	return MLN_OK;
}

/* frees an engine's array of items */
static inline void MlnApiItemsFree(MlnBuffer **items, unsigned int count)
{
	unsigned int i;

	if (items == NULL)
		return;
	for (i = 0; i < count; i++)
		SceMarlin_9609E2DF(items[i]);
	MlnFree(MLN_HEAP_SECURE, items);
}

/* the pairs the engine holds */
int SceMarlin_08CAD267(MlnApiObject *object, MlnApiPair **out)
{
	MlnBuffer **items = NULL;
	unsigned int count = 0;
	MlnApiPair *list = NULL;
	MlnServiceContext context;
	unsigned int session[3];
	MlnApiKind *kind;
	int ret;

	if (object == NULL || (kind = object->kind) == NULL)
		return 3;
	if (kind->type == 1) {
		if (!kind->remote)
			return 3;
		if (out == NULL)
			return MLN_RESULT_INVALID_ARGUMENT;
	} else {
		if (out == NULL)
			return MLN_RESULT_INVALID_ARGUMENT;
		if (kind->type == 0) {
			MlnServiceContextInit(&context, session);
			ret = MlnImpl_81018A2C(&context, &count, &items);
			goto pairs;
		}
		if (kind->type != 1)
			return 6;
	}
	ret = MlnImpl_8101EC08(object->client, &count, &items);
pairs:
	if (ret == MLN_OK) {
		ret = MlnApiPairsCreate(items, count, &list);
		if (ret == MLN_OK) {
			*out = list;
			list = NULL;
		}
	}
	MlnApiItemsFree(items, count);
	MlnApiPairsFree(list);
	return ret;
}

/* the pairs a service provider gives for a token */
int SceMarlin_BCED6CC4(MlnApiObject *object, MlnBuffer *serviceProvider, MlnBuffer *token,
		       MlnApiPair **out)
{
	MlnBuffer **items = NULL;
	unsigned int count = 0;
	MlnApiPair *list = NULL;
	MlnApiKind *kind;
	int ret;

	if (object == NULL || (kind = object->kind) == NULL)
		return 3;
	if (kind->type == 1 && !kind->remote)
		return 3;
	if (serviceProvider == NULL || serviceProvider->size == 0 || serviceProvider->data == NULL ||
	    token == NULL || token->size == 0 || token->data == NULL || out == NULL)
		return MLN_RESULT_INVALID_ARGUMENT;
	if (kind->type == 0)
		ret = MlnApi_81003464(serviceProvider, token, &count, &items);
	else if (kind->type == 1)
		ret = MlnImpl_8101E118(object->client, serviceProvider, token, &count, &items);
	else
		return 6;
	if (ret == MLN_OK) {
		ret = MlnApiPairsCreate(items, count, &list);
		if (ret == MLN_OK) {
			*out = list;
			list = NULL;
		}
	}
	MlnApiItemsFree(items, count);
	MlnApiPairsFree(list);
	return ret;
}

/* frees a list of pairs */
int SceMarlin_CCA99803(MlnApiPair *list)
{
	MlnApiPairsFree(list);
	return MLN_OK;
}

/* the two infos the engine gives for a request */
int SceMarlin_3AA04BBF(MlnApiObject *object, void *a1, void *a2, MlnApiDate *first,
		       MlnApiDate *second)
{
	int firstFound = 0;
	int secondFound = 0;
	MlnServiceContext context;
	unsigned int session[3];
	MlnApiKind *kind;
	int ret;

	if (object == NULL || (kind = object->kind) == NULL)
		return 3;
	if (kind->type == 1) {
		if (!kind->remote)
			return 3;
		if (first == NULL || second == NULL)
			return MLN_RESULT_INVALID_ARGUMENT;
	} else {
		if (first == NULL || second == NULL)
			return MLN_RESULT_INVALID_ARGUMENT;
		if (kind->type == 0) {
			MlnServiceContextInit(&context, session);
			ret = MlnImpl_810188E8(&context, a1, a2, &firstFound, &secondFound);
			if (ret != MLN_OK)
				return ret;
			goto found;
		}
		if (kind->type != 1)
			return 6;
	}
	ret = MlnImpl_8101DFA0(object->client, a1, a2, &firstFound, &secondFound);
	if (ret != MLN_OK)
		return ret;
found:
	if (firstFound == 0)
		MlnMemset(first, 0, sizeof(*first));
	else if (MlnApi_810037B0(&firstFound, first) != MLN_OK)
		return MLN_RESULT_INVALID_ARGUMENT;
	if (secondFound == 0) {
		MlnMemset(second, 0, sizeof(*second));
		return MLN_OK;
	}
	if (MlnApi_810037B0(&secondFound, second) != MLN_OK)
		return MLN_RESULT_INVALID_ARGUMENT;
	return MLN_OK;
}

int SceMarlin_7E65734B(MlnApiObject *object, void *a1, void *a2)
{
	MlnServiceContext context;
	unsigned int session[3];
	MlnApiKind *kind;

	if (object == NULL || (kind = object->kind) == NULL)
		return 3;
	if (kind->type == 1) {
		if (!kind->remote)
			return 3;
	} else if (kind->type == 0) {
		MlnServiceContextInit(&context, session);
		return MlnImpl_81018E08(&context, a1, a2);
	} else if (kind->type != 1) {
		return 6;
	}
	return MlnImpl_8101F0B8(object->client, a1, a2);
}

static inline MlnOctopusLink *MlnOctopusLinkCreate(MlnBuffer *to)
{
	MlnOctopusLink *link;

	link = MlnAllocate(MLN_HEAP_SECURE, sizeof(*link));
	if (link == NULL)
		return NULL;
	MlnMemset(link, 0, sizeof(*link));
	link->to = to;
	return link;
}

static inline void MlnOctopusNodesFree(MlnOctopusNode *node)
{
	MlnOctopusNode *next;
	MlnOctopusLink *link, *following;

	if (node == NULL)
		return;
	do {
		for (link = node->links; link != NULL; link = following) {
			following = link->next;
			MlnFree(MLN_HEAP_SECURE, link);
		}
		next = node->next;
		MlnFree(MLN_HEAP_SECURE, node);
	} while ((node = next) != NULL);
}

/*
 * The IDs reachable from an ID through a list of links, each a pair (from, to): the links
 * are grouped into nodes by where they start, then walked.
 */
int SceMarlin_CCD2D17E(MlnApiPair *links, const MlnBuffer *id, MlnIdSet **out)
{
	MlnOctopusNode *nodes = NULL, *node;
	MlnOctopusLink **tail;
	MlnBuffer *from, *to;
	MlnIdSet *set = NULL;
	int ret;

	if (links == NULL || out == NULL)
		return MLN_RESULT_INVALID_ARGUMENT;
	do {
		to = links->value;
		if (to == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto fail;
		}
		from = links->key;
		for (node = nodes; node != NULL; node = node->next)
			if (MlnBufferEquals(node->id, from))
				break;
		if (node != NULL) {
			for (tail = &node->links; *tail != NULL; tail = &(*tail)->next)
				;
			*tail = MlnOctopusLinkCreate(to);
			if (*tail == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				goto fail;
			}
		} else {
			node = MlnAllocate(MLN_HEAP_SECURE, sizeof(*node));
			if (node == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				goto fail;
			}
			MlnMemset(node, 0, sizeof(*node));
			node->id = from;
			node->links = MlnOctopusLinkCreate(to);
			if (node->links == NULL) {
				MlnOctopusNodesFree(node);
				ret = MLN_ERROR_NO_MEMORY;
				goto fail;
			}
			node->next = nodes;
			nodes = node;
		}
	} while ((links = links->next) != NULL);
	ret = MlnNodesCollectReachable(nodes, id, &set);
	if (ret == MLN_OK)
		*out = set;
fail:
	MlnOctopusNodesFree(nodes);
	return ret;
}

static inline void MlnIdSetFree(MlnIdSet *set)
{
	MlnIdSet *next;

	if (set == NULL)
		return;
	do {
		next = set->next;
		SceMarlin_9609E2DF(set->id);
		MlnFree(MLN_HEAP_SECURE, set);
	} while ((set = next) != NULL);
}

/* frees a set of IDs */
int SceMarlin_7A7CCFD0(MlnIdSet *set)
{
	MlnIdSetFree(set);
	return MLN_OK;
}

/*
 * Runs a transaction between two objects, one on the local engine and one on a remote client:
 * each side's request is fed to the other until a step needs no more, whose value is the result.
 */
int SceMarlin_D7A21F7F(MlnApiObject *source, MlnApiObject *target, void *a2, int a3,
		       MlnApiNode **results)
{
	MlnApiNode *list = NULL;
	MlnApiResult *step = NULL;
	MlnApiResult *sourceStep, *targetStep;
	MlnHttpRequest *request;
	MlnBuffer *handle, *message, *body;
	int ret;

	if (source == NULL || source->kind == NULL ||
	    (source->kind->type == 1 && !source->kind->remote) ||
	    target == NULL || target->kind == NULL ||
	    (target->kind->type == 1 && !target->kind->remote)) {
		ret = 3;
		goto out;
	}
	if (a3 != 1 || results == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
		goto out;
	}
	if (source->kind->type == 0) {
		if (target->kind->type == 0) {
			ret = 3;
			goto out;
		}
		handle = NULL;
		if (a2 == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			SceMarlin_9609E2DF(handle);
			goto out;
		}
		MlnApiObjectReleaseEngine(source);
		MlnApiObjectReleaseEngine(target);
		ret = MlnImpl_81019B44(1, NULL, NULL, NULL, 0, &source->engine);
		if (ret == MLN_OK)
			ret = MlnImpl_8101A59C(&handle);
		if (ret != MLN_OK) {
			SceMarlin_9609E2DF(handle);
			goto out;
		}
		ret = MlnImpl_8101E380(target->client, handle, NULL, a2, 1);
		SceMarlin_9609E2DF(handle);
		if (ret != MLN_OK)
			goto out;
		do {
			sourceStep = NULL;
			targetStep = NULL;
			message = NULL;
			body = NULL;
			if (step != NULL && step->request != NULL)
				message = step->request->body;
			ret = MlnImpl_8101E658(target->client, &message, &targetStep);
			if (ret != MLN_OK)
				goto steps;
			MlnImpl_81013420(step);
			step = NULL;
			request = targetStep->request;
			if (request == NULL) {
				if (targetStep->value != NULL) {
					step = targetStep;
					targetStep = NULL;
				}
			} else {
				if (request->url != NULL &&
				    MlnStrcmp(request->url, (const char *)s_requestSources[0].data) == 0) {
					ret = MlnImpl_8101A360(source->engine, &sourceStep);
				} else {
					body = targetStep->request->body;
					ret = MlnImpl_8101A030(source->engine, &body, &sourceStep);
					if (ret == 0x1f0001)
						ret = 82;
				}
				if (ret != MLN_OK)
					goto steps;
				step = sourceStep;
				sourceStep = NULL;
			}
			MlnImpl_81013420(sourceStep);
			MlnImpl_81013420(targetStep);
		} while (step != NULL && step->request != NULL);
	} else {
		if (target->kind->type != 0) {
			ret = 3;
			goto out;
		}
		sourceStep = NULL;
		targetStep = NULL;
		MlnApiObjectReleaseEngine(source);
		MlnApiObjectReleaseEngine(target);
		ret = MlnImpl_8101E380(source->client, NULL, NULL, NULL, 0);
		if (ret == MLN_OK)
			ret = MlnImpl_8101E658(source->client, NULL, &sourceStep);
		if (ret == MLN_OK)
			ret = MlnImpl_81019B44(1, sourceStep->request->body, NULL, a2, 0,
					       &target->engine);
		if (ret == MLN_OK) {
			ret = MlnImpl_8101A030(target->engine, NULL, &targetStep);
			if (ret == 0x1f0001)
				ret = 82;
		}
		if (ret != MLN_OK)
			goto steps;
		step = targetStep;
		targetStep = NULL;
		MlnImpl_81013420(sourceStep);
		MlnImpl_81013420(targetStep);
		do {
			message = NULL;
			targetStep = NULL;
			ret = MlnImpl_8101E9EC(source->client, step, &message);
			if (ret == MLN_OK) {
				ret = MlnImpl_8101A030(target->engine, &message, &targetStep);
				if (ret == 0x1f0001)
					ret = 82;
			}
			if (ret != MLN_OK) {
				SceMarlin_9609E2DF(message);
				MlnImpl_81013420(targetStep);
				goto out;
			}
			MlnImpl_81013420(step);
			step = targetStep;
			targetStep = NULL;
			SceMarlin_9609E2DF(message);
			MlnImpl_81013420(targetStep);
		} while (step != NULL && step->request != NULL);
	}
	if (step != NULL && step->value != NULL) {
		ret = MlnApi_8100326C(&list, &step->value);
		if (ret != MLN_OK)
			goto out;
	}
	ret = MLN_OK;
	*results = list;
	list = NULL;
	goto out;
steps:
	MlnImpl_81013420(sourceStep);
	MlnImpl_81013420(targetStep);
out:
	MlnImpl_81013420(step);
	SceMarlin_45B29121(list);
	return ret;
}

int SceMarlin_E8276179(MlnApiObject *object, void *a1, MlnBuffer **out)
{
	MlnBuffer *found = NULL;
	MlnBuffer *handle;
	MlnServiceContext context;
	unsigned int session[3];
	MlnApiKind *kind;
	int ret;

	if (object == NULL || (kind = object->kind) == NULL) {
		ret = 3;
		goto out;
	}
	if (kind->type == 1) {
		if (!kind->remote) {
			ret = 3;
			goto out;
		}
		if (out == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto out;
		}
	} else {
		if (out == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto out;
		}
		if (kind->type == 0) {
			handle = NULL;
			MlnServiceContextInit(&context, session);
			ret = MlnImpl_81016A5C(&context, a1, &handle);
			if (ret == MLN_OK) {
				found = handle;
				handle = NULL;
			}
			SceMarlin_9609E2DF(handle);
			goto found;
		}
		if (kind->type != 1) {
			ret = 6;
			goto out;
		}
	}
	ret = MlnImpl_8101F1F0(object->client, a1, &found);
found:
	if (ret == MLN_OK) {
		*out = found;
		found = NULL;
		ret = MLN_OK;
	}
out:
	SceMarlin_9609E2DF(found);
	return ret;
}

/* the console's ID for the engine: every other byte of its Open PSID */
int SceMarlin_05CB30D5(MlnApiObject *object, void *a1, MlnBuffer **out)
{
	SceKernelOpenPsId openPsId;
	MlnBuffer *id = NULL;
	MlnBuffer *buffer;
	int ret;

	if (object == NULL || object->kind == NULL ||
	    (object->kind->type == 1 && !object->kind->remote)) {
		ret = 3;
	} else if (out == NULL) {
		ret = MLN_RESULT_INVALID_ARGUMENT;
	} else if (sceKernelGetOpenPsId(&openPsId) != 0) {
		ret = MLN_RESULT_FAILED;
	} else {
		buffer = MlnBufferCreate(MLN_HEAP_SECURE, 8, NULL);
		if (buffer == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
		} else {
			buffer->data[0] = openPsId.id[0];
			buffer->data[1] = openPsId.id[2];
			buffer->data[2] = openPsId.id[4];
			buffer->data[3] = openPsId.id[6];
			buffer->data[4] = openPsId.id[8];
			buffer->data[5] = openPsId.id[10];
			buffer->data[6] = openPsId.id[12];
			buffer->data[7] = openPsId.id[14];
			*out = buffer;
			ret = MLN_OK;
		}
	}
	SceMarlin_9609E2DF(id);
	return ret;
}

/* the rights a license grants */
int SceMarlin_CD5AEEF2(MlnApiObject *object, MlnBuffer *license, MlnApiRights **out)
{
	MlnRightsSummary *summary = NULL;
	MlnApiRights *rights = NULL;
	MlnApiKind *kind;
	unsigned int i;
	int ret;

	if (object == NULL || (kind = object->kind) == NULL) {
		ret = 3;
		goto out;
	}
	if (kind->type == 1) {
		if (!kind->remote) {
			ret = 3;
			goto out;
		}
		if (license == NULL || out == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto out;
		}
	} else {
		if (license == NULL || out == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto out;
		}
		if (kind->type == 0) {
			ret = MlnApi_81003A9C(license, &summary);
			goto summary;
		}
		if (kind->type != 1) {
			ret = 6;
			goto out;
		}
	}
	ret = MlnImpl_8101EE90(object->client, license, &summary);
summary:
	if (ret != MLN_OK)
		goto fail;
	if (summary != NULL) {
		rights = MlnAllocate(MLN_HEAP_SECURE, sizeof(*rights));
		if (rights == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto fail;
		}
		MlnMemset(rights, 0, sizeof(*rights));
		for (i = 0; i < summary->idCount; i++)
			MlnIdSetAdd(&rights->ids, summary->ids[i]);
		if (summary->notBefore != -1 &&
		    MlnApi_810037B0(&summary->notBefore, &rights->notBefore) != MLN_OK) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto fail;
		}
		if (summary->notAfter != -1 &&
		    MlnApi_810037B0(&summary->notAfter, &rights->notAfter) != MLN_OK) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto fail;
		}
		if (summary->notMoreThan != -1)
			rights->notMoreThan = summary->notMoreThan;
		if (summary->notLongerThan != -1)
			rights->notLongerThan = summary->notLongerThan;
	}
	*out = rights;
	rights = NULL;
	ret = MLN_OK;
fail:
	MlnRightsSummaryFree(summary);
out:
	MlnFree(MLN_HEAP_SECURE, rights);
	return ret;
}

/* frees the rights of a license */
int SceMarlin_10C0787D(MlnApiRights *rights)
{
	if (rights != NULL) {
		MlnIdSetFree(rights->ids);
		MlnFree(MLN_HEAP_SECURE, rights);
	}
	return MLN_OK;
}

int SceMarlin_236015FA(MlnApiObject *object)
{
	MlnServiceContext context;
	unsigned int session[3];
	MlnApiKind *kind;
	int unused = 0;
	int ret;

	if (object == NULL || (kind = object->kind) == NULL)
		return 3;
	if (kind->type == 1) {
		if (!kind->remote)
			return 3;
	} else if (kind->type == 0) {
		MlnServiceContextInit(&context, session);
		ret = MlnImpl_810147D8(&context, 1);
		if (ret != MLN_OK)
			return ret;
		MlnServiceContextInit(&context, session);
		ret = MlnImpl_810147D8(&context, 2);
		if (ret != MLN_OK)
			return ret;
		MlnServiceContextInit(&context, session);
		ret = MlnImpl_810147D8(&context, 0);
		if (ret == 16)
			return 15;
		return ret;
	} else if (kind->type != 1) {
		return 6;
	}
	return MlnImpl_8101F33C(object->client, &unused);
}

int SceMarlin_C4FF32EF(MlnApiObject *object, void **a1)
{
	MlnServiceContext context;
	unsigned int session[3];
	MlnApiKind *kind;

	if (object == NULL || (kind = object->kind) == NULL)
		return 3;
	if (kind->type == 1) {
		if (!kind->remote)
			return 3;
		if (a1 == NULL)
			return MLN_RESULT_INVALID_ARGUMENT;
	} else {
		if (a1 == NULL)
			return MLN_RESULT_INVALID_ARGUMENT;
		if (kind->type == 0) {
			MlnServiceContextInit(&context, session);
			return MlnImpl_81018D00(&context, a1);
		}
		if (kind->type != 1)
			return 6;
	}
	return MlnImpl_8101F4B0(object->client, a1);
}

int SceMarlin_14097094(MlnApiObject *object, MlnBuffer *a1)
{
	MlnServiceContext context;
	unsigned int session[3];
	MlnApiKind *kind;

	if (object == NULL || (kind = object->kind) == NULL)
		return 3;
	if (kind->type == 1 && !kind->remote)
		return 3;
	if (a1 == NULL || a1->size == 0 || a1->data == NULL)
		return MLN_RESULT_INVALID_ARGUMENT;
	if (kind->type == 0) {
		MlnServiceContextInit(&context, session);
		return MlnImpl_81018DA0(&context, a1);
	}
	if (kind->type == 1)
		return MlnImpl_8101F5F4(object->client, a1);
	return 6;
}

/* the value a token gives, read from the buffer the engine finds for it */
int SceMarlin_B477D615(MlnApiObject *object, MlnBuffer *token, MlnRightsInfo **out)
{
	MlnBuffer *found = NULL;
	MlnBuffer *handle;
	MlnApiKind *kind;
	unsigned int result;
	MlnRightsInfo *value = NULL;
	int ret;

	if (object == NULL || (kind = object->kind) == NULL) {
		ret = 3;
		goto out;
	}
	if (kind->type == 1) {
		if (!kind->remote) {
			ret = 3;
			goto out;
		}
		if (token == NULL || out == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto out;
		}
	} else {
		if (token == NULL || out == NULL) {
			ret = MLN_RESULT_INVALID_ARGUMENT;
			goto out;
		}
		if (kind->type == 0) {
			handle = NULL;
			if (token->size != 0 && token->data != NULL) {
				ret = SceMarlin_A43BC777(&s_transferContext);
				if (ret == MLN_OK)
					ret = SceMarlin_FB95B259(&s_transferContext,
								 (void *)token->size, token->data);
				if (ret == MLN_OK)
					ret = MlnApi_810026B8(&s_transferContext, &handle);
				if (ret == MLN_OK) {
					found = handle;
					handle = NULL;
				}
			} else {
				ret = MLN_RESULT_INVALID_ARGUMENT;
			}
			SceMarlin_23838449(&s_transferContext);
			goto found;
		}
		if (kind->type != 1) {
			ret = 6;
			goto out;
		}
	}
	ret = MlnImpl_8101F724(object->client, token, &found);
found:
	if (ret != MLN_OK || found == NULL)
		goto out;
	result = MlnRightsInfoParse(MLN_HEAP_SECURE, found->size, found->data, &value);
	switch (result) {
	case MLN_OK:
		*out = value;
		ret = MLN_OK;
		break;
	case 2:
		ret = 2;
		break;
	case 3:
	case MLN_XML_ERROR_SYNTAX:
	case 0x10010005:
	case 0x10010008:
	case 0x1001000a:
		ret = MLN_RESULT_INVALID_ARGUMENT;
		break;
	case 6:
		ret = MLN_RESULT_EXPIRED;
		break;
	case 0x30000001:
	case 0x30000002:
		ret = MLN_RESULT_NETWORK;
		break;
	default:
		ret = MLN_RESULT_FAILED;
		break;
	}
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, found);
	return ret;
}

int SceMarlin_39FF1549(MlnApiObject *object)
{
	if (object == NULL || object->kind == NULL || object->kind->type != 1 ||
	    object->kind->remote)
		return 3;
	return MlnImpl_8102045C(object->session);
}
