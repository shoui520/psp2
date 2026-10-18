#pragma once

#include <psp2kern/types.h>
#include <stdint.h>
#include <bt/bt_hid.h>

int sceBtAvrcpReadVolume(uint64_t deviceAddress);
int sceBtAvrcpSendButton(uint64_t deviceAddress, int button);
int sceBtAvrcpSendVolume(uint64_t deviceAddress, int volume);
int sceBtAvrcpSetPlayStatus(int status);
int sceBtAvrcpSetTitle(const void *title, unsigned int length);
int sceBtDeleteRegisteredInfo(uint64_t deviceAddress);
int sceBtFreqAudio(uint64_t deviceAddress);
int sceBtGetConfiguration(void);
int sceBtGetConnectingInfo(uint64_t deviceAddress);
int sceBtGetDeviceId(uint64_t deviceAddress, void *buffer);
int sceBtGetDeviceName(uint64_t deviceAddress, char *buffer);
int sceBtGetInfoForTest(uint64_t deviceAddress, void *buffer);
int sceBtGetLastError(void);
int sceBtGetRegisteredInfo(uint64_t deviceAddress, void *buffer, int maxCount);
int sceBtGetStatusForTest(int selector, void *buffer, int size);
int sceBtHfpGetCurrentPhoneNumber(void);
int sceBtHfpRequest(void);
int sceBtHidGetReportDescriptor(uint64_t deviceAddress, void *buffer, unsigned int capacity);
int sceBtHidTransfer(uint64_t deviceAddress, struct _SceBtHidRequest *request);
int sceBtHidVuGetReportFeature(uint64_t deviceAddress, uint8_t reportId);
int sceBtHidVuGetReportInput(uint64_t deviceAddress, uint8_t reportId);
int sceBtHidVuReadEvent(void *buffer, int maxCount);
int sceBtHidVuRegisterCallback(SceUID callback, int vendorId, int productId);
int sceBtHidVuSetInterruptOutput(uint64_t deviceAddress, const void *data, int length);
int sceBtHidVuSetReportFeature(uint64_t deviceAddress, const void *data, int length);
int sceBtHidVuSetReportOutput(uint64_t deviceAddress, const void *data, int length);
int sceBtHidVuUnregisterCallback(void);
int sceBtPairingOOB(void);
int sceBtPushBip(uint64_t deviceAddress, const void *path, unsigned int length);
int sceBtPushOpp(void);
int sceBtRecvAudio(uint64_t deviceAddress, void *buffer, SceSize size);
int sceBtRecvBip(const void *path, unsigned int length);
int sceBtRecvOpp(void);
int sceBtRecvSpp(uint64_t deviceAddress, void *data, int length);
int sceBtRegisterCallback(SceUID callback, uint64_t eventMask);
int sceBtReplyPinCode(unsigned int mac0, unsigned int mac1, unsigned char *code, SceSize length);
int sceBtReplyUserConfirmation(unsigned int mac0, unsigned int mac1, int accepted);
int sceBtSendAudio(uint64_t deviceAddress, void *buffer, SceSize size);
int sceBtSendL2capEchoRequestForTest(uint64_t deviceAddress, const void *data, unsigned int length);
int sceBtSendSpp(uint64_t deviceAddress, const void *data, unsigned int length);
int sceBtSetConfiguration(int configuration);
int sceBtSetContentProtection(int protection);
int sceBtSetInquiryResultForTest(const void *resultBuffer);
int sceBtSetInquiryScan(int enabled);
int sceBtSetL2capEchoResponseBufferForTest(uint64_t deviceAddress, uint8_t *buffer, int capacity);
int sceBtSetStatusForTest(int selector, void *buffer, int size);
int sceBtStartAudio(uint64_t deviceAddress, int profile);
int sceBtStartConnect(unsigned int mac0, unsigned int mac1);
int sceBtStartDisconnect(unsigned int mac0, unsigned int mac1);
int sceBtStartInquiry(void);
int sceBtStopAudio(uint64_t deviceAddress, int profile);
int sceBtStopInquiry(void);
int sceBtUnregisterCallback(SceUID callback);
