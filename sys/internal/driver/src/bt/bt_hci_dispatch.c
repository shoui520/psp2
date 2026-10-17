/* HCI event dispatch; original source filename unavailable. */
#include <psp2kern/kernel/aimgr.h>
#include <syscon/syscon.h>
#include <string.h>
#include "bt_internal.h"

int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);
int BtPack(uint8_t *destination, int remaining, const char *format, ...);
int BtSendHciCommand(BtStack *stack, unsigned int opcode,
		     const char *format, ...);
BtConnection *BtFindConnection(BtStack *stack, unsigned char selector,
			       uint64_t deviceAddress);
BtDevice *BtGetOrCreateDevice(BtStack *stack, uint64_t deviceAddress, int create);
int BtProcessInquiryResult(BtStack *stack, uint64_t deviceAddress,
			  uint8_t pageScanRepetitionMode, uint32_t deviceClass,
			  uint16_t clockOffset, int8_t rssi,
			  const uint8_t *extendedInquiryResponse);
void BtProcessRemoteNameResponse(BtStack *stack, int status,
				uint64_t deviceAddress, uint32_t deviceClass,
				int8_t rssi, const uint8_t *name);
int BtUpdateDeviceClass(BtDevice *device, uint32_t deviceClass);
void BtSetDeviceAuthenticationFailed(BtStack *stack, BtConnection *connection,
				     int failed);
int BtReadDeviceLinkKey(BtStack *stack, uint64_t deviceAddress, uint8_t *key);
int BtWriteDeviceLinkKey(BtStack *stack, uint64_t deviceAddress,
			 const uint8_t *key);
int BtFlushRegisteredDevices(BtStack *stack);
void BtNotifyConnectionComplete(BtConnection *connection, int status,
				uint32_t argument);
int BtAdvanceConnection(BtConnection *connection);
int BtBeginDisconnect(BtConnection *connection);
BtConnection *BtClearConnection(BtConnection *connection, int status);
int BtAcceptScoConnection(BtConnection *connection);
int BtNotifyScoConnection(BtConnection *connection, int status, int handle);
int BtInitializeSdpChannel(BtConnection *connection, BtChannel *channel);
void BtContinueHciCommands(BtStack *stack);
int BtPumpAclQueue(BtStack *stack);
void BtCompleteAclPackets(BtStack *stack, unsigned int handle, int packetCount);
int BtInitializeNetworkInterface(BtStack *stack, uint64_t localAddress);
int BtInitializeNetwork(void);
int BtIsNewCardRevision(void);
unsigned int BtCancelTimer(unsigned int id);
int BtQueueTimer(unsigned int *timer, void (*callback)(void *argument),
		 void *argument, SceInt64 delay);
void BtRoleSwitchTimedOut(void *argument);
void BtRemoteNameTimedOut(void *argument);
void BtReadAddressRetry(void *argument);
uintptr_t BtAdvanceShutdown(BtStack *stack);
uintptr_t BtCompleteConfiguration(BtStack *stack);
int BtConfigureAutoAcceptDevices(BtStack *stack);
BtStack *BtResetAutoAcceptDevices(BtStack *stack);
int BtCompleteControllerSleep(BtStack *stack);
uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
			unsigned int argument, uint64_t deviceAddress);

