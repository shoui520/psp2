#pragma once

#include <sysmem/procevent.h>
#include <sysmem/sysroot_hid.h>
#include <bt/bt_hid.h>
#include <bt/bt_event.h>
#include <bt/bt_registered.h>
#include "hid.h"
#include <sysmem/list.h>
#include <sysmem/cpu.h>

typedef struct HidCollection HidCollection;
typedef struct HidProcessContext HidProcessContext;

typedef struct HidCollectionInfo {
	SceSize size;
	int id;
	int type;
} HidCollectionInfo;

_Static_assert(sizeof(HidCollectionInfo) == 12, "HID collection diagnostic record");

typedef struct HidGlobalState {
	uint16_t usagePage;
	int logicalMinimum;
	int logicalMaximum;
	int physicalMinimum;
	int physicalMaximum;
	int reportSize;
	uint8_t reportId;
	int reportCount;
} HidGlobalState;

typedef struct HidLocalState {
	uint16_t usages[32];
	uint16_t usageMinimum;
	uint16_t usageMaximum;
} HidLocalState;

_Static_assert(sizeof(HidGlobalState) == 32, "HID global parser state");
_Static_assert(offsetof(HidGlobalState, logicalMinimum) == 4,
	       "HID logical minimum");
_Static_assert(offsetof(HidGlobalState, reportId) == 24, "HID report ID");
_Static_assert(sizeof(HidLocalState) == 68, "HID local parser state");

typedef struct HidReportCursor {
	int pendingCount;
	int readIndex;
	SceKernelSpinlock spinlock;
	int peekCount;
} HidReportCursor;

typedef struct HidKeyboardIntercept {
	uint8_t pending;
	uint8_t enabled;
	uint8_t pendingEnabled;
} HidKeyboardIntercept;

struct HidProcessContext {
	SceUID pid;
	unsigned int flags;
	SceKernelSpinlock spinlock;
	HidReportCursor reports[24];
	unsigned int privilegeFlags;
	HidKeyboardIntercept keyboardIntercept;
};

_Static_assert(sizeof(HidReportCursor) == 16, "HID per-process report cursor");
_Static_assert(sizeof(HidProcessContext) == 404, "HID process context");
_Static_assert(offsetof(HidProcessContext, reports) == 12, "HID report cursors");
_Static_assert(offsetof(HidProcessContext, privilegeFlags) == 396,
	       "HID process privileges");
_Static_assert(offsetof(HidProcessContext, keyboardIntercept) == 400,
	       "HID keyboard interception state");

typedef struct HidProcessLink {
	ListNode link;
	HidProcessContext *process;
} HidProcessLink;

_Static_assert(sizeof(HidProcessLink) == 12, "HID process-list node");

typedef struct HidHintCallback {
	ListNode link;
	SceUID callbackId;
	SceUID pid;
} HidHintCallback;

_Static_assert(sizeof(HidHintCallback) == 16, "HID hint-callback node");

typedef struct HidHintCallbacks {
	HidHintCallback enumeration;
	HidHintCallback read;
} HidHintCallbacks;

_Static_assert(sizeof(HidHintCallbacks) == 32, "HID paired hint-callback heads");

typedef struct HidDevice {
	unsigned int transportType;
	uint64_t deviceAddress;
	uint16_t vendorId;
	uint16_t productId;
	uint16_t version;
	uint8_t primary;
	char name[256];
	int descriptorLength;
	uint8_t descriptor[512];
	int usesReportIds;
	HidCollection *collections[8];
	SceBtHidRequest *readRequests[2];
	SceBtHidRequest *writeRequest;
	SceBtHidRequest *pendingWriteRequest;
} HidDevice;

_Static_assert(sizeof(HidDevice) == 848, "HidDevice size");
_Static_assert(offsetof(HidDevice, deviceAddress) == 8, "HidDevice address");
_Static_assert(offsetof(HidDevice, name) == 23, "HidDevice name");
_Static_assert(offsetof(HidDevice, descriptorLength) == 280, "descriptor length");
_Static_assert(offsetof(HidDevice, descriptor) == 284, "descriptor bytes");
_Static_assert(offsetof(HidDevice, usesReportIds) == 796, "report ID flag");
_Static_assert(offsetof(HidDevice, collections) == 800, "HID collections");
_Static_assert(offsetof(HidDevice, readRequests) == 832, "input transfers");
_Static_assert(offsetof(HidDevice, pendingWriteRequest) == 844, "pending output");

typedef struct HidReportOffset {
	uint8_t reportId;
	int bitOffset;
} HidReportOffset;

typedef struct HidBitField {
	int present;
	uint8_t reportId;
	int bitOffset;
} HidBitField;

typedef struct HidMouseAxis {
	int present;
	uint8_t reportId;
	int bitOffset;
	int bitSize;
	int signExtend;
} HidMouseAxis;

typedef struct HidArrayField {
	int present;
	uint8_t reportId;
	int bitOffset;
	int bitSize;
	int count;
} HidArrayField;

typedef struct HidLedField {
	int present;
	uint8_t reportId;
} HidLedField;

typedef struct HidConsumerBitFields {
	int present;
	uint16_t usages[32];
	uint8_t reportId;
	int bitOffset;
	int count;
} HidConsumerBitFields;

typedef struct HidReportEntry {
	uint64_t inputTimestamp;
	union {
		SceHidKeyboardReport keyboard;
		SceHidMouseReport mouse;
		SceHidConsumerControlReport consumer;
	} report;
	unsigned int flags;
} HidReportEntry;

