/*
 * The API ("BASE:"): SceLibMtp's exports, each request a context begun here and ended by
 * mtpIfEndCommon, and the handler the task queue runs every MTP operation through.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include "mtp_internal.h"

#define MTP_TAG_BASE			"BASE:"

/* the handles of objects carry their storage in these bits */
#define MTP_HANDLE_STORAGE_MASK		0x78000000

/* the smallest buffer sceMtp's callers get */
#define MTP_BUFFER_MIN_SIZE		0x1000
#define MTP_BUFFER_ALIGN		0x40

/* a host registered before its details are known is looked for this often */
#define MTP_BASE_DISCOVERY_RETRIES	10
#define MTP_BASE_DISCOVERY_INTERVAL	500000

/* a host application that resumes an import by resending its missing part */
#define MTP_BASE_APPLICATION_PART_OF_OBJECT	2

static SceUInt64 s_resetTick;
SceLibMtpHostFuncs g_bgCopyServiceFuncs;

extern "C" {

int commandHandler(MtpOperation *command, MtpOperation *response);

int sceMtpStart(int ifType, const MtpNetHost *host, int unused, int run)
{
	return mtpCoreStart(ifType, host, commandHandler, run);
}

int sceMtpStop(void)
{
	return mtpCoreStop();
}

/* every operation of a session: the session's own three first, the rest once it is open */
int commandHandler(MtpOperation *command, MtpOperation *response)
{
	int ret;

	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, command, response);
	switch (command->code) {
	case 0x1001:
		return mtpCmdGetDeviceInfo(command, response);
	case 0x1002:
		return mtpCmdOpenSession(command, response);
	case 0x1003:
		return mtpCmdCloseSession(command, response);
	}
	if (!mtpCoreHasPendingOperation()) {
		mtpUtilSetMtpResponse(MTP_RESPONSE_SESSION_NOT_OPEN, NULL, 0, command, response);
		MTP_ERR(MTP_TAG_BASE, 82, "SessionOpened\n");
		goto failed;
	}
	if (command->code != 0x100D && command->code != 0x940D) {
		mtpCoreSetObjectTransferHandle(0);
		mtpCoreSetObjectTransferSize(0);
	}
	switch (command->code) {
	case 0x1004:
		ret = mtpCmdGetStorageIds(command, response);
		break;
	case 0x1005:
		ret = mtpCmdGetStorageInfo(command, response);
		break;
	case 0x1006:
		ret = mtpCmdGetNumObjects(command, response);
		break;
	case 0x1007:
		ret = mtpCmdGetObjectHandles(command, response);
		break;
	case 0x1008:
		ret = mtpCmdGetObjectInfo(command, response);
		break;
	case 0x1009:
		ret = mtpCmdGetObject(command, response);
		break;
	case 0x100C:
		ret = mtpCmdSendObjectInfo(command, response);
		break;
	case 0x100D:
		ret = mtpCmdSendObject(command, response);
		break;
	case 0x1014:
		ret = mtpCmdGetDevicePropDesc(command, response);
		break;
	case 0x1015:
		ret = mtpCmdGetDevicePropValue(command, response);
		break;
	case 0x1016:
		ret = mtpCmdSetDevicePropValue(command, response);
		break;
	case 0x101B:
		ret = mtpCmdGetPartialObject(command, response);
		break;
	case 0x9801:
		ret = mtpCmdGetObjectPropsSupported(command, response);
		break;
	case 0x9802:
		ret = mtpCmdGetObjectPropDesc(command, response);
		break;
	case 0x9803:
		ret = mtpCmdGetObjectPropValue(command, response);
		break;
	case 0x9804:
		ret = mtpCmdSetObjectPropValue(command, response);
		break;
	case 0x9810:
		ret = mtpCmdGetObjectReferences(command, response);
		break;
	case 0x9811:
		ret = mtpCmdSetObjectReferences(command, response);
		break;
	case 0x9805:
		ret = mtpCmdGetObjectPropList(command, response);
		break;
	case 0x9806:
		ret = mtpCmdSetObjectPropList(command, response);
		break;
	case 0x9407:
		ret = mtpCmdGetMarlinMsgResult(command, response);
		break;
	case 0x9408:
		ret = mtpCmdGetMarlinMsgInfo(command, response);
		break;
	case 0x9409:
		ret = mtpCmdGetMarlinMsg(command, response);
		break;
	case 0x940C:
		ret = mtpCmdSendMarlinMsgInfo(command, response);
		break;
	case 0x940D:
		ret = mtpCmdSendMarlinMsg(command, response);
		break;
	case 0x9511:
		return mtpCmdGetVitaInformation(command, response);
	case 0x9513:
		return mtpCmdSendNumOfObject(command, response);
	case 0x9514:
		return mtpCmdGetBrowseInfo(command, response);
	case 0x9515:
		return mtpCmdSendObjectMetadata(command, response);
	case 0x9516:
		return mtpCmdSendObjectThumbnail(command, response);
	case 0x9518:
		return mtpCmdReportResult(command, response);
	case 0x951C:
		return mtpCmdSendInitiatorInfo(command, response);
	case 0x951F:
		return mtpCmdHttpGetUrl(command, response);
	case 0x9520:
		return mtpCmdHttpSendData(command, response);
	case 0x9523:
		return mtpCmdSendSettingInfo(command, response);
	case 0x9524:
		return mtpCmdGetSettingInfo(command, response);
	case 0x9528:
		return mtpCmdGetObjectStatusRequest(command, response);
	case 0x9529:
		return mtpCmdHttpSendProperty(command, response);
	case 0x952A:
		return mtpCmdSendHostStatus(command, response);
	case 0x952B:
		return mtpCmdGetPartOfObjectRequest(command, response);
	case 0x952C:
		return mtpCmdSendPartOfObject(command, response);
	case 0x952E:
		return mtpCmdGetFsOperation(command, response);
	case 0x952F:
		return mtpCmdGetPartOfObject(command, response);
	case 0x9533:
		return mtpCmdSendStorageSize(command, response);
	case 0x9534:
		return mtpCmdGetTreateObjectRequest(command, response);
	case 0x9535:
		return mtpCmdSendCopyConfirmationInfo(command, response);
	case 0x9536:
		return mtpCmdGetObjectMetadataItemsRequest(command, response);
	case 0x9537:
		return mtpCmdGetCopyConfirmationInfoRequest(command, response);
	case 0x9538:
		return mtpCmdKeepAlive(command, response);
	case 0x9539:
		return mtpCmdGetNetOperationData(command, response);
	case 0x953A:
		return mtpCmdSendNetOperationData(command, response);
	case 0x953B:
		return mtpCmdGetCapabilityInfo(command, response);
	case 0x953C:
		return mtpCmdSendCapabilityInfo(command, response);
	case 0x953D:
		return mtpCmdGetSearchInfo(command, response);
	case 0x953E:
		return mtpCmdGetHandoverInfo(command, response);
	case 0x953F:
		return mtpCmdGetPartOfObjectSpecfiedByHost(command, response);
	case 0x9540:
		return mtpCmdCheckObjectExistence(command, response);
	case 0x9545:
		return mtpCmdSendNumOfFoundObject(command, response);
	case 0x9600:
		return mtpCmdGetChallenge(command, response);
	case 0x9601:
		return mtpCmdSendRtc(command, response);
	case 0x9602:
	case 0x9603:
		return mtpCmdSendActivation(command, response);
	case 0x9604:
		return mtpCmdSendLicense(command, response);
	case 0x9680:
		ret = mtpCmdSendMarlinResults(command, response);
		break;
	case 0x9681:
		ret = mtpCmdGetMarlinLicenseInfo(command, response);
		break;
	case 0x9682:
		ret = mtpCmdSendMarlinLicenseInfo(command, response);
		break;
	case 0x9683:
		ret = mtpCmdGetMarlinLicense(command, response);
		break;
	case 0x9684:
		ret = mtpCmdSendMarlinLicense(command, response);
		break;
	case 0x9685:
		ret = mtpCmdGetMarlinEventInfo(command, response);
		break;
	case 0x9686:
		ret = mtpCmdSetMarlinLicenseIdToContext(command, response);
		break;
	default:
		MTP_ERR(MTP_TAG_BASE, 281, "not supported operation=%0xx", command->code);
		mtpUtilSetMtpResponse(MTP_RESPONSE_OPERATION_NOT_SUPPORTED, NULL, 0, command,
				      response);
		goto failed;
	}
	return ret;

failed:
	return -2;
}

