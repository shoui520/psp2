/* Private USB attachment records owned by SceDs3. */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <usbd/usbd.h>
#include <bt/bt_hid.h>
#include <threadmgr/fast_mutex_object.h>

#define DS3_USB_DEVICES 4
#define DS3_USB_DEVICE_UNUSED 0x0fffffff

typedef struct Ds3UsbDevice {
	int deviceId;
	uint8_t linkKey[16];
	int controlPipe;
	int inputPipe;
	int outputPipe;
	int inputPacketSize;
	int interfaceNumber;
	int alternateSetting;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t deviceAddress[6];
	uint8_t hostAddress[6];
	uint8_t suppressPsUntilRelease;
	uint8_t psSuppressionCount;
} Ds3UsbDevice;

_Static_assert(sizeof(Ds3UsbDevice) == 64, "USB attachment record size");
_Static_assert(offsetof(Ds3UsbDevice, deviceAddress) == 50,
	"USB controller address");

typedef struct Ds3UsbState {
	Ds3UsbDevice devices[DS3_USB_DEVICES];
	int inputLock;
	uint8_t localAddress[6];
	uint8_t reserved[2];
} Ds3UsbState;

_Static_assert(sizeof(Ds3UsbState) == 268, "USB state size");
_Static_assert(offsetof(Ds3UsbState, localAddress) == 260,
	"USB local Bluetooth address");

int ds3IsSafeMode(void);
int ds3RemoveTrackedAddress(uint64_t address);
int ds3ReleaseDevice(int transport, int deviceId, uint64_t address);

/* Private controller records shared by the USB and Bluetooth paths. */
typedef struct Ds3LedPattern {
	uint16_t duration;
	uint8_t period;
	uint8_t offTime;
	uint8_t onTime;
	uint8_t reserved[3];
} Ds3LedPattern;

typedef struct Ds3LedState {
	Ds3LedPattern patterns[4];
	uint8_t mask;
	uint8_t lightbarColor[3];
	uint8_t reserved[4];
} Ds3LedState;

typedef struct Ds3ActuatorState {
	uint64_t timestamp;
	uint8_t smallMotor;
	uint8_t largeMotor;
	uint8_t auxiliaryMotor;
	uint8_t reserved[5];
} Ds3ActuatorState;

typedef struct Ds3Device {
	uint64_t deviceAddress;
	uint64_t lastActivityTime;
	uint64_t lastTouchTime;
	Ds3LedState ledState;
	Ds3ActuatorState actuatorState;
	uint16_t ds3Calibration[9];
	uint8_t auxiliaryMotorLevel;
	uint8_t auxiliaryMotorReport;
	uint8_t reservedCalibration[4];
	int16_t ds4Calibration[18];
	SceBtHidRequest *inputRequests[2];
	SceBtHidRequest *outputRequests[2];
	int usbDeviceId;
	int setupStage;
	uint32_t buttons;
	uint32_t previousButtons;
	uint16_t idProduct;
	uint16_t sampleSequence;
	uint8_t inputIndex;
	uint8_t outputIndex;
	uint8_t outputPending;
	int8_t port;
	uint8_t transport;
	uint8_t deviceType;
	uint8_t deviceSubtype;
	uint8_t connectionStatus;
	uint8_t batteryInfo;
	uint8_t inputReady;
	uint8_t pendingWork;
	uint8_t setupRetries;
	uint8_t usbState;
	uint8_t bluetoothState;
	uint8_t activityFlag;
	uint8_t powerStateChecked;
} Ds3Device;

_Static_assert(sizeof(Ds3LedPattern) == 8, "LED pattern size");
_Static_assert(sizeof(Ds3Device) == 192, "Controller record size");
_Static_assert(offsetof(Ds3Device, inputRequests) == 140,
	"Controller input requests");
_Static_assert(offsetof(Ds3Device, usbState) == 188,
	"Controller USB state");

int ds3AllocateDevice(int transport, int deviceId, uint64_t address);
Ds3Device *ds3FindUsbDevice(int deviceId);
void ds3UsbSetupComplete(int result, int transferred, void *argument);
void ds3UsbInputComplete(int result, int transferred, void *argument);

Ds3Device *ds3FindDeviceByPort(int port);

typedef union Ds3UsbInputReport {
	uint8_t bytes[64];
	uint32_t words[16];
} Ds3UsbInputReport;

_Static_assert(sizeof(Ds3UsbInputReport) == 64, "USB input report size");

_Static_assert(sizeof(Ds3LedState) == 40, "LED state size");
Ds3Device *ds3BuildUsbOutputReport(Ds3Device *controller, uint8_t *report);
Ds3Device *ds3UpdateUsbOutputState(Ds3Device *controller, const uint8_t *report);

_Static_assert(sizeof(Ds3ActuatorState) == 16, "Actuator state size");

int ds3IsDs4UsbEnabled(void);
int ds3IsSuspending(void);
Ds3Device *ds3FindBtDevice(uint64_t address);
int ds3BuildLedState(int mask, int mode, Ds3LedState *leds);
int ds3DetachBluetoothTransport(uint64_t address);
int ds3UnpairUsbDevice(int deviceId);
int ds3AssignPort(uint64_t address, int8_t *port, uint8_t deviceType);
int ds3SignalPortChange(void);
Ds3Device *ds3UpdateBatteryInfo(Ds3Device *controller, uint8_t battery);

