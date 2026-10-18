/* Bluetooth audio and AVRCP device interfaces. */
#pragma once

#include <scetypes.h>
#include <stdint.h>

int SceBtForDriver_E9936446(SceUInt64 deviceAddress);	/* vitasdk: ksceBtGetConnectingInfo */
/* vitasdk: ksceBtAvrcpSendButton */
int SceBtForDriver_EC5F8519(SceUInt64 deviceAddress, int button);
/* vitasdk: ksceBtAvrcpSendVolume */
int SceBtForDriver_7689DA3D(SceUInt64 deviceAddress, int volume);
int SceBtForDriver_C9C70056(SceUInt64 deviceAddress);	/* vitasdk: ksceBtAvrcpReadVolume */
/* vitasdk: ksceBtStartAudio */
int SceBtForDriver_8D47CABD(SceUInt64 deviceAddress, int profile);
/* vitasdk: ksceBtStopAudio */
int SceBtForDriver_CAE5A9F3(SceUInt64 deviceAddress, int profile);
int SceBtForDriver_DA20DCC8(SceUInt64 deviceAddress);	/* vitasdk: ksceBtFreqAudio */
/* vitasdk: ksceBtSendAudio */
int SceBtForDriver_47F19727(SceUInt64 deviceAddress, void *buffer, SceSize size);
/* vitasdk: ksceBtRecvAudio */
int SceBtForDriver_CD7C0492(SceUInt64 deviceAddress, void *buffer, SceSize size);

/* vitasdk: ksceBtHidVuSetReportFeature */
int SceBtForDriver_014A1D28(uint64_t deviceAddress, const void *data, int length);
int SceBtForDriver_08632086(int protection);	/* vitasdk: ksceBtSetContentProtection */
/* vitasdk: ksceBtSetL2capEchoResponseBufferForTest */
int SceBtForDriver_0B7D8E0E(uint64_t deviceAddress, uint8_t *buffer, int capacity);
int SceBtForDriver_1AB31C7D(void);	/* vitasdk: ksceBtHidVuUnregisterCallback */
int SceBtForDriver_1C26F878(const void *resultBuffer);	/* vitasdk: ksceBtSetInquiryResultForTest */
int SceBtForDriver_21C788E2(void);	/* vitasdk: ksceBtPushOpp */
/* vitasdk: ksceBtGetInfoForTest */
int SceBtForDriver_2C9DD7BB(uint64_t deviceAddress, void *buffer);
int SceBtForDriver_3333A878(void *buffer, int maxCount);	/* vitasdk: ksceBtHidVuReadEvent */
/* vitasdk: ksceBtHidVuGetReportInput */
int SceBtForDriver_3BBC1EE1(uint64_t deviceAddress, uint8_t reportId);
int SceBtForDriver_4F5A852E(int enabled);	/* vitasdk: ksceBtSetInquiryScan */
/* vitasdk: ksceBtStartConnect */
int SceBtForDriver_6059113A(unsigned int mac0, unsigned int mac1);
/* vitasdk: ksceBtGetDeviceName */
int SceBtForDriver_633F784C(uint64_t deviceAddress, char *buffer);
int SceBtForDriver_6A1D9598(void);	/* vitasdk: ksceBtStopInquiry */
int SceBtForDriver_6E49D11D(void);	/* vitasdk: ksceBtHfpRequest */
/* vitasdk: ksceBtGetVidPid */
int SceBtForDriver_780A5557(uint64_t deviceAddress, void *buffer);
int SceBtForDriver_7BC66D6D(void);	/* vitasdk: ksceBtHfpGetCurrentPhoneNumber */
/* vitasdk: ksceBtPushBip */
int SceBtForDriver_7FF77792(uint64_t deviceAddress, const void *path, unsigned int length);
int SceBtForDriver_8008AA2C(void);	/* vitasdk: ksceBtRecvOpp */
/* vitasdk: ksceBtAvrcpSetTitle */
int SceBtForDriver_91EF55AD(const void *title, unsigned int length);
/* vitasdk: ksceBtSendSpp */
int SceBtForDriver_92241DFB(uint64_t deviceAddress, const void *data, unsigned int length);
int SceBtForDriver_94C66A3B(void);	/* vitasdk: ksceBtGetConfiguration */
/* vitasdk: ksceBtSetStatusForTest */
int SceBtForDriver_9C195411(int selector, void *buffer, int size);
int SceBtForDriver_9DC7E7E5(void);	/* vitasdk: ksceBtStartInquiry */
/* vitasdk: ksceBtHidVuSetReportOutput */
int SceBtForDriver_AB2032A3(uint64_t deviceAddress, const void *data, int length);
/* vitasdk: ksceBtHidVuRegisterCallback */
int SceBtForDriver_AB57F8C1(SceUID callback, int vendorId, int productId);
int SceBtForDriver_AD31A43E(void);	/* vitasdk: ksceBtGetLastError */
/* vitasdk: ksceBtReplyUserConfirmation */
int SceBtForDriver_B5FA291C(unsigned int mac0, unsigned int mac1, int accepted);
/* vitasdk: ksceBtReplyPinCode */
int SceBtForDriver_BB69A62A(unsigned int mac0, unsigned int mac1, unsigned char *code, SceSize length);
int SceBtForDriver_BF72E1BE(SceUID callback);	/* vitasdk: ksceBtUnregisterCallback */
/* vitasdk: ksceBtRecvSpp */
int SceBtForDriver_C09D9E7C(uint64_t deviceAddress, void *data, int length);
int SceBtForDriver_C5E7349C(int status);	/* vitasdk: ksceBtAvrcpSetPlayStatus */
/* vitasdk: ksceBtHidVuGetReportFeature */
int SceBtForDriver_CA9B885C(uint64_t deviceAddress, uint8_t reportId);
/* vitasdk: ksceBtHidVuSetInterruptOutput */
int SceBtForDriver_D2449052(uint64_t deviceAddress, const void *data, int length);
/* vitasdk: ksceBtSendL2capEchoRequestForTest */
int SceBtForDriver_E3A66814(uint64_t deviceAddress, const void *data, unsigned int length);
int SceBtForDriver_E6F659E0(uint64_t deviceAddress);	/* vitasdk: ksceBtDeleteRegisteredInfo */
#define SCE_BT_ERROR_REG_DELETE_CONNECTING	((int)0x802F0109)	/* a connection to it is open */
int SceBtForDriver_E8E95977(const void *path, unsigned int length);	/* vitasdk: ksceBtRecvBip */
int SceBtForDriver_EBDCC992(int configuration);	/* vitasdk: ksceBtSetConfiguration */