int sceMtpSuspend(void)
{
	int ret;

	ret = MtpIfStopDriver(MTP_IF_PORT_USB);
	if (ret < 0)
		MTP_ERR(MTP_TAG_BASE, 67, "MtpIfStopDriver failed: 0x%x\n", ret);
	return ret;
}

int sceMtpResume(void)
{
	int ret;

	ret = MtpIfStartDriver(MTP_IF_PORT_USB);
	if (ret < 0)
		MTP_ERR(MTP_TAG_BASE, 81, "MtpIfStartDriver failed: 0x%x\n", ret);
	return ret;
}

/* restart the port, remembering when */
int sceMtpReset(void)
{
	SceRtcTick tick;
	int ret;

	mtpCoreWaitSema();
	sceRtcGetCurrentTick(&tick);
	s_resetTick = tick.tick;
	ret = mtpDevStopStartPort();
	mtpCoreSignalSema();
	return ret;
}

/* when the port was last reset, once */
SceUInt64 SceLibMtp_38CBDF4A(void)
{
	SceUInt64 tick = s_resetTick;

	s_resetTick = 0;
	return tick;
}

int SceLibMtp_2EBE47B8(const char *name)
{
	MtpStorage *storage;
	int ret;

	storage = mtpStrgFindStorageByName(name);
	if (storage == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (storage->start == NULL || storage->info->started != 0) {
		if (storage->info->started != 0)
			return SCE_MTP_ERROR_ALREADY;
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = mtpStrgVTCreateMtpHandles(storage);
	if (ret < 0)
		return ret;
	return storage->start(storage);
}

int SceLibMtp_A824A205(const char *name)
{
	MtpStorage *storage;
	int ret;

	storage = mtpStrgFindStorageByName(name);
	if (storage == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (storage->stop != NULL && storage->info->started != 0) {
		ret = storage->stop(storage);
		if (ret < 0)
			return ret;
		return mtpStrgVTDestroyMtpHandles(storage);
	}
	if (storage->info->started != 0)
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	return SCE_MTP_ERROR_ALREADY;
}

void *SceLibMtp_DA13726B(SceSize size)
{
	SceSize allocSize = MTP_BUFFER_MIN_SIZE;

	if (size > MTP_BUFFER_MIN_SIZE)
		allocSize = size;
	return mtpUtilMallocAlign(MTP_BUFFER_ALIGN, allocSize);
}

void SceLibMtp_492421F7(void *buf)
{
	mtpUtilFree(buf);
}

int SceLibMtp_08C00AFC(int *progress)
{
	return mtpCoreGetPublicStatus(progress);
}

int sceMtpBeginGetNumOfObject(int arg1, int arg2, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_SEND_NUM_OF_OBJECT);
	if (ret < 0)
		return ret;
	ret = mtpContextGetNumOfObject(context, arg1, arg2);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_123CA37E(SceUInt32 contextId, void *result)
{
	return mtpIfEndCommon(contextId, result, NULL);
}

int sceMtpBeginGetObjectMetadata(int arg1, int arg2, int arg3, int arg4,
				 SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_SEND_OBJECT_METADATA);
	if (ret < 0)
		return ret;
	ret = mtpContextCreateProgress(context);
	if (ret < 0) {
		mtpContextDelete(context);
		return ret;
	}
	ret = mtpContextImportBrowsingData(context, arg1, arg2, arg3, arg4);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_4F285949(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int sceMtpBeginImportObject(int arg1, int arg2, int unused, SceUInt32 *contextId, int arg5,
			    int arg6)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_SEND_OBJECT);
	if (ret < 0)
		return ret;
	ret = mtpContextCreateProgress(context);
	if (ret < 0) {
		mtpContextDelete(context);
		return ret;
	}
	ret = mtpContextImportObjectData(context, arg1, arg2, arg5, arg6);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

/* the imported object's ID, by the handle the host gave it */
int sceMtpEndImportObject(SceUInt32 contextId, MtpObjectId *objectId)
{
	MtpContext *context;
	SceUInt32 handle;
	SceUInt32 uniqueId;
	int ret;

	context = NULL;
	ret = mtpContextGet(contextId, 0, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_BASE, 374, "GetContext %u failed: 0x%x\n", contextId, ret);
		return ret;
	}
	ret = mtpContextWaitResult(context, &handle);
	if (ret == 0) {
		ret = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
		if (ret != 0) {
			MTP_ERR(MTP_TAG_BASE, 388,
				"mtpStrgVTConvertMtpHandleToUniqueId failed: 0x%x, handle=0x%x\n",
				ret, handle);
		} else if (objectId != NULL) {
			objectId->uniqueId = uniqueId;
			objectId->storage = handle & MTP_HANDLE_STORAGE_MASK;
		}
	}
	mtpContextDelete(context);
	if (ret != SCE_MTP_ERROR_CANCELED)
		mtpCoreClearObjectImport();
	return ret;
}

int sceMtpBeginExportObjectWithCheck(SceUInt64 arg1, SceUInt64 arg2, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_EXPORT_OBJECT);
	if (ret < 0)
		return ret;
	ret = mtpContextCreateProgress(context);
	if (ret < 0) {
		mtpContextDelete(context);
		return ret;
	}
	ret = mtpContextExportObjectWithCheck(context, arg1, arg2);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_8F3DBF97(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, "ExportObject");
}

int sceMtpBeginCreateHostDir(int arg1, int arg2, int arg3, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_FS_OPERATION);
	if (ret < 0)
		return ret;
	ret = mtpContextRequestFsOperation(context, MTP_FS_OPERATION_CREATE_DIR, arg3, 0, arg1,
					   arg2);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_549279DC(SceUInt32 contextId, void *result)
{
	return mtpIfEndCommon(contextId, result, NULL);
}

int sceMtpBeginDeleteHostDir(int arg1, int arg2, int arg3, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_FS_OPERATION);
	if (ret < 0)
		return ret;
	ret = mtpContextRequestFsOperation(context, MTP_FS_OPERATION_DELETE_DIR, arg3, 0, arg1,
					   arg2);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_F2A1482E(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int sceMtpBeginMoveHostDir(int arg1, int arg2, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_FS_OPERATION);
	if (ret < 0)
		return ret;
	ret = mtpContextRequestFsOperation(context, MTP_FS_OPERATION_MOVE_DIR, arg1, arg2, 0, 0);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_6079A84A(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int sceMtpBeginCreateHostFile(int arg1, int arg2, int arg3, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_FS_OPERATION);
	if (ret < 0)
		return ret;
	ret = mtpContextRequestFsOperation(context, MTP_FS_OPERATION_CREATE_FILE, arg3, 0, arg1,
					   arg2);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_F195BEC6(SceUInt32 contextId, void *result)
{
	return mtpIfEndCommon(contextId, result, NULL);
}

int sceMtpBeginGetObjectStatus2(int arg1, int arg2, int arg3, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_OBJECT_STATUS);
	if (ret < 0)
		return ret;
	ret = mtpContextGetObjectStatus(context, arg1, arg2, arg3);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_7A229158(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int sceMtpBeginGetObjectThumbnail(int arg1, int arg2, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_SEND_OBJECT_THUMBNAIL);
	if (ret < 0)
		return ret;
	ret = mtpContextImportObjectThumbnail(context, arg1, arg2);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_709C09B8(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int sceMtpBeginDeleteObject(int arg1, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_DELETE_OBJECT);
	if (ret < 0)
		return ret;
	ret = mtpContextDeleteObject(context, arg1);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_F988D2EE(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

/* the one system setting there is */
#define MTP_SYSTEM_SETTING_DEFAULT	1

int sceMtpBeginSetSystemSetting(int setting, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (setting != MTP_SYSTEM_SETTING_DEFAULT)
		setting = MTP_SYSTEM_SETTING_DEFAULT;
	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_GET_SETTING_INFO);
	if (ret < 0)
		return ret;
	ret = mtpContextGetSystemSetting(context, setting);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_D5FB089F(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int SceLibMtp_CFA3A9DA(int arg1, int arg2, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_SEND_SETTING_INFO);
	if (ret < 0)
		return ret;
	ret = mtpContextSetSystemSetting(context, arg1, arg2);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_EAA968F4(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int sceMtpBeginGetNpAccountInfo(int arg1, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_NP_ACCOUNT_INFO);
	if (ret < 0)
		return ret;
	ret = mtpContextGetNpAccountInfo(context, arg1);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_DD5917EF(SceUInt32 contextId, void *result)
{
	return mtpIfEndCommon(contextId, result, NULL);
}

/* a read to the end (size -1) starts at the beginning */
int sceMtpBeginReadObjectWithOffset(int arg1, int unused, SceUInt64 offset, SceUInt64 size,
				    SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	context = NULL;
	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (size == (SceUInt64)-1 && offset != 0)
		offset = 0;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_PART_OF_OBJECT);
	if (ret < 0)
		return ret;
	ret = mtpContextGetPartOfObject(context, arg1, offset, size, 0);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_7FDDA203(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int sceMtpBeginWriteObjectWithOffset(int arg1, int unused, SceUInt64 offset, SceUInt64 size,
				     SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_SEND_PART_OF_OBJECT);
	if (ret < 0)
		return ret;
	ret = mtpContextSendPartOfObject(context, arg1, offset, size);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_C5E9C54E(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int sceMtpBeginGetHostStorageSize(int arg1, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_STORAGE_SIZE);
	if (ret < 0)
		return ret;
	ret = mtpContextGetHostStorageSize(context, arg1);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_4CAFB048(SceUInt32 contextId, void *result)
{
	return mtpIfEndCommon(contextId, result, NULL);
}

int sceMtpBeginCheckSameObjectExist(SceUInt64 arg1, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_COPY_CONFIRMATION_INFO);
	if (ret < 0)
		return ret;
	ret = mtpContextCompareObjectData(context, arg1);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

/* a host that has no such object answers with an error that is not one */
int SceLibMtp_905BB9E8(SceUInt32 contextId, void *result)
{
	int ret;

	ret = mtpIfEndCommon(contextId, result, NULL);
	if (ret == SCE_MTP_ERROR_RESPONSE_A022)
		return 0;
	return ret;
}

int sceMtpBeginGetTotalObjectSize(int arg1, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (arg1 == 0)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_OBJECT_METADATA_ITEMS);
	if (ret < 0)
		return ret;
	ret = mtpContextGetTotalObjectSize(context, arg1);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_4239328A(SceUInt32 contextId, SceUInt32 low, SceUInt32 high)
{
	SceUInt32 result[2];

	result[0] = low;
	result[1] = high;
	return mtpIfEndCommon(contextId, result, NULL);
}

int sceMtpBeginSpecifiedObjectMetadata(int arg1, int arg2, int arg3, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (arg3 == 0 || arg2 == 0)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_SPECIFIED_OBJECT_METADATA);
	if (ret < 0)
		return ret;
	ret = mtpContextGetSpecifiedObjectMetadata(context, arg1, arg2, arg3);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_D4A132BD(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int sceMtpBeginNpDrmGetRtc(int arg1, int arg2, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_NPDRM_RTC);
	if (ret < 0)
		return ret;
	ret = mtpContextNpDrmGetRtc(context, arg1, arg2);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_2D3DD76D(SceUInt32 contextId, void *arg2, void *arg3)
{
	void *result[2];

	if (arg2 == NULL || arg3 == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	result[0] = arg3;
	result[1] = arg2;
	return mtpIfEndCommon(contextId, result, NULL);
}

int sceMtpBeginNpDrmActivate(int arg1, int arg2, int arg3, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_NPDRM);
	if (ret < 0)
		return ret;
	ret = mtpContextNpDrmExecAuthentication(context, arg1, 1, arg2, arg3);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_48DE8672(SceUInt32 contextId, void *arg2, void *arg3, void *arg4, void *arg5)
{
	void *result[4];

	if (arg2 == NULL || arg3 == NULL || arg4 == NULL || arg5 == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	result[2] = arg4;
	result[3] = arg2;
	result[0] = arg5;
	result[1] = arg3;
	return mtpIfEndCommon(contextId, result, NULL);
}

int sceMtpBeginNpDrmDeactivate(int arg1, int arg2, int arg3, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_NPDRM);
	if (ret < 0)
		return ret;
	ret = mtpContextNpDrmExecAuthentication(context, arg1, 0, arg2, arg3);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_923FAB90(SceUInt32 contextId, void *arg2, void *arg3, void *arg4)
{
	void *result[4];

	if (arg2 == NULL || arg3 == NULL || arg4 == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	result[1] = arg2;
	result[2] = arg3;
	result[3] = NULL;
	result[0] = arg4;
	return mtpIfEndCommon(contextId, result, NULL);
}

int sceMtpBeginNpDrmGetLicense(int arg1, int arg2, int arg3, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_NPDRM);
	if (ret < 0)
		return ret;
	ret = mtpContextNpDrmGetLicense(context, arg1, arg2, arg3);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_D9EEE248(SceUInt32 contextId, void *arg2, void *arg3)
{
	void *result[2];

	if (arg2 == NULL || arg3 == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	result[0] = arg3;
	result[1] = arg2;
	return mtpIfEndCommon(contextId, result, NULL);
}

int SceLibMtp_F6C15FE8(MtpHostVersion *version)
{
	mtpStatusGetHostVersion(version);
	return 0;
}

int SceLibMtp_0497FA87(SceUInt32 contextId, MtpContextProgress *progress)
{
	if (progress == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	return mtpContextGetProgress(contextId, progress);
}

int SceLibMtp_C10B3D7C(int type, int arg2, int arg3, void *arg4, int *arg5)
{
	return mtpMarlinGetInfo(type, arg2, arg3, arg4, arg5);
}

int SceLibMtp_6211F119(int type, int arg2, char *arg3, int arg4)
{
	return mtpMarlinSetInfo(type, arg2, arg3, arg4);
}

int SceLibMtp_C12DE479(int arg1, int arg2, int arg3)
{
	return mtpExecMarlinActivation(0, arg1, arg2, arg3);
}

int SceLibMtp_24369599(int arg1, int arg2, int arg3)
{
	return mtpExecMarlinDeactivation(0, arg1, arg2, arg3);
}

int SceLibMtp_A55E6AB6(int arg1, int arg2, int arg3, int arg4)
{
	return mtpExecMarlinLicenseMove(0, arg1, arg2, arg3, arg4);
}

int SceLibMtp_0BC34F1F(int arg1, int arg2, int arg3, int arg4)
{
	return mtpExecMarlinLicenseCopy(0, arg1, arg2, arg3, arg4);
}

int SceLibMtp_542F3064(int arg1, int arg2, int arg3, int arg4)
{
	return mtpExecMarlinLicenseCheck(0, arg1, arg2, arg3, arg4);
}

int SceLibMtp_E35C1C83(int arg1, int arg2, int arg3)
{
	return mtpExecMarlinSharedInfoAcq(0, arg1, arg2, arg3);
}

int SceLibMtp_5120B22A(SceUInt32 events, MtpCallbackFunc func, void *arg)
{
	return mtpCallbackRegister(events, func, arg);
}

int SceLibMtp_39CEBDD1(SceUInt32 events, MtpCallbackFunc func)
{
	return mtpCallbackUnregister(events, func);
}

int sceMtpSetFinishCallback(SceUInt32 contextId, void (*handler)(void *arg), void *arg)
{
	MtpContext *context;
	int ret;

	context = NULL;
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_BASE, 1715, "GetContext %u failed: 0x%x\n", contextId, ret);
		return ret;
	}
	ret = mtpContextSetCancelHandler(context, handler, arg);
	mtpContextPutContext(context, 0);
	return ret;
}

int sceMtpCheckContextValid(SceUInt32 contextId)
{
	MtpContext *context;
	int ret;

	context = NULL;
	ret = mtpContextGet(contextId, 0, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_BASE, 1740, "GetContext %u failed: 0x%x\n", contextId, ret);
		return ret;
	}
	if (mtpContextInvalidateContext(context, 1) == SCE_MTP_ERROR_NOT_STOPPED)
		return 0;
	return SCE_MTP_ERROR_INVALID_ARGUMENT;
}

int SceLibMtp_81BCF297(const char *path, MtpObjectId *objectId)
{
	SceUInt32 handle;
	SceUInt32 uniqueId;
	int ret;

	if (path == NULL || objectId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	ret = mtpStrgRegisterObject(path, &handle);
	if (ret < 0)
		return ret;
	ret = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (ret < 0)
		return ret;
	objectId->uniqueId = uniqueId;
	objectId->storage = handle & MTP_HANDLE_STORAGE_MASK;
	return 0;
}

/* an object ID: its unique ID in the low word, its storage bits in the high word */
#define MTP_OBJECT_ID_UNIQUE(id)	((SceUInt32)(id))
#define MTP_OBJECT_ID_STORAGE(id)	((SceUInt32)((id) >> 32))

/* the logical storage of an object ID's storage bits */
#define MTP_OBJECT_STORAGE_ID(storage) \
	((((storage) >> 27 & 3) + 1) | ((((storage) >> 29 & 3) + 1) << 16))

int SceLibMtp_E2A9E59C(SceInt64 objectId)
{
	SceUInt32 uniqueId = MTP_OBJECT_ID_UNIQUE(objectId);
	SceUInt32 storageId = MTP_OBJECT_STORAGE_ID(MTP_OBJECT_ID_STORAGE(objectId));
	MtpLogicalStorage *logical;
	int ret;

	logical = NULL;
	ret = mtpStrgGetLogicalStorage(storageId, &logical);
	if (ret < 0)
		return ret;
	return mtpObjDeleteObjectData(logical, uniqueId);
}

int SceLibMtp_9310EEEF(SceInt64 objectId, void *data)
{
	SceUInt32 uniqueId = MTP_OBJECT_ID_UNIQUE(objectId);
	SceUInt32 storageId = MTP_OBJECT_STORAGE_ID(MTP_OBJECT_ID_STORAGE(objectId));
	MtpLogicalStorage *logical;
	int ret;

	if (data == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	logical = NULL;
	ret = mtpStrgGetLogicalStorage(storageId, &logical);
	if (ret < 0)
		return ret;
	return mtpObjGetObjectData(logical, uniqueId, data);
}

int SceLibMtp_6BB1C852(int arg)
{
	return mtpObjSetNotifyMode(arg);
}

int sceMtpGetBgTaskInfo(SceUInt32 *id, int *type, void *path)
{
	int ret;

	ret = mtpBgCopyGetTaskInfo(id, type, path);
	if (ret < 0)
		MTP_ERR(MTP_TAG_BASE, 1846, "sceMtpGetBgTaskInfo failed: 0x%x\n", ret);
	return ret;
}

int SceLibMtp_E107F90D(const char *path, MtpObjectId *objectId)
{
	SceUInt32 handle;
	SceUInt32 uniqueId;
	int ret;

	if (path == NULL || *path == '\0' || objectId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	ret = mtpStrgGetMtpHandleByPath(path, &handle);
	if (ret < 0)
		return ret;
	ret = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (ret < 0)
		return ret;
	objectId->uniqueId = uniqueId;
	objectId->storage = handle & MTP_HANDLE_STORAGE_MASK;
	return 0;
}

int SceLibMtp_EA4E8C39(SceInt64 objectId, const void *data)
{
	SceUInt32 uniqueId = MTP_OBJECT_ID_UNIQUE(objectId);
	MtpLogicalStorage *logical;
	SceUInt32 storageId = MTP_OBJECT_STORAGE_ID(MTP_OBJECT_ID_STORAGE(objectId));
	int ret;

	if (data == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	logical = NULL;
	ret = mtpStrgGetLogicalStorage(storageId, &logical);
	if (ret < 0)
		return ret;
	ret = mtpObjUpdateObjectData(logical, uniqueId, data);
	if (ret < 0)
		return ret;
	mtpObjNotifyProprtyUpdate(uniqueId, storageId);
	return ret;
}

int SceLibMtp_79F3EE1B(void)
{
	return startHostDiscovery();
}

int SceLibMtp_71197C62(void)
{
	return stopHostDiscovery();
}

int SceLibMtp_06123E14(int *state)
{
	int discovery;
	int ret;

	if (state == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	ret = netMtpDeviceGetHostDiscoveryState(&discovery);
	if (ret < 0)
		return ret;
	if (discovery == 0)
		*state = 0;
	else if (discovery == 1)
		*state = 1;
	else if (discovery == 2)
		*state = 2;
	return 0;
}

int SceLibMtp_06714DA7(SceUInt32 *num)
{
	return netMtpDeviceGetNumDiscoveredHosts(num);
}

int SceLibMtp_2CA2CD16(SceUInt32 index, MtpNetHost *host)
{
	return netMtpDeviceGetDiscoveredHost(index, host);
}

/* a host found by discovery before it answers registration: ask for its details first */
int SceLibMtp_6DBD4FF7(MtpNetHost *host, NetMtpRegisterCallback callback)
{
	MtpNetHost found;
	SceUInt32 num;
	SceUInt32 i;
	int count;

	if (host->wirelessProtocolVersion == 0) {
		if (startHostDiscovery() < 0)
			return SCE_MTP_ERROR_NET_BAD_TIMING;
		count = MTP_BASE_DISCOVERY_RETRIES;
		while (count-- != 0 && host->wirelessProtocolVersion == 0) {
			sceKernelDelayThread(MTP_BASE_DISCOVERY_INTERVAL);
			netMtpDeviceGetNumDiscoveredHosts(&num);
			for (i = 0; i < num; i++) {
				netMtpDeviceGetDiscoveredHost(i, &found);
				if (mtpUtilMemcmp(found.id, host->id, sizeof(found.id)) == 0) {
					mtpUtilMemcpy(host, &found, sizeof(found));
					break;
				}
			}
		}
		stopHostDiscovery();
		if (host->wirelessProtocolVersion == 0)
			return SCE_MTP_ERROR_NET_BAD_TIMING;
	}
	return netMtpDeviceRegisterStart(host, callback);
}

int SceLibMtp_66B08C7D(const char *pin)
{
	return netMtpDeviceRegisterSendPin(pin);
}

int SceLibMtp_55B66FA0(void)
{
	return netMtpDeviceRegisterEnd();
}

int SceLibMtp_8E76FE4D(SceUInt32 *num)
{
	return mtpRegistryGetNumRegisteredNetMtpHosts(num);
}

int SceLibMtp_0898F989(SceUInt32 index, MtpNetHost *host)
{
	return mtpRegistryGetRegisteredNetMtpHost(index, host);
}

int SceLibMtp_9144D0B4(const MtpNetHost *host)
{
	return mtpRegistryUnregisterNetMtpHost(host);
}

/* a newly assigned host is stamped with the time; the one already assigned keeps its own */
int netMtpAssignHost(const MtpNetHost *host)
{
	MtpNetHost assigned;
	SceRtcTick tick;
	const SceUInt64 *time;
	int ret;

	time = NULL;
	if (host == NULL) {
		MTP_ERR(MTP_TAG_BASE, 2080, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = mtpRegistryGetAssignedNetMtpHost(&assigned);
	if (ret != 0) {
		if (ret != SCE_MTP_ERROR_NET_NOT_ASSIGNED) {
			MTP_ERR(MTP_TAG_BASE, 2094,
				"failed to get selected net mtp host from registry 0x%x\n", ret);
			return ret;
		}
	} else if (mtpUtilMemcmp(assigned.id, host->id, sizeof(assigned.id)) == 0) {
		goto assign;
	}
	time = &tick.tick;
	sceRtcGetCurrentTick(&tick);
assign:
	ret = mtpRegistryAssignNetMtpHost(host, time);
	if (ret > 0)
		return ret;
	return 0;
}

int SceLibMtp_27F8701C(void)
{
	return mtpRegistryUnassignNetMtpHosts();
}

int SceLibMtp_8CD95681(int *assigned)
{
	if (assigned == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	*assigned = mtpRegistryIsNetMtpHostAssigned();
	return 0;
}

int SceLibMtp_ED13170C(MtpNetHost *host)
{
	return mtpRegistryGetAssignedNetMtpHost(host);
}

int SceLibMtp_A78FEA82(int type)
{
	return mtpRegistrySetInterfaceType(type);
}

int sceMtpGetCurrentInterface(int *type)
{
	return mtpRegistryGetInterfaceType(type);
}

int SceLibMtp_4C1B7DBB(void)
{
	return mtpCoreGetIfType();
}

int SceLibMtp_D63692E8(MtpNetHost *host)
{
	return mtpCoreGetHostInfo(host);
}

int SceLibMtp_56FA46BB(const MtpNetHost *host, int *online)
{
	return netMtpDeviceCheckOnline(host, online);
}

int SceLibMtp_389F2FE2(const MtpNetHost *host, int *registered)
{
	return netMtpDeviceCheckRegistered(host, registered);
}

int SceLibMtp_B7984D4C(int *started)
{
	return mtpCoreIsStarted(started);
}

int SceLibMtp_CBE8187A(void)
{
	return mtpCoreSleep();
}

int SceLibMtp_965F1CD0(void)
{
	return mtpCoreWake(0);
}

int SceLibMtp_D6C90706(int arg1, int arg2, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(0);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_RPC_NET_OPERATION);
	if (ret < 0)
		return ret;
	ret = mtpContextRpcNetOperation(context, 0, arg1, arg2);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

/* the result of a network RPC: two words */
typedef struct MtpRpcNetOperationResult {
	int arg1;
	int arg2;
} MtpRpcNetOperationResult;

int SceLibMtp_6E2FE43A(SceUInt32 contextId, int arg1, int arg2)
{
	MtpRpcNetOperationResult result;

	result.arg1 = arg1;
	result.arg2 = arg2;
	return mtpIfEndCommon(contextId, &result, NULL);
}

int sceMtpBeginRpcNetOperationSend(int arg, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(0);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_RPC_NET_OPERATION);
	if (ret < 0)
		return ret;
	ret = mtpContextRpcNetOperation(context, 1, 0, arg);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_D919C89C(SceUInt32 contextId, int arg1, int arg2)
{
	MtpRpcNetOperationResult result;

	result.arg1 = arg1;
	result.arg2 = arg2;
	return mtpIfEndCommon(contextId, &result, NULL);
}

int sceMtpBeginRpcNetOperationRecv(int arg1, int arg2, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(0);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_RPC_NET_OPERATION);
	if (ret < 0)
		return ret;
	ret = mtpContextRpcNetOperation(context, 2, arg1, arg2);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_0311F66F(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int SceLibMtp_FB181427(int arg1, int arg2, int arg3, int *aborted)
{
	MtpContext *context;
	int ret;

	context = NULL;
	ret = mtpStatusCheck(0);
	if (ret < 0)
		return ret;
	ret = mtpContextCreateCancel(&context, MTP_OPERATION_REQUEST_RPC_NET_OPERATION_ABORT);
	if (ret < 0)
		return ret;
	ret = mtpContextRpcNetOperationAbort(context, arg1, arg2, arg3);
	*aborted = ret == 0 ? 0 : -1;
	mtpContextDelete(context);
	return ret;
}

int sceMtpBeginSearchObject(int arg1, int arg2, int arg3, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_SEARCH_OBJECT);
	if (ret < 0)
		return ret;
	ret = mtpContextSearchObject(context, arg2, arg1, arg3);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_BBCC3F5B(SceUInt32 contextId, int arg1, int arg2)
{
	MtpRpcNetOperationResult result;

	result.arg1 = arg1;
	result.arg2 = arg2;
	return mtpIfEndCommon(contextId, &result, NULL);
}

int sceMtpBeginHandover(int arg, SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(0, &context, MTP_OPERATION_REQUEST_HANDOVER);
	if (ret < 0)
		return ret;
	ret = mtpContextHandover(context, arg);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_462A6667(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

int SceLibMtp_29FAC63B(SceUInt8 *id)
{
	return netMtpDeviceGetDeviceId(id);
}

void SceLibMtp_768EBC03(int seconds)
{
	mtpCoreSetConnectTimeout(seconds);
}

/* a PC host answers the pause itself; a PS host's pause ends without waiting */
int sceMtpPauseExportObject(int arg)
{
	MtpContext *context;
	int ret;

	context = NULL;
	ret = mtpContextCreatePause(&context, MTP_OPERATION_PAUSE_EXPORT_OBJECT);
	if (ret < 0)
		return ret;
	paf::common::SharedPtr<MtpContext> held(context);
	ret = mtpContextPauseExportObject(context, arg);
	if (ret >= 0 && mtpStatusGetPlatformClass() != MTP_PLATFORM_CLASS_PS) {
		mtpContextRegister(held);
		ret = mtpContextWaitResult(context, NULL);
		mtpContextDelete(context);
	}
	return ret;
}

int sceMtpBeginResumeExportObject(SceUInt32 *contextId)
{
	MtpContext *context;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_RESUME_EXPORT_OBJECT);
	if (ret < 0)
		return ret;
	ret = mtpContextCreateProgress(context);
	if (ret < 0)
		return ret;
	ret = mtpContextResumeExportObject(context);
	if (ret < 0)
		mtpContextDelete(context);
	else
		*contextId = context->id;
	return ret;
}

int SceLibMtp_33B08344(SceUInt32 contextId)
{
	return mtpIfEndCommon(contextId, NULL, NULL);
}

/* a PS host resends the part of the object still missing; a PC host resumes the transfer */
int sceMtpBeginResumeImportObject(int arg1, int unused, SceUInt64 offset, SceUInt64 size,
				  const char *path, SceUInt32 *contextId)
{
	MtpContext *context;
	MtpStatus status;
	SceUInt32 handle;
	int ret;

	if (contextId == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	context = NULL;
	ret = mtpStatusCheck(1);
	if (ret < 0)
		return ret;
	ret = sceMtpGetHostInfo(&status);
	if (ret != 0) {
		MTP_ERR(MTP_TAG_BASE, 2573, " sceMtpGetHostInfo ret 0x%x\n", ret);
		return ret;
	}
	if (path == NULL || *path == '\0')
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	ret = mtpStrgGetMtpHandleByPath(path, &handle);
	if (ret < 0)
		return ret;
	mtpCoreSetObjectImportHandle(handle);
	if (status.applicationType == MTP_BASE_APPLICATION_PART_OF_OBJECT) {
		if (size == (SceUInt64)-1 && offset != 0)
			offset = 0;
		ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_PART_OF_OBJECT);
		if (ret < 0)
			return ret;
		ret = mtpContextCreateProgress(context);
		if (ret < 0)
			return ret;
		ret = mtpContextGetPartOfObject(context, arg1, offset, size, 1);
		if (ret < 0)
			mtpContextDelete(context);
		else
			*contextId = context->id;
	} else {
		ret = mtpContextCreate(1, &context, MTP_OPERATION_REQUEST_RESUME_IMPORT_OBJECT);
		if (ret < 0)
			return ret;
		ret = mtpContextCreateProgress(context);
		if (ret < 0)
			return ret;
		ret = mtpContextResumeImportObject(context);
		if (ret < 0)
			mtpContextDelete(context);
		else
			*contextId = context->id;
	}
	return ret;
}

int sceMtpEndResumeImportObject(SceUInt32 contextId, MtpObjectId *objectId)
{
	MtpContext *context;
	SceUInt32 uniqueId;
	SceUInt32 handle;
	int ret;

	context = NULL;
	ret = mtpContextGet(contextId, 0, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_BASE, 2650, "GetContext %u failed: 0x%x\n", contextId, ret);
		return ret;
	}
	handle = context->objectHandle;
	ret = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (ret != 0) {
		MTP_ERR(MTP_TAG_BASE, 2661,
			"mtpStrgVTConvertMtpHandleToUniqueId failed: 0x%x, handle=0x%x\n", ret,
			handle);
	} else if (objectId != NULL) {
		objectId->uniqueId = uniqueId;
		objectId->storage = handle & MTP_HANDLE_STORAGE_MASK;
	}
	ret = mtpContextWaitResult(context, NULL);
	mtpContextDelete(context);
	if (ret != SCE_MTP_ERROR_CANCELED)
		mtpCoreClearObjectImport();
	return ret;
}

void SceLibMtp_AF9A2787(void)
{
	mtpUtilEnableLargeBuffer();
}

int SceLibMtp_BB32CD0A(void)
{
	mtpCmdMarlinFreeBuffers();
	return mtpUtilLargeBufferEnd();
}

int SceLibMtp_E89AF226(const MtpBgCopyTask *task, void *path)
{
	return mtpBgCopyRequestTask(task, path);
}

int SceLibMtp_E7542628(void)
{
	return mtpBgCopyEndTask();
}

int SceLibMtp_B15DA25F(const void *status)
{
	return mtpBgCopySetTaskInfo(status);
}

int SceLibMtp_972A378F(const void *data, SceSize size, int index)
{
	return mtpBgCopyWriteData(data, size, index);
}

int SceLibMtp_ADE08924(void *data, SceUInt32 offset, SceSize size, int index, SceSize *read)
{
	return mtpBgCopyReadData(data, offset, size, index, read);
}

int SceLibMtp_934240B4(MtpEventHandler handler, void *arg)
{
	mtpCallbackSetEventHandler(handler, arg);
	return 0;
}

int SceLibMtp_E7CB0238(void)
{
	mtpCallbackClearEventHandler();
	return 0;
}

int SceLibMtp_87D21619(int unused, const SceUInt8 *task)
{
	return mtpBgCopySetTaskInfo(task + 16);
}

int SceLibMtp_DE41AE27(const char *titleId)
{
	mtpTaskQueueSetTitleId(titleId);
	return 0;
}

int SceLibMtp_3AF2F5D8(int arg)
{
	return mtpStatusSetArg(arg);
}

void SceLibMtp_4E2CF833(int canceling)
{
	mtpContextSetCanceling(canceling);
}

int SceLibMtp_67DB4C13(const SceLibMtpHostFuncs *funcs)
{
	g_bgCopyServiceFuncs = *funcs;
	return 0;
}

}