int ds3UsbProbe(int deviceId);
int ds3UsbAttach(int deviceId);
int ds3UsbDetach(int deviceId);
int ds3UsbSetHostAddress(Ds3Device *controller);
int ds3UsbGetFeatureReport(Ds3Device *controller, uint16_t reportId,
	uint16_t length);
int ds3UsbReadInput(unsigned int port, Ds3UsbInputReport *report);
int ds3UsbSetLeds(Ds3Device *controller, const Ds3LedState *state);
void ds3UsbAdvanceSetup(Ds3Device *controller, int transferred);
int ds3UsbSetActuators(Ds3Device *controller, const Ds3ActuatorState *state);
int ds3UsbSetAuxiliaryMotor(Ds3Device *controller);
int ds3UsbSetLightbar(Ds3Device *controller, const Ds3LedState *state);
int ds3UsbSetDs4Actuators(Ds3Device *controller, const Ds3ActuatorState *state);
int ds3UsbSetDs3Control(Ds3Device *controller, uint8_t control);
int ds3UsbSetDs4Control(Ds3Device *controller, uint8_t control);
int ds3UsbPollBattery(void);
int ds3UsbRegister(void);
int ds3GetLocalAddress(uint8_t *address);
int ds3DisconnectTrackedDevice(void);
int ds3SwapTrackedPort(uint64_t address, uint8_t port);

#define DS3_CONTROLLERS 8

typedef struct Ds3LightbarOverride {
	uint8_t color[3];
	uint8_t enabled;
} Ds3LightbarOverride;

typedef struct Ds3ProcessColors {
	SceUID pid;
	Ds3LightbarOverride ports[4];
} Ds3ProcessColors;

typedef struct Ds3State {
	Ds3Device devices[DS3_CONTROLLERS];
	uint64_t idleTimeout;
	SceUID bluetoothCallback;
	SceUID workerThread;
	SceUID workEvent;
	SceUID heap;
	SceUID foregroundProcess;
	SceUID singleControllerProcess;
	Ds3ProcessColors processColors[8];
	SceKernelSpinlock lock;
	struct ThreadmgrFastMutex stateMutex;
	struct ThreadmgrFastMutex registrationMutex;
	uint8_t usbDeviceCount;
	uint8_t pairedUsbDeviceCount;
	uint8_t bluetoothDeviceCount;
	uint8_t activeDeviceCount;
	uint8_t localAddress[6];
	uint8_t ds4UsbEnabled;
	uint8_t suspending;
	uint8_t singleControllerMode;
	uint8_t safeMode;
	uint8_t reserved[6];
} Ds3State;

_Static_assert(sizeof(Ds3LightbarOverride) == 4, "Lightbar override size");
_Static_assert(sizeof(Ds3ProcessColors) == 20, "Process color state size");
_Static_assert(sizeof(Ds3State) == 1880, "Controller manager size");
_Static_assert(offsetof(Ds3State, idleTimeout) == 1536, "Controller idle timeout");
_Static_assert(offsetof(Ds3State, stateMutex) == 1732, "Controller state mutex");
_Static_assert(offsetof(Ds3State, localAddress) == 1864, "Controller local address");

/* Private port assignments and their notification thread. */
typedef struct Ds3TrackedPort {
	uint64_t deviceAddress;
	uint8_t reserved;
	uint8_t deviceType;
	uint8_t reservedTail[6];
} Ds3TrackedPort;

typedef struct Ds3PortManager {
	Ds3TrackedPort ports[4];
	SceUID workerThread;
	SceUID workEvent;
	struct ThreadmgrFastMutex mutex;
} Ds3PortManager;

_Static_assert(sizeof(Ds3TrackedPort) == 16, "Tracked controller port size");
_Static_assert(sizeof(Ds3PortManager) == 136, "Port manager size");
_Static_assert(offsetof(Ds3PortManager, mutex) == 72, "Port manager mutex");
int ds3QueueLedState(int port, const Ds3LedState *state);
int ds3QueueLightbarState(int port, const Ds3LedState *state);
int ds3IsUsbCharging(void);
int ds3IsDs4Port(int port);
int ds3GetBatteryInfo(int port, uint8_t *battery);
int ds3PortManagerThread(SceSize args, void *argp);
int ds3InitializePortManager(void);
int ds3InitializeBdRemocon(void);
int _sceDs3BtCb(int notifyId, int notifyCount, int notifyArg, void *userData);

typedef struct Ds3BdRemocon {
	uint8_t inputReports[2][64];
	uint64_t deviceAddress;
	SceUID callback;
	SceUID workerThread;
	SceUID releaseTimer;
	SceBtHidRequest inputRequests[2];
	uint32_t buttonAssign;
	uint32_t buttons;
	uint32_t state;
	uint32_t connected;
	uint32_t inputIndex;
	uint32_t inputReady;
	uint32_t lastVcount;
	uint8_t reserved[8];
} Ds3BdRemocon;

_Static_assert(sizeof(Ds3BdRemocon) == 232, "BD remote state size");
_Static_assert(offsetof(Ds3BdRemocon, inputRequests) == 148,
	       "BD remote input requests");
_Static_assert(offsetof(Ds3BdRemocon, buttonAssign) == 196,
	       "BD remote button assignment");
int ds3BdRemoconThread(SceSize args, void *argp);
int ds3BdRemoconCallback(int notifyId, int notifyCount, int notifyArg, void *userData);
int ds3BdRemoconSysEvent(int resume, int eventId, void *args, void *opt);
