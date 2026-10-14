/* Bluetooth audio and AVRCP device interfaces. */
#pragma once

#include <psp2kern/types.h>

int ksceBtGetConnectingInfo(SceUInt64 deviceAddress);
int ksceBtAvrcpSendButton(SceUInt64 deviceAddress, int button);
int ksceBtAvrcpSendVolume(SceUInt64 deviceAddress, int volume);
int ksceBtAvrcpReadVolume(SceUInt64 deviceAddress);
int ksceBtStartAudio(SceUInt64 deviceAddress, int profile);
int ksceBtStopAudio(SceUInt64 deviceAddress, int profile);
int ksceBtFreqAudio(SceUInt64 deviceAddress);
int ksceBtSendAudio(SceUInt64 deviceAddress, void *buffer, SceSize size);
int ksceBtRecvAudio(SceUInt64 deviceAddress, void *buffer, SceSize size);
