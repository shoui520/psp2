#pragma once

#include <psp2/types.h>
#include <psp2kern/types.h>
#include <threadmgr/fast_mutex_object.h>
#include <vnz_wrapper/venezia_debug.h>
#include <avcdec.h>
#include <stddef.h>
#include <avcodec/avcodec.h>

struct M4vdecPicture;

typedef struct M4vdecAu {
	unsigned char timingMetadata[16];
	SceAvcdecBuf es;
} M4vdecAu;

_Static_assert(sizeof(M4vdecAu) == 24, "M4V access unit ABI");
_Static_assert(offsetof(M4vdecAu, es) == 16, "M4V elementary stream ABI");

typedef struct M4vdecArrayPicture {
	unsigned int numOfOutput;
	unsigned int numOfElm;
	struct M4vdecPicture **pPicture;
} M4vdecArrayPicture;

_Static_assert(sizeof(M4vdecArrayPicture) == 12, "M4V picture array ABI");
_Static_assert(offsetof(M4vdecArrayPicture, pPicture) == 8,
	       "M4V picture pointers ABI");

typedef struct VideocodecSeiUserDataEntry {
	unsigned char payloadType;
	unsigned char padding[3];
	SceSize payloadSize;
} VideocodecSeiUserDataEntry;

typedef struct VideocodecSeiClockTimestamp {
	unsigned char clockMetadata[13];
	unsigned char padding[3];
	int timeOffset;
} VideocodecSeiClockTimestamp;

typedef struct VideocodecSeiPictureTiming {
	unsigned char valid;
	unsigned char padding[3];
	unsigned int cpbRemovalDelay;
	unsigned int dpbOutputDelay;
	unsigned char picStruct;
	unsigned char clockTimestampFlag[3];
	VideocodecSeiClockTimestamp clockTimestamp[3];
} VideocodecSeiPictureTiming;

_Static_assert(sizeof(VideocodecSeiClockTimestamp) == 20,
	       "SEI clock timestamp ABI");
_Static_assert(offsetof(VideocodecSeiClockTimestamp, timeOffset) == 16,
	       "SEI clock offset ABI");
_Static_assert(sizeof(VideocodecSeiPictureTiming) == 76,
	       "SEI picture timing ABI");
_Static_assert(offsetof(VideocodecSeiPictureTiming, clockTimestamp) == 16,
	       "SEI picture clocks ABI");

typedef struct VideocodecSeiUserData {
	SceSize size;
	unsigned char numOfUserData;
	unsigned char padding[3];
	SceSize userDataSize;
	VideocodecSeiUserDataEntry userData[15];
} VideocodecSeiUserData;

_Static_assert(sizeof(VideocodecSeiUserDataEntry) == 8, "SEI user data entry ABI");
_Static_assert(sizeof(VideocodecSeiUserData) == 132, "SEI user data ABI");
_Static_assert(offsetof(VideocodecSeiUserData, userData) == 12,
	       "SEI user data entries ABI");

/* Complete resident owner, separate from the following DSP0 codec state. */
typedef struct VideocodecResident {
	int processLocalStorageId;
	SceUID activeProcessId;
	SceUID ownerProcessIds[4];
	void (*notifyProcessChange)(void);
	struct ThreadmgrFastMutex allMutex;
} VideocodecResident;

_Static_assert(sizeof(VideocodecResident) == 92, "video codec resident ABI");

/* Complete host/RPC allocation; opaque spans contain live coprocessor data. */
typedef struct Dsp0VideocodecLibraryCommand {
	uintptr_t contextAddress;
	unsigned int libraryToken;
	unsigned char coprocessorHeader[24];
	int numOfDecoders;
	int numOfEncoders;
	SceSize commandSize;
	unsigned short horizontal;
	unsigned char coprocessorConfiguration[2];
	unsigned int coprocessorQueryData[2];
	uintptr_t workAddress;
	SceSize workSize;
	unsigned char coprocessorState[192];
} Dsp0VideocodecLibraryCommand;

_Static_assert(sizeof(Dsp0VideocodecLibraryCommand) == 256,
	       "DSP0 library command ABI");
