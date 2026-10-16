#include <sce/libkernel_backends.h>
#include <psp2/types.h>

/* The opaque context records are copied and interpreted by GcAuthMgr. */
struct MsSaveBBMacContext;
struct MsSaveBBCipherContext;

struct MsSaveBBMacRequest {
	SceSize macSize;
	SceSize keySize;
	SceUInt reserved[2];
};

struct MsSaveBBCipherRequest {
	void *data;
	const void *key;
	SceSize dataSize;
	SceSize keySize;
	SceUInt reserved[2];
};


int SceLibKernel_1693032E(struct MsSaveBBMacContext *context, SceUInt mode)
{
	return _sceSblGcAuthMgrMsSaveBBMacInit(context, mode);
}

int sceSblGcAuthMgrMsSaveBBMacUpdate(struct MsSaveBBMacContext *context,
		const void *data, SceSize size)
{
	return _sceSblGcAuthMgrMsSaveBBMacUpdate(context, data, size);
}

int SceLibKernel_774AE3CB(struct MsSaveBBMacContext *context,
		void *mac, const void *key)
{
	struct MsSaveBBMacRequest request;

	request.macSize = 16;
	request.keySize = 16;
	return _sceSblGcAuthMgrMsSaveBBMacFinal(context, mac, key, &request);
}

int SceLibKernel_2E05B2DC(struct MsSaveBBCipherContext *context, SceUInt mode,
		SceUInt generationMode, void *data, const void *key)
{
	struct MsSaveBBCipherRequest request;

	request.data = data;
	request.key = key;
	request.dataSize = 16;
	request.keySize = 16;
	return _sceSblGcAuthMgrMsSaveBBCipherInit(context, mode, generationMode,
					       &request);
}

int SceLibKernel_56A59D4F(struct MsSaveBBCipherContext *context,
		void *data, SceSize size)
{
	return _sceSblGcAuthMgrMsSaveBBCipherUpdate(context, data, size);
}

int sceSblGcAuthMgrMsSaveBBCipherFinal(struct MsSaveBBCipherContext *context)
{
	return _sceSblGcAuthMgrMsSaveBBCipherFinal(context);
}

int sceSblGcAuthMgrAdhocBB160Shutdown(void)
{
	return _sceSblGcAuthMgrAdhocBB160Shutdown();
}

int sceSblGcAuthMgrAdhocBB224Shutdown(void)
{
	return _sceSblGcAuthMgrAdhocBB224Shutdown();
}