void BtDispatchHciEvent(BtStack *stack, uint8_t *packet, int length)
{
	BtHciEventPacket *event = (BtHciEventPacket *)packet;
	BtConnection *connection;
	BtDevice *device;
	uint64_t address;
	unsigned int count, status, handle, clockOffset, argument;
	unsigned int opcode, interval, parameterLength, deviceClass;
	unsigned int pageScanRepetitionMode;
	uint8_t key[16];
	uint8_t *parameters, *response;
	int responseLength, result, productCode, hardwareInfo;
	unsigned int vendorSetting;

	if (length <= 1)
		return;
	parameterLength = event->parameterLength;
	if (parameterLength != length - 2)
		return;
	parameters = event->parameters;
	switch (event->eventCode) {
	case 1:
		BtUnpack(parameters, parameterLength, "1", &status);
		if (!status)
			goto continueInquiry;
		stack->state = 28;
		break;
	case 2:
		if (stack->suppressInquiryResult)
			break;
		BtUnpack(parameters, parameterLength, "161--32", &count,
			 &address, &pageScanRepetitionMode, &deviceClass, &clockOffset);
		argument = -127;
		goto inquiryResult;
	case 3:
		BtUnpack(parameters, parameterLength, "126-1", &status, &handle,
			 &address, &argument);
		connection = BtFindConnection(stack, 1, address);
		if (connection) {
			if (status) {
				BtNotifyConnectionComplete(connection, (uint8_t)status, 0);
				memset(connection, 0, sizeof(*connection));
				if (stack->state == 63) {
					stack->state = 28;
					BtAdvanceShutdown(stack);
				}
			} else {
				connection->aclHandle = handle;
				if (argument == 1)
					connection->flags |= 4;
				if (connection->flags & 0x200000) {
					BtBeginDisconnect(connection);
					break;
				}
				if (!(connection->flags & 1)) {
					connection->packetType = 13080;
					BtSendHciCommand(stack, 0x040f, "22", handle, 13080);
				}
				vendorSetting = stack->vendorConnectionSetting;
				if (vendorSetting != 0xffff && stack->controllerType == 1) {
					connection->flags |= 0x400000;
					BtSendHciCommand(stack, 0xfc2b, "21",
							 connection->aclHandle, vendorSetting);
				} else {
					BtSendHciCommand(stack, 0x041f, "2", handle);
				}
			}
		}
		if (stack->state == 40)
			stack->state = 28;
		break;
	case 4:
		BtUnpack(parameters, parameterLength, "631", &address,
			 &deviceClass, &argument);
		if (argument > 1) {
			if (argument == 2)
				goto rejectSco;
			break;
		}
		connection = BtFindConnection(stack, argument == 1 ? 17 : 1, address);
		if (!connection)
			goto rejectConnection;
		result = stack->scanEnable & 1;
		device = BtGetOrCreateDevice(stack, address, result);
		if (result) {
			if (!device) {
				memset(connection, 0, sizeof(*connection));
				goto rejectConnection;
			}
		} else if (!device || !(device->flags & 0x1000)) {
			goto rejectUnregistered;
		}
		if (device->flags & 0x10000) {
rejectUnregistered:
			memset(connection, 0, sizeof(*connection));
			status = 15;
			goto rejectConnectionStatus;
		}
		BtUpdateDeviceClass(device, deviceClass);
		connection->device = device;
		if (argument == 1) {
			connection->flags |= 0x200;
			BtSendHciCommand(stack, 0x0409, "61", address, 0);
			BtNotifyEvent(8, 0, deviceClass, address);
		} else if (BtAcceptScoConnection(connection)) {
rejectSco:
			BtSendHciCommand(stack, 0x042a, "61", address, 13);
		}
		break;
rejectConnection:
		status = 13;
rejectConnectionStatus:
		BtSendHciCommand(stack, 0x040a, "61", address, status);
		BtNotifyEvent(9, (uint8_t)status, deviceClass, address);
		break;
	case 5:
		BtUnpack(parameters, parameterLength, "121", &status, &handle, &argument);
		connection = BtFindConnection(stack, 0x22, (int)handle);
		if (connection) {
			if (argument == 5)
				BtSetDeviceAuthenticationFailed(stack, connection, 1);
			BtClearConnection(connection, argument);
			if (stack->state == 65)
				BtAdvanceShutdown(stack);
		} else {
			connection = BtFindConnection(stack, 3, (int)handle);
			if (connection) {
				connection->scoHandle = 0;
				BtNotifyScoConnection(connection, (uint8_t)status, 0);
			}
		}
		break;
	case 6:
		BtUnpack(parameters, parameterLength, "12", &status, &handle);
		connection = BtFindConnection(stack, 2, (int)handle);
		if (!connection)
			break;
		if (status && status != 12) {
			/* The original reads this event-local word without writing it. */
			if (argument == 5)
				BtSetDeviceAuthenticationFailed(stack, connection, 1);
			goto authenticationFailed;
		}
		if (!(connection->flags & 2))
			connection->flags |= 2;
		BtSetDeviceAuthenticationFailed(stack, connection, 0);
		if (connection->flags & 4)
			BtAdvanceConnection(connection);
		else
			BtSendHciCommand(stack, 0x0413, "21", handle, 1);
		break;
	case 7:
		BtUnpack(parameters, parameterLength, "16", &status, &address);
		stack->nameRequestTimer = BtCancelTimer(stack->nameRequestTimer);
		if (stack->state == 32) {
			BtProcessRemoteNameResponse(stack, (uint8_t)status, address,
				stack->inquiryDeviceClass, stack->inquiryRssi, packet + 9);
			goto continueInquiry;
		}
		if (stack->state == 34)
			goto inquiryStopped;
		if (stack->state == 35) {
			BtProcessRemoteNameResponse(stack, 4, address, 0, 0, NULL);
			goto continueInquiry;
		}
		if (stack->state == 50) {
			stack->state = 28;
			if (!status) {
				connection = BtFindConnection(stack, 1, address);
				if (connection) {
					if (!packet[9]) {
						BtBeginDisconnect(connection);
						break;
					}
					if (BtUpdateUtf8String(connection->device->name,
							packet + 9, 120) > 0)
						connection->device->generation++;
					BtAdvanceConnection(connection);
				}
			}
		} else if (stack->state == 62 || stack->state == 64) {
			stack->state = 28;
			BtAdvanceShutdown(stack);
		}
		break;
continueInquiry:
		stack->state = 30;
		BtSendHciCommand(stack, 0x0401, "311", 0x9e8b33, 48, 0);
		break;
	case 8:
		BtUnpack(parameters, parameterLength, "121", &status, &handle, &argument);
		connection = BtFindConnection(stack, 2, (int)handle);
		if (!connection)
			break;
		if (status) {
authenticationFailed:
			BtNotifyConnectionComplete(connection, (uint8_t)status, 0);
			BtBeginDisconnect(connection);
		} else if (argument == 1) {
			if (!(connection->flags & 4))
				connection->flags |= 4;
			BtAdvanceConnection(connection);
		} else {
			connection->flags &= ~4u;
		}
		break;
	case 0x0b:
		BtUnpack(parameters, parameterLength, "12", &status, &handle);
		connection = BtFindConnection(stack, 2, (int)handle);
		if (!connection)
			break;
		if (status)
			goto authenticationFailed;
		BtUnpack(packet + 5, length - 5, "8", &connection->remoteFeatures);
		if (connection->remoteFeatures & UINT64_C(0x8000000000000000))
			BtSendHciCommand(stack, 0x041c, "21", connection->aclHandle, 1);
		else
			goto remoteFeaturesRead;
		break;
	case 0x0c:
		BtUnpack(parameters, parameterLength, "12", &status, &handle);
		connection = BtFindConnection(stack, 2, (int)handle);
		if (connection)
			BtSendHciCommand(stack, 0x041b, "2", handle);
		break;
	case 0x0e:
		response = packet + 6;
		responseLength = length - 6;
		BtUnpack(parameters, parameterLength, "121", &count, &opcode, &status);
		if (count > 0)
			BtContinueHciCommands(stack);
		switch (opcode) {
		case 0x0402:
			if (stack->state == 33) {
inquiryStopped:
				stack->state = 28;
				BtFlushRegisteredDevices(stack);
				BtNotifyEvent(2, 0, 0, 0);
			} else if (stack->state == 61) {
				stack->state = 28;
				BtAdvanceShutdown(stack);
			} else if (stack->state == 31) {
				stack->state = 32;
				BtSendHciCommand(stack, 0x0419, "61-2", stack->inquiryAddress,
					stack->pageScanRepetitionMode, stack->clockOffset | 0x8000);
				BtQueueTimer(&stack->nameRequestTimer, BtRemoteNameTimedOut,
					     stack, 5120000);
			}
			break;
		case 0x080f:
			if (stack->state == 12) {
				stack->state = 13;
				BtSendHciCommand(stack, 0x0c18, "2", 24000);
			}
			break;
		case 0x0c01:
			if (stack->state == 10) {
				stack->state = 11;
				if (stack->localFeatures & UINT64_C(0x1000000000000))
					argument = 2;
				else
					argument = !!(stack->localFeatures & UINT64_C(0x40000000));
				BtSendHciCommand(stack, 0x0c45, "1", argument);
			}
			break;
		case 0x0c03:
			goto controllerReset;
		case 0x0c05:
			if (stack->state == 9) {
				BtResetAutoAcceptDevices(stack);
				stack->state = 10;
				BtSendHciCommand(stack, 0x0c01, "8", UINT64_C(0x1dbff8077ffb9fff));
			} else if (stack->state == 71) {
				BtConfigureAutoAcceptDevices(stack);
			}
			break;
		case 0x0c12:
			if (stack->state == 8) {
				stack->state = 9;
				BtSendHciCommand(stack, 0x0c05, "1", 0);
			}
			break;
		case 0x0c13:
			if (stack->state != 6)
				break;
			if (stack->localFeatures & UINT64_C(0x1000000000000)) {
				BtEirRecord *services = (BtEirRecord *)stack->scratch;
				BtEirRecord *name;

				stack->state = 7;
				memset(stack->scratch, 0, 240);
				result = BtGetLocalServiceClasses(services->payload);
				services->length = result + 1;
				services->type = 3;
				name = (BtEirRecord *)(services->payload + result);
				name->length = 17;
				name->type = 9;
				memcpy(name->payload, "PlayStation Vita", 16);
				BtPack(name->payload + 16, 10, "11 2222", 9, 16,
				       2, 0x054c, 0x058c, 0x0100);
				BtSendHciCommand(stack, 0x0c52, "1#", 1, stack->scratch, 240);
			} else {
writeInquiryMode:
				stack->state = 8;
				BtSendHciCommand(stack, 0x0c12, "61", (uint64_t)0, 1);
			}
			break;
		case 0x0c16:
			if (stack->state == 14) {
				if (stack->localFeatures & UINT64_C(0x8000000000000)) {
					stack->state = 15;
					BtSendHciCommand(stack, 0x0c56, "1", 1);
				} else {
readExtendedLocalFeatures:
					stack->state = 16;
					BtSendHciCommand(stack, 0x1004, "1", 1);
				}
			} else if (stack->state == 75) {
				stack->state = 74;
				stack->scanEnable = stack->autoAcceptDeviceCount > 0 ? 2 : 0;
				BtSendHciCommand(stack, 0x0c1a, "1", stack->scanEnable);
			}
			break;
		case 0x0c18:
			if (stack->state == 13) {
				stack->state = 14;
				BtSendHciCommand(stack, 0x0c16, "2", 46400);
			}
			break;
		case 0x0c1a:
			if (stack->state == 21) {
				stack->state = 28;
				BtCompleteConfiguration(stack);
			} else if (stack->state == 60) {
				stack->state = stack->previousState;
				BtAdvanceShutdown(stack);
			} else if (stack->state == 74) {
				stack->state = 76;
				BtSendHciCommand(stack, 0xfc23, "12", 2, 1000);
				BtSendHciCommand(stack, 0xfc69, "2211", 0, 0, 4, 15);
			}
			break;
		case 0x0c1c:
			if (stack->state == 19)
				stack->state = 20;
			else if (stack->state == 72)
				stack->state = 73;
			else
				break;
			BtSendHciCommand(stack, 0x0c47, "1", 1);
			break;
		case 0x0c1e:
			if (stack->state == 17) {
				stack->state = 18;
				BtSendHciCommand(stack, 0x0c43, "1", 1);
			}
			break;
		case 0x0c24:
			if (stack->state == 5) {
				stack->state = 6;
				BtSendHciCommand(stack, 0x0c13, "N", "PlayStation Vita");
			}
			break;
		case 0x0c37:
			BtUnpack(response, responseLength, "2", &handle);
			connection = BtFindConnection(stack, 2, (int)handle);
			if (connection)
				goto startSdp;
			break;
		case 0x0c43:
			if (stack->state == 18) {
				stack->state = 19;
				BtSendHciCommand(stack, 0x0c1c, "22", 360, 18);
			}
			break;
		case 0x0c45:
			if (stack->state == 11) {
				stack->state = 12;
				BtSendHciCommand(stack, 0x080f, "2", 4);
			}
			break;
		case 0x0c47:
			if (stack->state == 20) {
				if (stack->localFeatures & UINT64_C(0x20000000000000)) {
					stack->state = 22;
					result = stack->controllerType != 1 || BtIsNewCardRevision();
					BtSendHciCommand(stack, 0x0c5b, "1", result);
				} else {
enableScan:
					stack->state = 21;
					stack->scanEnable = 2;
					BtSendHciCommand(stack, 0x0c1a, "1", 2);
				}
			} else if (stack->state == 73) {
				stack->state = 75;
				BtSendHciCommand(stack, 0x0c16, "2", 46400);
			}
			break;
		case 0x0c52:
			if (stack->state == 7)
				goto writeInquiryMode;
			break;
		case 0x0c56:
			if (stack->state == 15)
				goto readExtendedLocalFeatures;
			break;
		case 0x0c5b:
			if (stack->state == 22) {
				if (stack->controllerType == 1)
					BtSendHciCommand(stack, 0xfc1d, "1", 0);
				else
					goto enableScan;
			}
			break;
		case 0x1003:
			if (stack->state != 3)
				break;
			BtUnpack(response, responseLength, "8", &stack->localFeatures);
			if (stack->controllerType == 1) {
				BtSendHciCommand(stack, 0xfc06, "1", 240);
				break;
			}
			goto readBufferSize;
		case 0x1004:
			if (stack->state == 16) {
				stack->state = 17;
				BtSendHciCommand(stack, 0x0c1e, "22", 4096, 18);
			}
			break;
		case 0x1005:
			if (stack->state == 4) {
				BtUnpack(response, responseLength, "2122", &stack->aclPacketLength,
					 &stack->scoPacketLength, &stack->aclPacketCount,
					 &stack->scoPacketCount);
				if (stack->aclPacketCount > 256)
					stack->aclPacketCount = 256;
				BtPumpAclQueue(stack);
				stack->state = 5;
				BtSendHciCommand(stack, 0x0c24, "3", 0x3e0100);
			}
			break;
		case 0x1009:
			if (stack->state & ~2)
				break;
			BtUnpack(response, responseLength, "6", &stack->deviceAddress);
			if (!stack->deviceAddress) {
				BtQueueTimer(NULL, BtReadAddressRetry, stack, 100000);
				break;
			}
			if (!(stack->mode & 0x2000)) {
				BtInitializeNetworkInterface(stack, stack->deviceAddress);
				stack->mode |= 0x2000;
			}
			if (!(stack->mode & 0x4000)) {
				BtInitializeNetwork();
				stack->mode |= 0x4000;
			}
			stack->state = 3;
			BtSendHciCommand(stack, 0x1003, gBtControllerEmptyFormat);
			break;
		case 0x1403:
		case 0x1405:
		case 0x1406:
			BtUnpack(response, responseLength, "21", &handle, &argument);
			connection = BtFindConnection(stack, 2, (int)handle);
			if (!connection)
				break;
			if (opcode == 0x1403) {
				connection->linkQuality = argument;
				BtSendHciCommand(stack, 0x1405, "2", connection->aclHandle);
			} else if (opcode == 0x1405) {
				connection->rssi = argument;
				BtSendHciCommand(stack, 0x1406, "2", connection->aclHandle);
			} else {
				connection->afhMode = argument;
				memcpy(connection->afhMap, packet + 9, 10);
				connection->statistics.queryTime = BtGetSystemTimeWide();
			}
			break;
		case 0xfc06:
readBufferSize:
			stack->state = 4;
			BtSendHciCommand(stack, 0x1005, gBtControllerEmptyFormat);
			break;
		case 0xfc0f:
			stack->firmwareVersion = packet[9] | (packet[6] << 8) |
				(packet[7] << 16) | ((unsigned int)packet[8] << 24);
			if (BtIsNewCardRevision()) {
				BtSendHciCommand(stack, 0xfc68, "1", 15);
				break;
			}
			goto configureVendorController;
		case 0xfc1d:
			BtSendHciCommand(stack, 0xfc0f, gBtControllerEmptyFormat);
			break;
		case 0xfc23:
			if (stack->state == 23) {
				stack->state = 24;
				BtSendHciCommand(stack, 0xfc47, "111", 1, 2, 0);
			}
			break;
		case 0xfc2b:
		case 0xfc2c:
			if (stack->state == 27)
				goto enableScan;
			if (opcode == 0xfc2c)
				BtUnpack(response, responseLength, "21", &handle, &argument);
			else {
				BtUnpack(response, responseLength, "2", &handle);
				argument = 0;
			}
			if (handle) {
				connection = BtFindConnection(stack, 2, (int)handle);
				if (!connection)
					break;
				if (connection->flags & 0x400000) {
					connection->flags &= ~0x400000u;
					BtSendHciCommand(stack, 0x041f, "2", handle);
				} else {
					BtNotifyEvent(20, opcode == 0xfc2c ? 6 : 5,
						status | (argument << 8), connection->deviceAddress);
				}
			} else {
				BtNotifyEvent(20, opcode == 0xfc2c ? 6 : 5,
					status | (argument << 8), 0);
			}
			break;
		case 0xfc47:
			if (stack->state == 24) {
				if (stack->mode & 0x10000) {
					stack->state = 25;
					BtSendHciCommand(stack, 0xfc47, "111", 1, 1, 0);
					break;
				}
			} else if (stack->state == 25) {
				stack->state = 26;
				BtSendHciCommand(stack, 0xfc47, "111", 2, 2, 0);
				break;
			} else if (stack->state != 26) {
				break;
			}
			productCode = ksceSblAimgrGetProductCode();
			hardwareInfo = ksceSysconGetHardwareInfo() & 0xfd0000;
			if (hardwareInfo == 0x800000)
				vendorSetting = 0xfff8;
			else if (hardwareInfo == 0x700000)
				vendorSetting = 0xfffc;
			else
				vendorSetting = 0xffff;
			stack->vendorConnectionSetting = vendorSetting;
			if ((unsigned int)(productCode - 0x102) > 1 &&
			    !(stack->mode & 0x1000) && vendorSetting != 0xffff &&
			    stack->controllerType == 1) {
				stack->state = 27;
				BtSendHciCommand(stack, 0xfc2b, "21", 0, vendorSetting);
			} else {
				goto enableScan;
			}
			break;
		case 0xfc59:
			if (stack->state == 77) {
				stack->state = 78;
				stack->mode |= 0x200;
				BtSendHciCommand(stack, 0xfc5a, gBtControllerEmptyFormat);
			}
			break;
		case 0xfc5a:
			if (stack->state == 78) {
				stack->state = 79;
				stack->mode |= 0x400;
			}
			break;
		case 0xfc68:
configureVendorController:
			stack->state = 23;
			BtSendHciCommand(stack, 0xfc23, "12", 2, 10000);
			break;
		case 0xfc69:
			if (stack->state == 76) {
				stack->state = 77;
				BtSendHciCommand(stack, 0xfc59, "11", 16, 255);
			}
			break;
		}
		break;
	case 0x0f:
		BtUnpack(parameters, parameterLength, "112", &status, &count, &opcode);
		if (count > 0)
			BtContinueHciCommands(stack);
		if (!opcode) {
controllerReset:
			if (stack->state == 0) {
				stack->state = 2;
				BtSendHciCommand(stack, 0x1009, gBtControllerEmptyFormat);
			}
		}
		break;
	case 0x12:
		BtUnpack(parameters, parameterLength, "161", &status, &address, &argument);
		connection = BtFindConnection(stack, 1, address);
		if (!connection)
			break;
		if (status || argument) {
			connection->flags |= 0x200;
			if (connection->flags & 0x100000) {
				connection->authenticationRetries++;
				if (connection->authenticationRetries <= 9)
					BtQueueTimer(&connection->authenticationTimer,
						BtRoleSwitchTimedOut, connection, 100000);
				else
					BtBeginDisconnect(connection);
			}
		} else if (connection->flags & 0x100000) {
			connection->flags &= ~0x100200u;
			connection->authenticationTimer = BtCancelTimer(connection->authenticationTimer);
			BtAdvanceConnection(connection);
		} else {
			connection->flags &= ~0x200u;
		}
		break;
	case 0x13:
		BtUnpack(parameters, parameterLength, "122", &count, &handle, &argument);
		BtCompleteAclPackets(stack, (uint16_t)handle & 0xfff, (uint16_t)argument);
		break;
	case 0x14:
		BtUnpack(parameters, parameterLength, "1212", &status, &handle, &argument, &interval);
		connection = BtFindConnection(stack, 2, (int)handle);
		if (!connection)
			break;
		if (argument == 2) {
			if (interval > 800)
				BtSendHciCommand(stack, 0x0804, "2", handle);
		} else if (!argument && connection->linkMode == 2 && connection->interval > 800) {
			result = 2 * gBtSniffIntervals[connection - stack->connections];
			BtSendHciCommand(stack, 0x0803, "22222", handle, result, result, 4, argument);
		}
		connection->linkMode = argument;
		connection->interval = interval;
		break;
	case 0x16:
		BtUnpack(parameters, parameterLength, "6", &address);
		connection = BtFindConnection(stack, 1, address);
		if (connection && connection->device && connection->device->vendorId == 1452 &&
		    (unsigned int)(connection->device->productId - 780) <= 1) {
			BtSendHciCommand(stack, 0x040d, "6P", address, 4, "0000");
		} else if (BtReadDeviceLinkKey(stack, address, key)) {
			connection = BtFindConnection(stack, 1, address);
			if (connection)
				BtNotifyEvent(3, 0, (connection->device->flags >> 6) & 1, address);
		} else {
			BtSendHciCommand(stack, 0x040e, "6", address);
		}
		break;
	case 0x17:
		BtUnpack(parameters, parameterLength, "6", &address);
		if (BtReadDeviceLinkKey(stack, address, key)) {
			BtSendHciCommand(stack, 0x040c, "6", address);
		} else if (BtFindConnection(stack, 1, address)) {
			BtSendHciCommand(stack, 0x040b, "6#", address, key, 16);
		}
		break;
	case 0x18:
		BtUnpack(parameters, parameterLength, "6#", &address, key, 16);
		if (BtFindConnection(stack, 1, address))
			BtWriteDeviceLinkKey(stack, address, key);
		break;
	case 0x1b:
		BtUnpack(parameters, parameterLength, "21", &handle, &argument);
		connection = BtFindConnection(stack, 2, (int)handle);
		if (connection)
			connection->linkState = argument;
		break;
	case 0x1c:
		BtUnpack(parameters, parameterLength, "122", &status, &handle, &clockOffset);
		if (!status) {
			connection = BtFindConnection(stack, 2, (int)handle);
			if (connection) {
				if (stack->controllerType == 1) {
					BtSendHciCommand(connection->stack, 0xfc1e, "1 888", 1,
						UINT64_C(0x3131313131313131), UINT64_C(0),
						UINT64_C(0x0c0c0c0c0c0c0c0c));
					connection->avdtp.controllerSleepState = 1;
					BtSendHciCommand(stack, 0xfc24, "21", connection->aclHandle, 1);
				}
				device = BtGetOrCreateDevice(stack, connection->deviceAddress, 0);
				if (device)
					device->clockOffset = clockOffset | 0x8000;
			}
		}
		BtSendHciCommand(stack, 0x041d, "2", handle);
		break;
	case 0x22:
		if (stack->suppressInquiryResult)
			break;
		BtUnpack(parameters, parameterLength, "161-321", &count, &address,
			 &pageScanRepetitionMode, &deviceClass, &clockOffset, &argument);
inquiryResult:
		if (count == 1 && BtProcessInquiryResult(stack, address,
			(uint8_t)pageScanRepetitionMode, deviceClass,
			clockOffset | 0x8000, (int8_t)argument, NULL))
			goto requestRemoteName;
		break;
	case 0x23:
		BtUnpack(parameters, parameterLength, "12", &status, &handle);
		connection = BtFindConnection(stack, 2, (int)handle);
		if (!connection)
			break;
		BtUnpack(packet + 7, length - 7, "8", &connection->remoteExtendedFeatures);
remoteFeaturesRead:
		if (connection->flags & 0x200) {
startSdp:
			if (connection->flags & 1)
				BtInitializeSdpChannel(connection, NULL);
		} else {
			BtSendHciCommand(stack, 0x0c37, "22", handle, 8000);
		}
		break;
	case 0x2c:
		BtUnpack(parameters, parameterLength, "126", &status, &handle, &address);
		connection = BtFindConnection(stack, 1, address);
		if (connection)
			BtNotifyScoConnection(connection, (uint8_t)status, (uint16_t)handle);
		break;
	case 0x2f:
		BtUnpack(parameters, parameterLength, "161-321#", &count, &address,
			 &pageScanRepetitionMode, &deviceClass, &clockOffset,
			 &argument, stack->scratch, 240);
		result = BtProcessInquiryResult(stack, address, (uint8_t)pageScanRepetitionMode,
			deviceClass, clockOffset | 0x8000, (int8_t)argument, stack->scratch);
		if (result && !stack->suppressInquiryResult) {
requestRemoteName:
			stack->pageScanRepetitionMode = pageScanRepetitionMode;
			stack->inquiryDeviceClass = deviceClass;
			stack->inquiryAddress = address;
			stack->clockOffset = clockOffset;
			stack->inquiryRssi = argument;
			stack->state = 31;
			BtSendHciCommand(stack, 0x0402, gBtControllerEmptyFormat);
		}
		break;
	case 0x31:
		BtUnpack(parameters, parameterLength, "6", &address);
		if (BtFindConnection(stack, 1, address))
			BtSendHciCommand(stack, 0x042b, "6111", address, 5, 1, 0);
		break;
	case 0x32:
		BtUnpack(parameters, parameterLength, "61", &address, &argument);
		connection = BtFindConnection(stack, 1, address);
		if (connection)
			connection->sspIoCapability = argument;
		break;
	case 0x33:
	case 0x3b:
		BtUnpack(parameters, parameterLength, "64", &address, &argument);
		connection = BtFindConnection(stack, 1, address);
		if (connection)
			BtNotifyEvent(4, connection->sspIoCapability, argument, connection->deviceAddress);
		break;
	case 0x3c:
		BtUnpack(parameters, parameterLength, "61", &address, &argument);
		connection = BtFindConnection(stack, 1, address);
		if (connection)
			BtNotifyEvent(29, 0, argument, connection->deviceAddress);
		break;
	case 0xff:
		if (stack->controllerType != 1 || !parameterLength)
			break;
		if (parameters[0] == 91) {
			if (stack->state == 1) {
				stack->state = 2;
				BtSendHciCommand(stack, 0x1009, gBtControllerEmptyFormat);
			} else if (stack->state == 70) {
				stack->state = 71;
				BtConfigureAutoAcceptDevices(stack);
			} else {
				goto vendorEventTail;
			}
		}
		if (parameters[0] == 90) {
			if (stack->state == 79)
				BtCompleteControllerSleep(stack);
			else
				goto vendorEventTail;
		}
		if (parameters[0] == 35 && stack->state == 66)
			BtAdvanceShutdown(stack);
vendorEventTail:
		if (parameterLength != 1 && parameters[0] == 32 && parameters[1] == 1 &&
		    stack->state == 67)
			BtAdvanceShutdown(stack);
		break;
	}
}