_Static_assert(offsetof(Dsp0VideocodecLibraryCommand, numOfDecoders) == 32,
	       "DSP0 decoder count ABI");
_Static_assert(offsetof(Dsp0VideocodecLibraryCommand, horizontal) == 44,
	       "DSP0 library configuration ABI");
_Static_assert(offsetof(Dsp0VideocodecLibraryCommand, workAddress) == 56,
	       "DSP0 library work memory ABI");

struct Dsp0VideocodecDecoder;
struct Dsp0VideocodecEncoder;

typedef struct Dsp0VideocodecGlobal {
	unsigned int horizontal;
	unsigned int vertical;
	unsigned int numOfRefFrames;
	unsigned int numOfStreams;
	unsigned int numOfEncoders;
	void *workMemory;
	struct Dsp0VideocodecLibraryCommand *library;
	struct ThreadmgrFastMutex mutex;
	struct ThreadmgrFastMutex *pMutex;
	int initialized;
	struct Dsp0VideocodecDecoder *decoders;
	struct Dsp0VideocodecEncoder *encoders;
	SceKernelSpinlock statusLock;
	int status;
} Dsp0VideocodecGlobal;

_Static_assert(sizeof(Dsp0VideocodecGlobal) == 116, "DSP0 process storage ABI");

typedef struct Dsp0VideocodecInitParam {
	unsigned int horizontal;
	unsigned int vertical;
	unsigned int numOfRefFrames;
	unsigned int numOfStreams;
	unsigned int numOfEncoders;
} Dsp0VideocodecInitParam;

_Static_assert(sizeof(Dsp0VideocodecInitParam) == 20, "DSP0 initialization ABI");

typedef struct AvcencNalUnit {
	void *data;
	SceSize size;
	unsigned char type;
	unsigned char referencePriority;
	unsigned char padding[2];
} AvcencNalUnit;

typedef struct AvcencCscFrame {
	SceSize size;
	unsigned int pixelType;
	unsigned int framePitch;
	unsigned int frameWidth;
	unsigned int frameHeight;
	SceAvcdecFrameOption opt;
	void *pPicture[2];
	unsigned int encodeMode;
} AvcencCscFrame;

typedef struct AvcencEncoderParameter {
	unsigned int bitRate[2];
	SceSize cpbSize;
	unsigned int gopLength;
	unsigned int numUnitsInTick;
	unsigned int timeScale;
} AvcencEncoderParameter;

_Static_assert(sizeof(AvcencEncoderParameter) == 24,
	       "AVC encoder parameter ABI");

_Static_assert(sizeof(AvcencCscFrame) == 48, "AVC encoder CSC frame ABI");
_Static_assert(offsetof(AvcencCscFrame, pPicture) == 36,
	       "AVC encoder CSC picture addresses ABI");

int SceAvcodecForDriver_4F11F915(AvcencCscFrame *pDst,
			       const AvcencCscFrame *pSrc);	/* vitasdk: ksceAvcencCsc */

typedef struct AvcencControl {
	unsigned int handle;
	void *workMemory;
	SceSize workMemorySize;
	void *frameMemory;
	SceSize frameMemorySize;
	void *headerMemory;
	SceSize headerMemorySize;
	AvcencNalUnit headers[2];
} AvcencControl;

struct AvcencAu;

typedef struct AvcencArrayAu {
	unsigned int numOfOutput;
	unsigned int numOfElm;
	struct AvcencAu **pAu;
} AvcencArrayAu;

_Static_assert(sizeof(AvcencArrayAu) == 12, "AVC encoder output array ABI");
_Static_assert(offsetof(AvcencArrayAu, pAu) == 8,
	       "AVC encoder output addresses ABI");

int SceAvcodecForDriver_DE515BFF(const AvcencControl *pCtrl,
		       AvcencArrayAu *pArrayAu);	/* vitasdk: ksceAvcencEncodeStop */
/* vitasdk: ksceAvcencEncode */
int SceAvcodecForDriver_A0E63278(const AvcencControl *pCtrl,
		    const AvcencCscFrame *pFrame, AvcencArrayAu *pArrayAu);

