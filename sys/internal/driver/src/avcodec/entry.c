/* SceAvcodec process event dispatch. */
#include <processmgr/processmgr.h>
#include <sysmem/procevent.h>
#include <modulemgr/modulemgr.h>
#include <vnz_wrapper/vnz_wrapper.h>
#include "audiodec/audiodec_internal.h"
#include "audioenc/audioenc_internal.h"
#include "videocodec/videocodec_internal.h"
#include "jpeg/jpeg_internal.h"
#include "jpegenc/jpegenc_internal.h"

extern int _sceVideocodecProcessExitEntry(SceUID pid, SceUInt32 type);
extern int _sceVideocodecProcessCreateEntry(SceUID pid, SceUInt32 type);
extern int dsp0VideocodecProcessSuspend(SceUID pid);

int avcodecProcessExitHandler(SceUID pid, ProcEventParam *param, void *common)
{
	SceUInt32 type = param->type;
	int videoStatus, status, ret;

	videoStatus = _sceVideocodecProcessExitEntry(pid, type);
	status = _sceDsp0VideocodecProcessExitEntry(pid, type);
	if (status >= 0)
		status = videoStatus < 0 ? videoStatus : 0;
	ret = jpegEncoderProcessExitEntry(pid, type);
	if (ret < 0)
		status = ret;
	ret = jpegProcessExitEntry(pid, type);
	if (ret < 0)
		status = ret;
	ret = _sceAudioencProcessExitEntry(pid, type);
	if (ret < 0)
		status = ret;
	ret = _sceAudiodecProcessExitEntry(pid, type);
	return ret < 0 ? ret : status;
}

int avcodecProcessCreateHandler(SceUID pid, const ProcEventCreateParam *param)
{
	SceUInt32 type = param->type;
	int ret;

	ret = _sceAudiodecProcessCreateEntry(pid, type);
	if (ret < 0)
		return ret;
	ret = _sceAudioencProcessCreateEntry(pid, type);
	if (ret < 0)
		return ret;
	ret = jpegProcessCreateEntry(pid, type);
	if (ret < 0)
		return ret;
	ret = jpegEncoderProcessCreateEntry(pid, type);
	if (ret < 0)
		return ret;
	ret = _sceDsp0VideocodecProcessCreateEntry(pid, type);
	if (ret < 0)
		return ret;
	ret = _sceVideocodecProcessCreateEntry(pid, type);
	return ret < 0 ? ret : 0;
}

int avcodecProcessResumeHandler(SceUID pid, unsigned int event)
{
	int ret;

	switch (event) {
	case 0x10000:
		ret = videocodecProcessResume(pid);
		return ret < 0 ? ret : 0;
	case 0x20001:
		ret = dsp0VideocodecProcessResume(pid);
		return ret < 0 ? ret : 0;
	}
	return 0;
}

int avcodecProcessSuspendHandler(SceUID pid, unsigned int event)
{
	int ret;

	switch (event) {
	case 0x202:
		return dsp0VideocodecIsProcessSuspended(pid) ? 0 : -1;
	case 0x800:
		ret = videocodecProcessSuspend(pid);
		return ret < 0 ? ret : 0;
	case 0x201:
		if (sceCodecEngineDsp0IsProcessSuspended(pid) &&
		    dsp0VideocodecIsProcessActive(pid)) {
			ret = dsp0VideocodecProcessSuspend(pid);
			return ret < 0 ? ret : 0;
		}
		break;
	}
	return 0;
}

int avcodecProcessKillHandler(SceUID pid, ProcEventParam *param, void *common)
{
	SceUInt32 type = param->type;
	int videoStatus, status, ret;

	videoStatus = _sceVideocodecProcessExitEntry(pid, type);
	status = _sceDsp0VideocodecProcessExitEntry(pid, type);
	if (status >= 0)
		status = videoStatus < 0 ? videoStatus : 0;
	ret = jpegEncoderProcessExitEntry(pid, type);
	if (ret < 0)
		status = ret;
	ret = jpegProcessExitEntry(pid, type);
	if (ret < 0)
		status = ret;
	ret = _sceAudioencProcessExitEntry(pid, type);
	if (ret < 0)
		status = ret;
	ret = _sceAudiodecProcessExitEntry(pid, type);
	return ret < 0 ? ret : status;
}

int module_start(SceSize arglen, const void *argp)
{
	SceKernelProcEventHandler handler;

	if (initializeAudiodec() < 0 || initializeAudioenc() < 0 ||
	    initializeJpeg() < 0 || initializeJpegEncoder() < 0 ||
	    initializeDsp0Videocodec() < 0 || initializeVideocodec() < 0)
		return SCE_KERNEL_START_FAILED;
	handler.create = (ProcEventCreateHandler)
		avcodecProcessCreateHandler;
	handler.suspend = (ProcEventPhaseHandler)
		avcodecProcessSuspendHandler;
	handler.resume = (ProcEventPhaseHandler)
		avcodecProcessResumeHandler;
	handler.exit = NULL;
	handler.kill = NULL;
	handler.notify = NULL;
	handler.size = sizeof(handler);
	if (sceCodecEngineRegisterProcExitHandler(avcodecProcessExitHandler) < 0 ||
	    sceCodecEngineRegisterProcKillHandler(avcodecProcessKillHandler) < 0 ||
	    sceKernelRegisterProcEventHandler("SceAvcodec", &handler, NULL) < 0)
		return SCE_KERNEL_START_FAILED;
	return SCE_KERNEL_START_SUCCESS;
}
