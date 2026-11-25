/*
 * mtp_wrapper.cpp: SceLibMtp module loader and startup/shutdown wrapper
 * (0x812BCA30 - 0x812BCEDB).
 */
#include <system/system.h>
#include "mtp_server_internal.h"

struct MtpLoadedModule {
	const char *path;
	SceUID moduleId;
};

static MtpLoadedModule s_mtpModules[1] = {
	{ "vs0:vsh/common/libmtp.suprx", -1 }
};						/* 0x81545B8C */

static int s_mtpWrapperInitialized = 0;		/* 0x81545B94 */

static int loadModule()
{
	for (int i = 0; i < 1; i++) {
		int status = 0;
		SceUID modId = sceKernelLoadStartModule(s_mtpModules[i].path, 0, NULL,
							0, NULL, &status);
		if (modId < 0) {
			sceClibPrintf("%s(%u) Error: sceKernelLoadStartModule %s Faild: 0x%x\n",
				      "loadModule", 58, s_mtpModules[i].path, modId);
			return modId;
		}
		if (status < 0) {
			sceClibPrintf("%s(%u) Error: Module Start %s Faild: 0x%x\n",
				      "loadModule", 62, s_mtpModules[i].path, status);
			return status;
		}
		s_mtpModules[i].moduleId = modId;
	}
	return 0;
}

static void unloadModule()
{
	for (int i = 0; i >= 0; i--) {
		SceUID modId = s_mtpModules[i].moduleId;
		if (modId != -1) {
			int status = 0;
			int ret = sceKernelStopUnloadModule(modId, 0, NULL, 0, NULL, &status);
			if (ret < 0)
				sceClibPrintf(
					"%s(%u) Error: sceKernelStopUnloadModule %s failed: 0x%x\n",
					"unloadModule", 78, s_mtpModules[i].path, ret);
			if (status < 0)
				sceClibPrintf("%s(%u) Error: Module Stop %s failed: 0x%x\n",
					      "unloadModule", 81, s_mtpModules[i].path, ret);
			s_mtpModules[i].moduleId = -1;
		}
	}
}

/* 0x812BCA30 */
int mtpWrapperInitialize(const SceMtpInitParam *param)
{
	if (s_mtpWrapperInitialized != 0)
		return MTP_SVC_ERROR_ALREADY_INITIALIZED;

	int ret = loadModule();
	if (ret < 0) {
		unloadModule();
		return ret;
	}

	SceMtpInitParam initCopy;
	const SceMtpInitParam *initPtr = NULL;
	if (param != NULL) {
		sce_paf_memset(&initCopy, 0, sizeof(initCopy));
		initCopy = *param;
		initPtr = &initCopy;
	}

	ret = sceMtpInit(initPtr);
	if (ret < 0) {
		sceClibPrintf("%s(%u) Error: sceMtpInit failed: 0x%x\n",
			      "mtpWrapperInitialize", 120, ret);
		unloadModule();
		return ret;
	}

	int ifType = 0;
	ret = sceMtpGetCurrentInterface(&ifType);
	if (ret < 0) {
		sceClibPrintf("%s(%u) Error: sceMtpGetCurrentInterface failed: 0x%x\n",
			      "mtpWrapperInitialize", 131, ret);
		unloadModule();
		return ret;
	}

	if (ifType == MTP_IF_TYPE_USB && paf::system::Model::GetType() == 1) {
		sceClibPrintf(
			"%s(%u) Warning: MTP_IF_TYPE_USB is not supported, MTP_IF_TYPE_TCPIP is used instead.\n",
			"mtpWrapperInitialize", 140);
		ifType = MTP_IF_TYPE_TCPIP;
	}

	MtpNetHost assignedHost;
	const MtpNetHost *hostPtr = NULL;
	if (ifType == MTP_IF_TYPE_TCPIP && SceLibMtp_ED13170C(&assignedHost) == 0)
		hostPtr = &assignedHost;

	ret = sceMtpStart(ifType, hostPtr, 0, 0);
	if (ret < 0) {
		sceClibPrintf("%s(%u) Error: sceMtpStart failed: 0x%x\n",
			      "mtpWrapperInitialize", 156, ret);
		sceMtpEnd();
		unloadModule();
		return ret;
	}

	s_mtpWrapperInitialized = 1;
	return ret;
}

/* 0x812BCDD8 */
int mtpWrapperFinalize(void)
{
	if (s_mtpWrapperInitialized == 0)
		return MTP_SVC_ERROR_NOT_INITIALIZED;

	int ret = sceMtpStop();
	if (ret < 0)
		sceClibPrintf("%s(%u) Error: sceMtpStop failed: 0x%x\n",
			      "mtpWrapperFinalize", 179, ret);

	ret = sceMtpEnd();
	if (ret < 0)
		sceClibPrintf("%s(%u) Error: sceMtpEnd failed: 0x%x\n",
			      "mtpWrapperFinalize", 184, ret);

	unloadModule();
	s_mtpWrapperInitialized = 0;
	return 0;
}