_Static_assert(sizeof(AvcencNalUnit) == 12, "AVC encoder NAL descriptor ABI");
_Static_assert(sizeof(AvcencControl) == 52, "AVC encoder control ABI");
_Static_assert(offsetof(AvcencControl, headers) == 28,
	       "AVC encoder header outputs ABI");

int videocodecTermLibraryForProcess(unsigned int codecType, unsigned int flags,
				  SceUID pid);
int SceAvcodecForDriver_27211864(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_D9FCDF3E(SceAvcdecCtrl *pCtrl, SceUID cbid);
int SceAvcodecForDriver_13C530E4(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_E4F668C5(SceAvcdecCtrl *pCtrl, SceUID cbid);
int SceAvcodecForDriver_F714DB9A(SceAvcdecCtrl *pCtrl,
	const VeneziaVipVideoInput *pAu, const SceUInt *timeout);
int SceAvcodecForDriver_D7CD7F82(SceAvcdecCtrl *pCtrl,
	SceAvcdecArrayPicture *pArrayPicture, const SceUInt *timeout);
int SceAvcodecForDriver_D2ACCE22(SceAvcdecCtrl *pCtrl,
	const SceAvcdecAu *pAu, const SceUInt *timeout);
int SceAvcodecForDriver_78BA8E98(SceAvcdecCtrl *pCtrl,
	const SceAvcdecAu *pAu, const SceUInt *timeout);
int SceAvcodecForDriver_6E250A5F(SceAvcdecCtrl *pCtrl,
	SceAvcdecArrayPicture *pArrayPicture,
	SceAvcdecArrayPicture *pWorkPicture, const SceUInt *timeout);
int SceAvcodecForDriver_663B3195(unsigned int codecType,
	SceAvcdecCtrl *pCtrl, const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int videocodecCreateDecoder(unsigned int codecType,
	SceAvcdecCtrl *pCtrl, const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int videocodecValidateDecoderInfo(unsigned int codecType,
	const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_D5A34236(unsigned int codecType,
	const SceAvcdecQueryDecoderInfo *pQuery, SceAvcdecDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_C518A109(SceAvcdecCtrl *pCtrl, int trickMode);
int SceAvcodecForDriver_FACCD284(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo, SceVideodecMemInfo *pMemInfo);
int SceAvcodecForDriver_22D7B653(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo, SceVideodecMemInfo *pMemInfo);
int SceAvcodecForDriver_4AE6B437(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_4DD3826D(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_95E8AFCA(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_6DC39EDC(unsigned int codecType, SceVideodecCtrl *pCtrl,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_C93DA0C7(unsigned int codecType, SceVideodecCtrl *pCtrl,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_BC55D10F(unsigned int codecType, SceVideodecCtrl *pCtrl,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_427914D5(SceAvcdecCtrl *pCtrl, unsigned int field,
	VideocodecSeiUserData *pUserData, SceAvcdecBuf *pBuf);
int SceAvcodecForDriver_BC244D21(SceAvcdecCtrl *pCtrl, unsigned int field,
	VideocodecSeiPictureTiming *pPictureTiming);
int SceAvcodecForDriver_77529FD2(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_F67F93DC(const SceAvcdecCtrl *pCtrl,
	M4vdecArrayPicture *pArrayPicture);
int SceAvcodecForDriver_3F146942(const SceAvcdecCtrl *pCtrl,
	M4vdecArrayPicture *pArrayPicture, M4vdecArrayPicture *pWorkPicture);
int SceAvcodecForDriver_2C7A9841(const SceAvcdecCtrl *pCtrl,
	const M4vdecAu *pAu, M4vdecArrayPicture *pArrayPicture);
int SceAvcodecForDriver_823E187E(const SceAvcdecCtrl *pCtrl,
	const M4vdecAu *pAu, M4vdecArrayPicture *pArrayPicture,
	M4vdecArrayPicture *pWorkPicture);
int SceAvcodecForDriver_91E94B76(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_FD805BF1(const SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_F8BCBE20(SceAvcdecCtrl *pCtrl,
	SceAvcdecArrayPicture *pArrayPicture, const SceUInt *timeout);
int SceAvcodecForDriver_E355CAAE(const SceAvcdecCtrl *pCtrl,
	SceAvcdecArrayPicture *pArrayPicture, SceAvcdecArrayPicture *pWorkPicture);
int SceAvcodecForDriver_EAEB16D5(const SceAvcdecCtrl *pCtrl,
	const SceAvcdecAu *pAu, SceAvcdecArrayPicture *pArrayPicture,
	SceAvcdecArrayPicture *pWorkPicture);
int SceAvcodecForDriver_536C8296(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo, SceVideodecMemInfo *pMemInfo);
int SceAvcodecForDriver_301CCA25(unsigned int codecType, SceAvcdecCtrl *pCtrl,
		const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_01387A93(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pQuery,
		SceAvcdecDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_C114C499(unsigned int codecType,
	const SceAvcdecQueryDecoderInfo *pQuery, SceAvcdecDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_C9EF7057(SceAvcdecCtrl *pCtrl,
	const VeneziaVipVideoInput *pAu, const SceUInt *timeout);
int SceAvcodecForDriver_90F394E6(SceAvcdecCtrl *pCtrl, SceUID cbid,
			       unsigned int eventMask);
int SceAvcodecForDriver_E7E14DE4(SceAvcdecCtrl *pCtrl, SceUID cbid,
			       unsigned int eventMask);
int SceAvcodecForDriver_A2085B6B(unsigned int codecType, unsigned int size);
/* vitasdk: ksceAvcencSetAvailablePreset */
int SceAvcodecForDriver_7DC13C29(unsigned int codecType, unsigned int preset);
int SceAvcodecForDriver_2895CB6D(AvcencControl *pCtrl);	/* vitasdk: ksceAvcencDeleteEncoder */
int SceAvcodecForDriver_229AB2CF(const AvcencControl *pCtrl);	/* vitasdk: ksceAvcencEncodeFlush */
/* vitasdk: ksceAvcencSetEncoderParameter */
int SceAvcodecForDriver_E0232BFE(const AvcencControl *pCtrl,
			       const AvcencEncoderParameter *pParameter);
int SceAvcodecForDriver_1A584DB7(const AvcencControl *pCtrl, unsigned int index,
				AvcencNalUnit *pNalUnit);	/* vitasdk: ksceAvcencGetNalUnit */

int SceAvcodecForDriver_20892154(const SceAvcdecCtrl *pCtrl,
	const VeneziaVipVideoInput *pAu, SceAvcdecArrayPicture *pArrayPicture,
	SceAvcdecArrayPicture *pWorkPicture);
int SceAvcodecForDriver_0CC84413(const SceAvcdecCtrl *pCtrl,
	const VeneziaVipVideoInput *pAu, SceAvcdecArrayPicture *pArrayPicture);
int SceAvcodecForDriver_73401748(unsigned int codecType, unsigned int mode);
int SceAvcodecForDriver_866B49BF(unsigned int codecType, unsigned int mode);
int SceAvcodecForDriver_43985AA8(unsigned int codecType, unsigned int flags);
int SceAvcodecForDriver_12DAE04A(unsigned int codecType, unsigned int flags);
int SceAvcodecForDriver_20C83C00(unsigned int codecType, unsigned int mode);
int SceAvcodecForDriver_3CB7661A(unsigned int codecType, unsigned int mode);
int SceAvcodecForDriver_C8366BB8(unsigned int codecType, unsigned int flag);
int SceAvcodecForDriver_7E8CC4E5(unsigned int codecType, SceAvcdecCtrl *pCtrl,
			       const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_EA07A3B7(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_43A9CD35(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_48DECA56(const SceAvcdecCtrl *pCtrl,
			       const SceAvcdecAu *pAu,
			       SceAvcdecArrayPicture *pArrayPicture);
int SceAvcodecForDriver_DFC0AAC6(const SceAvcdecCtrl *pCtrl,
			       SceAvcdecArrayPicture *pArrayPicture);
int SceAvcodecForDriver_D46B8463(const SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_16B501C9(SceAvcdecPicture *pDst,
			       const SceAvcdecPicture *pSrc);