struct HidCollection {
	int id;
	int type;
	HidDevice *device;
	HidReportOffset reportOffsets[8];
	union {
		struct {
			HidBitField buttons[5];
			HidMouseAxis xAxis;
			HidMouseAxis yAxis;
			HidMouseAxis wheel;
			HidMouseAxis pan;
		} mouse;
		struct {
			HidBitField modifiers[8];
			HidLedField leds;
			HidArrayField keys;
		} keyboard;
		struct {
			int mode;
			union {
				HidArrayField array;
				HidConsumerBitFields groups[8];
			} fields;
		} consumer;
	} descriptor;
	int capsLockPressed;
	int numLockPressed;
	int scrollLockPressed;
	uint8_t ledState;
	int writeIndex;
	HidReportEntry reports[16];
};

_Static_assert(sizeof(HidReportEntry) == 40, "HID report ring entry");
_Static_assert(sizeof(HidCollection) == 1384, "HID collection");
_Static_assert(offsetof(HidCollection, descriptor) == 76, "HID parsed fields");
_Static_assert(offsetof(HidCollection, capsLockPressed) == 720, "HID lock keys");
_Static_assert(offsetof(HidCollection, ledState) == 732, "HID LED state");
_Static_assert(offsetof(HidCollection, writeIndex) == 736, "HID ring write index");
_Static_assert(offsetof(HidCollection, reports) == 744, "HID report ring");

typedef struct HidReportRouting {
	int pendingOwner;
	SceUID ownerPid;
	SceUID pendingOwnerPid;
	int allKeysReleased;
} HidReportRouting;

_Static_assert(sizeof(HidReportRouting) == 16, "HID report routing");

extern unsigned int hidFlags;
extern SceKernelSpinlock hidSpinlock;
extern const SceKernelProcEventHandler hidProcessHandlers;
extern const SysrootHidOps hidSysrootOps;
extern HidCollection *hidCollections[24];
extern HidReportRouting hidRouting[3];
int hidReadCollectionReports(HidCollection *collection, void *buffer, int count);
int hidClearCollectionReports(HidCollection *collection);

int hidBluetoothThread(SceSize args, void *argp);
unsigned int hidGetFlags(unsigned int mask);
int hidSystemEvent(int resume, int eventId, void *args, void *opt);
int hidInitializeMemory(void);
int hidLock(void);
int hidUnlock(void);
void *hidAllocateMemory(SceSize size);
int hidFreeMemory(void *memory);
void hidReleaseCollection(HidCollection *collection, int notify);
HidCollection *hidCreateCollection(const HidGlobalState *global,
				   const HidLocalState *local);
void hidParseInputItem(HidCollection *collection, const HidGlobalState *global,
		       const HidLocalState *local, unsigned int flags);
void hidParseOutputItem(HidCollection *collection, const HidGlobalState *global,
			const HidLocalState *local, unsigned int flags);
int hidAttachCollection(HidCollection *collection, HidDevice *device);
int hidIsSupportedCollection(HidCollection *collection);
int hidGetCollectionInfo(const HidCollection *collection, HidCollectionInfo *info);
void hidConnectBluetoothDevice(uint64_t address);
int hidDisconnectBluetoothDevice(uint64_t address);
void hidReceiveBluetoothReport(uint64_t address, int status,
			       SceBtHidRequest *request, uint64_t timestamp);
int hidCompleteBluetoothWrite(uint64_t address, int status,
			      SceBtHidRequest *completedRequest);
int hidCollectionHasReportId(HidCollection *collection, unsigned int reportId);
void hidProcessReport(HidCollection *collection, unsigned int reportId,
		      const uint8_t *data, SceSize length, uint64_t timestamp);
uintptr_t hidWriteBluetoothReport(HidDevice *device, int reportId,
				 const void *data, SceSize size);
void hidInitializeReportLists(void);
int hidGetCollectionWriteIndex(unsigned int index);
HidReportCursor *hidGetProcessReportCursor(unsigned int index);
void hidResetProcessIteration(void);
HidReportCursor *hidNextProcessReportCursor(unsigned int index);
int hidInitializeProcessList(void);
int hidIsProcessStopped(SceUID pid);
int hidHasProcess(SceUID pid);
int hidGetProcessPrivileges(SceUID pid, unsigned int *privileges);
int hidGetKeyboardIntercept(SceUID pid, HidKeyboardIntercept *intercept);
int hidSetKeyboardIntercept(SceUID pid, HidKeyboardIntercept intercept);
void hidReleaseProcessCallbacks(SceUID pid);
void hidNotifyMouseEnumeration(void);
void hidNotifyMouseRead(void);
void hidNotifyKeyboardEnumeration(void);
void hidNotifyKeyboardRead(void);
void hidNotifyConsumerEnumeration(void);
void hidNotifyConsumerRead(void);
extern HidProcessLink hidProcessHead;
int hidProcessCreate(SceUID pid, ProcEventCreateParam *param, int arg);
int hidProcessExit(SceUID pid, ProcEventParam *param, int arg);
int hidProcessKill(SceUID pid, ProcEventParam *param, int arg);
int hidProcessStop(SceUID pid, int eventType, ProcEventParam *param,
		   int arg);
int processResumeStart(SceUID pid, int eventType,
		       ProcEventParam *param, int arg);
int hidDumpDevices(void *buffer, SceSize size, SceUInt32 *count);
int hidDumpLists(void *buffer, SceSize size, SceUInt32 *count);
