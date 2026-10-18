/* OV7735 sensor register access and control. */

#include "camera_config.h"
#include <psp2kern/kernel/threadmgr.h>
#include <lowio/i2c.h>
#include <lowio/gpio.h>
#include <string.h>

int cameraHasStartedProcess(int devnum, int activeOnly);

typedef struct CameraSensorInfo {
	SceUInt16 chipId;
	SceUInt16 revision;
	SceUInt16 probeResult;
	SceUInt16 reserved;
	int controlMode;
	int useExtendedTiming;
} CameraSensorInfo;

typedef struct CameraSensorTiming {
	CameraSensorRegister registers[15];
} CameraSensorTiming;

typedef struct CameraSavedSensorState {
	union {
		struct {
			CameraSensorRegister colorAdjustment[3];
			CameraSensorRegister exposureControl;
			CameraSensorRegister exposure[3];
			CameraSensorRegister gain[2];
			CameraSensorRegister whiteBalanceControl[2];
			CameraSensorRegister whiteBalance[6];
			CameraSensorRegister whiteBalanceTail[3];
		};
		CameraSensorRegister registers[20];
	};
} CameraSavedSensorState;

typedef struct CameraExposureGap {
	int framerate;
	unsigned int dwExposureTimeGap;
} CameraExposureGap;

static const CameraSensorRegister cameraResolution8Registers[30] = {
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x8b, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xe7, 0xff},
	{0x3808, 0x2, 0xff},
	{0x3809, 0x80, 0xff},
	{0x380a, 0x1, 0xff},
	{0x380b, 0x68, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x4, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x40, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0x14, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3814, 0x11, 0xff},
	{0x3815, 0x11, 0xff},
	{0x3820, 0x0, 0xff},
	{0x3821, 0x10, 0xff},
	{0x3622, 0x0, 0xff},
	{0x3660, 0x90, 0xff},
	{0x4001, 0x2, 0xff},
	{0x4004, 0x4, 0xff},
	{0x4608, 0x0, 0xff},
	{0x4609, 0x80, 0xff},
};

static const CameraSensorRegister cameraAntiflicker50Registers[2] = {
	{0x3c01, 0x80, 0x80},
	{0x3c00, 0x4, 0x4},
};

static const CameraSensorRegister cameraResolution3Registers[30] = {
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x8b, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xe7, 0xff},
	{0x3808, 0x0, 0xff},
	{0x3809, 0xa0, 0xff},
	{0x380a, 0x0, 0xff},
	{0x380b, 0x78, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x4, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x4, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0x14, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3814, 0x11, 0xff},
	{0x3815, 0x11, 0xff},
	{0x3820, 0x0, 0xff},
	{0x3821, 0x10, 0xff},
	{0x3622, 0x0, 0xff},
	{0x3660, 0x90, 0xff},
	{0x4001, 0x2, 0xff},
	{0x4004, 0x4, 0xff},
	{0x4608, 0x2, 0xff},
	{0x4609, 0x60, 0xff},
};

static const CameraSensorRegister cameraEv7Registers[4] = {
	{0x3a0f, 0x72, 0xff},
	{0x3a10, 0x69, 0xff},
	{0x3a1b, 0x73, 0xff},
	{0x3a1e, 0x68, 0xff},
};

static const CameraSensorRegister cameraAntiflicker60Registers[2] = {
	{0x3c01, 0x80, 0x80},
	{0x3c00, 0x0, 0x4},
};

static const CameraSensorRegister cameraIsoApplyRegisters[4] = {
	{0x3208, 0x0, 0xff},
	{0x3503, 0x0, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static const CameraSensorRegister cameraIso800Registers[6] = {
	{0x3503, 0x7, 0xff},
	{0x350a, 0x0, 0x3},
	{0x350b, 0x68, 0xff},
	{0x3a13, 0xe8, 0xff},
	{0x3a18, 0x0, 0xff},
	{0x3a19, 0x9e, 0xff},
};

static const CameraSensorRegister cameraSensorCommonRegisters[128] = {
	{0x3037, 0x2, 0xff},
	{0x3039, 0x0, 0xff},
	{0x3034, 0x18, 0xff},
	{0x3036, 0x40, 0xff},
	{0x3035, 0x41, 0xff},
	{0x3662, 0x10, 0xff},
	{0x3106, 0x9, 0xff},
	{0x3020, 0x31, 0xff},
	{0x3663, 0x1, 0xff},
	{0x3007, 0x80, 0xff},
	{0x303d, 0x20, 0xff},
	{0x303c, 0x11, 0xff},
	{0x303b, 0x8, 0xff},
	{0x303a, 0x0, 0xff},
	{0x3821, 0x10, 0xff},
	{0x3660, 0x90, 0xff},
	{0x3622, 0x0, 0xff},
	{0x3820, 0x0, 0xff},
	{0x3600, 0x98, 0xff},
	{0x3601, 0x4c, 0xff},
	{0x3602, 0xe, 0xff},
	{0x3603, 0x24, 0xff},
	{0x3620, 0x53, 0xff},
	{0x3621, 0x10, 0xff},
	{0x3604, 0xc1, 0xff},
	{0x3631, 0x2a, 0xff},
	{0x3633, 0x65, 0xff},
	{0x3634, 0x1, 0xff},
	{0x3701, 0x33, 0xff},
	{0x3704, 0x28, 0xff},
	{0x370c, 0x30, 0xff},
	{0x370d, 0x3, 0xff},
	{0x3712, 0x5, 0xff},
	{0x3613, 0x0, 0xff},
	{0x3707, 0x67, 0xff},
	{0x3823, 0x0, 0xff},
	{0x4500, 0x32, 0xff},
	{0x5000, 0xcf, 0xff},
	{0x5484, 0x63, 0xff},
	{0x5485, 0x6e, 0xff},
	{0x5486, 0x78, 0xff},
	{0x5487, 0x80, 0xff},
	{0x5488, 0x88, 0xff},
	{0x5489, 0x90, 0xff},
	{0x5800, 0x0, 0xff},
	{0x5801, 0x0, 0xff},
	{0x5802, 0x1, 0xff},
	{0x5803, 0x40, 0xff},
	{0x5804, 0x0, 0xff},
	{0x5805, 0xf0, 0xff},
	{0x5806, 0x28, 0xff},
	{0x5807, 0x2a, 0xff},
	{0x5808, 0x3a, 0xff},
	{0x5308, 0x30, 0xff},
	{0x5306, 0x0, 0xff},
	{0x5180, 0x50, 0xff},
	{0x5184, 0x73, 0xff},
	{0x5185, 0x50, 0xff},
	{0x5186, 0x49, 0xff},
	{0x5187, 0xab, 0xff},
	{0x518b, 0x3f, 0xff},
	{0x518c, 0x26, 0xff},
	{0x3035, 0x21, 0xff},
	{0x3036, 0x44, 0xff},
	{0x3037, 0x12, 0xff},
	{0x3020, 0x9, 0xff},
	{0x3016, 0x18, 0xff},
	{0x3827, 0xec, 0xff},
	{0x4601, 0x16, 0xff},
	{0x4837, 0x28, 0xff},
	{0x3018, 0x4, 0xff},
	{0x5a24, 0x0, 0xff},
	{0x3a18, 0x0, 0xff},
	{0x3a19, 0xfe, 0xff},
	{0x3a1a, 0x6, 0xff},
	{0x3623, 0x3, 0xff},
	{0x3006, 0x12, 0xff},
	{0x3c00, 0x80, 0xff},
	{0x3c01, 0x36, 0xff},
	{0x3c04, 0x18, 0xff},
	{0x3c05, 0x70, 0xff},
	{0x3c06, 0x0, 0xff},
	{0x3c07, 0x20, 0xff},
	{0x3c08, 0x0, 0xff},
	{0x3c09, 0xc2, 0xff},
	{0x3c0a, 0x9c, 0xff},
	{0x3c0b, 0x40, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0x14, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x5091, 0x0, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x4, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x4, 0xff},
	{0x3808, 0x2, 0xff},
	{0x3809, 0x80, 0xff},
	{0x380a, 0x1, 0xff},
	{0x380b, 0xe0, 0xff},
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x8b, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xe7, 0xff},
	{0x4300, 0x3f, 0xff},
	{0x501f, 0x1, 0xff},
	{0x4301, 0x80, 0xff},
	{0x4001, 0x2, 0xff},
	{0x4004, 0x4, 0xff},
	{0x4051, 0x0, 0xff},
	{0x4002, 0x45, 0xff},
	{0x3632, 0xc9, 0xff},
	{0x5000, 0xcf, 0xff},
	{0x5001, 0xa7, 0xff},
	{0x5780, 0x5f, 0xff},
	{0x3c00, 0x4, 0xff},
	{0x4009, 0xc, 0xff},
	{0x3a18, 0x1, 0xff},
	{0x3a19, 0x0, 0xff},
	{0x3a0c, 0xa4, 0xff},
	{0x3a0f, 0x56, 0xff},
	{0x3a10, 0x4e, 0xff},
	{0x3a1b, 0x57, 0xff},
	{0x3a1e, 0x4d, 0xff},
};

static const CameraSensorRegister cameraSensorPowerOnRegisters[2] = {
	{0x3018, 0x04, 0xff},
	{0x0100, 0x01, 0xff},
};

static const CameraSensorRegister cameraExposureCeiling60Registers[2] = {
	{0x3a0d, 0x2, 0xff},
	{0x3a0e, 0x1, 0xff},
};

static const CameraSensorRegister cameraBt601RangeRegisters[20] = {
	{0x5381, 0x1d, 0xff},
	{0x5382, 0x32, 0xff},
	{0x5383, 0x1f, 0xff},
	{0x5384, 0x5, 0xff},
	{0x5385, 0x6b, 0xff},
	{0x5386, 0x70, 0xff},
	{0x5387, 0x5f, 0xff},
	{0x5388, 0x4e, 0xff},
	{0x5389, 0x11, 0xff},
	{0x538b, 0x98, 0xff},
	{0x538a, 0x1, 0xff},
	{0x4303, 0xac, 0xff},
	{0x4305, 0x40, 0xff},
	{0x4307, 0xc0, 0xff},
	{0x4309, 0x40, 0xff},
	{0x430b, 0xc0, 0xff},
	{0x430d, 0x40, 0xff},
	{0x5580, 0x6, 0xff},
	{0x5587, 0x10, 0xff},
	{0x100, 0x1, 0xff},
};

static const CameraSensorRegister cameraIso100Registers[6] = {
	{0x3503, 0x7, 0xff},
	{0x350a, 0x0, 0x3},
	{0x350b, 0x10, 0xff},
	{0x3a13, 0x90, 0xff},
	{0x3a18, 0x0, 0xff},
	{0x3a19, 0x14, 0xff},
};

static const CameraSensorRegister cameraNight1Registers[7] = {
	{0x3a00, 0x4, 0x4},
	{0x3a05, 0x0, 0x40},
	{0x3a17, 0x0, 0x3},
	{0x3a14, 0x7, 0xff},
	{0x3a15, 0x20, 0xff},
	{0x3a02, 0x7, 0xff},
	{0x3a03, 0xf0, 0xff},
};

static const CameraSensorRegister cameraEv15Registers[4] = {
	{0x3a0f, 0x90, 0xff},
	{0x3a10, 0x87, 0xff},
	{0x3a1b, 0x91, 0xff},
	{0x3a1e, 0x86, 0xff},
};

static const CameraSensorRegister cameraResolution2Registers[30] = {
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x8b, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xe7, 0xff},
	{0x3808, 0x1, 0xff},
	{0x3809, 0x40, 0xff},
	{0x380a, 0x0, 0xff},
	{0x380b, 0xf0, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x4, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x4, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0x14, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3814, 0x11, 0xff},
	{0x3815, 0x11, 0xff},
	{0x3820, 0x0, 0xff},
	{0x3821, 0x10, 0xff},
	{0x3622, 0x0, 0xff},
	{0x3660, 0x90, 0xff},
	{0x4001, 0x2, 0xff},
	{0x4004, 0x4, 0xff},
	{0x4608, 0x1, 0xff},
	{0x4609, 0xc0, 0xff},
};

static const CameraSensorRegister cameraSharpness61440Registers[2] = {
	{0x5302, 0x0, 0xff},
	{0x5303, 0x0, 0xff},
};

static const CameraSensorRegister cameraAntiflicker60ExtendedRegisters[2] = {
	{0x3c01, 0x0, 0x80},
	{0x3c00, 0x0, 0x4},
};

static const CameraSensorTiming cameraFramerate25Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0xb2, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3035, 0x41, 0xf0},
	{0x3a08, 0x0, 0x3},
	{0x3a09, 0x7e, 0xff},
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0x69, 0xff},
	{0x3a0d, 0x4, 0x3f},
	{0x3a0e, 0x4, 0x3f},
	{0x4837, 0x52, 0xff},
	{0x3632, 0x29, 0xff},
	{0x100, 0x0, 0xff},
}};


static const CameraSensorRegister cameraAntiflickerAutoRegisters[2] = {
	{0x3c01, 0x0, 0x80},
	{0x3c00, 0x4, 0x4},
};

static const CameraSensorRegister cameraEv5Registers[4] = {
	{0x3a0f, 0x68, 0xff},
	{0x3a10, 0x60, 0xff},
	{0x3a1b, 0x69, 0xff},
	{0x3a1e, 0x5f, 0xff},
};

static const CameraSensorRegister cameraResolution3HighFramerateRegisters[30] = {
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x93, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xef, 0xff},
	{0x3808, 0x0, 0xff},
	{0x3809, 0xa0, 0xff},
	{0x380a, 0x0, 0xff},
	{0x380b, 0x78, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x4, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x4, 0xff},
	{0x380c, 0x2, 0xff},
	{0x380d, 0xea, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xc, 0xff},
	{0x3814, 0x31, 0xff},
	{0x3815, 0x31, 0xff},
	{0x3820, 0x1, 0xff},
	{0x3821, 0x11, 0xff},
	{0x3622, 0x40, 0xff},
	{0x3660, 0x80, 0xff},
	{0x4001, 0x0, 0xff},
	{0x4004, 0x8, 0xff},
	{0x4608, 0x2, 0xff},
	{0x4609, 0x60, 0xff},
};

static const CameraSensorRegister cameraEv3Registers[4] = {
	{0x3a0f, 0x5f, 0xff},
	{0x3a10, 0x57, 0xff},
	{0x3a1b, 0x60, 0xff},
	{0x3a1e, 0x56, 0xff},
};

static const CameraSensorRegister cameraImageQuality4Registers[40] = {
	{0x5481, 0xa, 0xff},
	{0x5482, 0x12, 0xff},
	{0x5483, 0x21, 0xff},
	{0x5484, 0x3d, 0xff},
	{0x5485, 0x4b, 0xff},
	{0x5486, 0x58, 0xff},
	{0x5302, 0x38, 0xff},
	{0x5303, 0x18, 0xff},
	{0x5305, 0x20, 0xff},
	{0x5306, 0xa, 0xff},
	{0x5307, 0x20, 0xff},
	{0x5584, 0x20, 0xff},
	{0x5583, 0x3f, 0xff},
	{0x5589, 0x18, 0xff},
	{0x558b, 0x80, 0xff},
	{0x518a, 0xe, 0xff},
	{0x3a18, 0x1, 0xff},
	{0x3a19, 0x0, 0xff},
	{0x3208, 0x0, 0xff},
	{0x5381, 0x1d, 0xff},
	{0x5382, 0x32, 0xff},
	{0x5383, 0x1f, 0xff},
	{0x5384, 0x5, 0xff},
	{0x5385, 0x6b, 0xff},
	{0x5386, 0x70, 0xff},
	{0x5387, 0x5f, 0xff},
	{0x5388, 0x4e, 0xff},
	{0x5389, 0x11, 0xff},
	{0x538b, 0x98, 0xff},
	{0x538a, 0x1, 0xff},
	{0x4303, 0xac, 0xff},
	{0x4305, 0x40, 0xff},
	{0x4307, 0xc0, 0xff},
	{0x4309, 0x40, 0xff},
	{0x430b, 0xc0, 0xff},
	{0x430d, 0x40, 0xff},
	{0x5580, 0x6, 0xff},
	{0x5587, 0x10, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static const CameraSensorRegister cameraIso200Registers[6] = {
	{0x3503, 0x7, 0xff},
	{0x350a, 0x0, 0x3},
	{0x350b, 0x1a, 0xff},
	{0x3a13, 0x9a, 0xff},
	{0x3a18, 0x0, 0xff},
	{0x3a19, 0x27, 0xff},
};

static const CameraSensorRegister cameraResolution6Registers[30] = {
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x8b, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xe7, 0xff},
	{0x3808, 0x1, 0xff},
	{0x3809, 0xe0, 0xff},
	{0x380a, 0x1, 0xff},
	{0x380b, 0x10, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x4, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x3f, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0x14, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3814, 0x11, 0xff},
	{0x3815, 0x11, 0xff},
	{0x3820, 0x0, 0xff},
	{0x3821, 0x10, 0xff},
	{0x3622, 0x0, 0xff},
	{0x3660, 0x90, 0xff},
	{0x4001, 0x2, 0xff},
	{0x4004, 0x4, 0xff},
	{0x4608, 0x1, 0xff},
	{0x4609, 0x20, 0xff},
};

static const CameraSensorRegister cameraAutoControlHoldRegisters[4] = {
	{0x3503, 0x1, 0x1},
	{0x3503, 0x2, 0x2},
	{0x5001, 0x0, 0x1},
	{0x5180, 0x2, 0x2},
};

static const CameraSensorRegister cameraExposureCeiling50Registers[2] = {
	{0x3a0d, 0x1, 0xff},
	{0x3a0e, 0x1, 0xff},
};

static const CameraSensorRegister cameraResolution5HighFramerateRegisters[30] = {
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x93, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xef, 0xff},
	{0x3808, 0x0, 0xff},
	{0x3809, 0xb0, 0xff},
	{0x380a, 0x0, 0xff},
	{0x380b, 0x90, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x11, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x4, 0xff},
	{0x380c, 0x2, 0xff},
	{0x380d, 0xea, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xc, 0xff},
	{0x3814, 0x31, 0xff},
	{0x3815, 0x31, 0xff},
	{0x3820, 0x1, 0xff},
	{0x3821, 0x11, 0xff},
	{0x3622, 0x40, 0xff},
	{0x3660, 0x80, 0xff},
	{0x4001, 0x0, 0xff},
	{0x4004, 0x8, 0xff},
	{0x4608, 0x2, 0xff},
	{0x4609, 0x50, 0xff},
};

static const CameraSensorRegister cameraFullRangeRegisters[20] = {
	{0x5381, 0x22, 0xff},
	{0x5382, 0x3a, 0xff},
	{0x5383, 0x24, 0xff},
	{0x5384, 0x6, 0xff},
	{0x5385, 0x7a, 0xff},
	{0x5386, 0x80, 0xff},
	{0x5387, 0x6c, 0xff},
	{0x5388, 0x59, 0xff},
	{0x5389, 0x13, 0xff},
	{0x538b, 0x98, 0xff},
	{0x538a, 0x1, 0xff},
	{0x4303, 0xff, 0xff},
	{0x4305, 0x0, 0xff},
	{0x4307, 0xff, 0xff},
	{0x4309, 0x0, 0xff},
	{0x430b, 0xff, 0xff},
	{0x430d, 0x0, 0xff},
	{0x5580, 0x2, 0xff},
	{0x5587, 0x0, 0xff},
	{0x100, 0x1, 0xff},
};

static const CameraSensorRegister cameraEv17Registers[4] = {
	{0x3a0f, 0x9a, 0xff},
	{0x3a10, 0x91, 0xff},
	{0x3a1b, 0x9b, 0xff},
	{0x3a1e, 0x90, 0xff},
};

static const CameraSensorRegister cameraSensorProcessedFormatRegisters[9] = {
	{0x3663, 0x1, 0xff},
	{0x3037, 0x12, 0xff},
	{0x3020, 0x9, 0xff},
	{0x3034, 0x18, 0xff},
	{0x3036, 0x44, 0xff},
	{0x4300, 0x3f, 0xff},
	{0x501f, 0x1, 0xff},
	{0x4301, 0x80, 0xff},
	{0x5000, 0xcf, 0xff},
};

static const CameraSensorRegister cameraEv13Registers[4] = {
	{0x3a0f, 0x86, 0xff},
	{0x3a10, 0x7d, 0xff},
	{0x3a1b, 0x87, 0xff},
	{0x3a1e, 0x7c, 0xff},
};

static const CameraSensorRegister cameraImageQuality0Registers[38] = {
	{0x5481, 0xa, 0xff},
	{0x5482, 0x12, 0xff},
	{0x5483, 0x21, 0xff},
	{0x5484, 0x3d, 0xff},
	{0x5485, 0x4b, 0xff},
	{0x5486, 0x58, 0xff},
	{0x5302, 0x38, 0xff},
	{0x5303, 0x18, 0xff},
	{0x5305, 0x20, 0xff},
	{0x5306, 0xa, 0xff},
	{0x5307, 0x20, 0xff},
	{0x5584, 0x20, 0xff},
	{0x5583, 0x3f, 0xff},
	{0x5589, 0x18, 0xff},
	{0x558b, 0x80, 0xff},
	{0x518a, 0xe, 0xff},
	{0x3208, 0x0, 0xff},
	{0x5381, 0x22, 0xff},
	{0x5382, 0x3a, 0xff},
	{0x5383, 0x24, 0xff},
	{0x5384, 0x6, 0xff},
	{0x5385, 0x7a, 0xff},
	{0x5386, 0x80, 0xff},
	{0x5387, 0x6c, 0xff},
	{0x5388, 0x59, 0xff},
	{0x5389, 0x13, 0xff},
	{0x538b, 0x98, 0xff},
	{0x538a, 0x1, 0xff},
	{0x4303, 0xff, 0xff},
	{0x4305, 0x0, 0xff},
	{0x4307, 0xff, 0xff},
	{0x4309, 0x0, 0xff},
	{0x430b, 0xff, 0xff},
	{0x430d, 0x0, 0xff},
	{0x5580, 0x2, 0xff},
	{0x5587, 0x0, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static const CameraSensorRegister cameraIsoAutoRegisters[4] = {
	{0x3503, 0x0, 0xff},
	{0x3a13, 0x90, 0xff},
	{0x3a18, 0x1, 0xff},
	{0x3a19, 0x0, 0xff},
};

static const CameraSensorRegister cameraResolution4Registers[30] = {
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x8b, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xe7, 0xff},
	{0x3808, 0x1, 0xff},
	{0x3809, 0x60, 0xff},
	{0x380a, 0x1, 0xff},
	{0x380b, 0x20, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x1f, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x4, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0x14, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3814, 0x11, 0xff},
	{0x3815, 0x11, 0xff},
	{0x3820, 0x0, 0xff},
	{0x3821, 0x10, 0xff},
	{0x3622, 0x0, 0xff},
	{0x3660, 0x90, 0xff},
	{0x4001, 0x2, 0xff},
	{0x4004, 0x4, 0xff},
	{0x4608, 0x1, 0xff},
	{0x4609, 0xa0, 0xff},
};

static const CameraSensorTiming cameraFramerate30Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0x14, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3035, 0x41, 0xf0},
	{0x3a08, 0x0, 0x3},
	{0x3a09, 0x98, 0xff},
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0x7e, 0xff},
	{0x3a0d, 0x4, 0x3f},
	{0x3a0e, 0x3, 0x3f},
	{0x4837, 0x52, 0xff},
	{0x3632, 0x29, 0xff},
	{0x100, 0x0, 0xff},
}};

static const CameraSensorRegister cameraSensorPowerOffRegisters[2] = {
	{0x0100, 0x00, 0xff},
	{0x3018, 0x1c, 0xff},
};

static const CameraSensorRegister cameraResolution2HighFramerateRegisters[30] = {
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x93, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xef, 0xff},
	{0x3808, 0x1, 0xff},
	{0x3809, 0x40, 0xff},
	{0x380a, 0x0, 0xff},
	{0x380b, 0xf0, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x4, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x4, 0xff},
	{0x380c, 0x2, 0xff},
	{0x380d, 0xea, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xc, 0xff},
	{0x3814, 0x31, 0xff},
	{0x3815, 0x31, 0xff},
	{0x3820, 0x1, 0xff},
	{0x3821, 0x11, 0xff},
	{0x3622, 0x40, 0xff},
	{0x3660, 0x80, 0xff},
	{0x4001, 0x0, 0xff},
	{0x4004, 0x8, 0xff},
	{0x4608, 0x1, 0xff},
	{0x4609, 0xc0, 0xff},
};

static const CameraSensorRegister cameraSensorResetRegisters[2] = {
	{0x100, 0x0, 0xff},
	{0x103, 0x1, 0xff},
};

static const CameraSensorRegister cameraIso400Registers[6] = {
	{0x3503, 0x7, 0xff},
	{0x350a, 0x0, 0x3},
	{0x350b, 0x34, 0xff},
	{0x3a13, 0xb4, 0xff},
	{0x3a18, 0x0, 0xff},
	{0x3a19, 0x4f, 0xff},
};

static const CameraSensorRegister cameraSensorExtendedTimingRegisters[10] = {
	{0x3a0a, 0x1, 0x3},
	{0x3a0b, 0x30, 0xff},
	{0x3a0d, 0x1, 0x3f},
	{0x3a05, 0x24, 0xff},
	{0x4800, 0x24, 0xff},
	{0x100, 0x0, 0xff},
	{0x3503, 0x7, 0xff},
	{0x3501, 0x1, 0xff},
	{0x3502, 0x30, 0xff},
	{0x3503, 0x0, 0xff},
};

static const CameraSensorRegister cameraNight0Registers[1] = {
	{0x3a00, 0x0, 0x4},
};

static const CameraSensorRegister cameraEvMinus20Registers[4] = {
	{0x3a0f, 0x23, 0xff},
	{0x3a10, 0x1e, 0xff},
	{0x3a1b, 0x24, 0xff},
	{0x3a1e, 0x1d, 0xff},
};

static const CameraSensorRegister cameraEvMinus10Registers[4] = {
	{0x3a0f, 0x3b, 0xff},
	{0x3a10, 0x36, 0xff},
	{0x3a1b, 0x3c, 0xff},
	{0x3a1e, 0x35, 0xff},
};

static const CameraSensorRegister cameraEv0Registers[4] = {
	{0x3a0f, 0x56, 0xff},
	{0x3a10, 0x4e, 0xff},
	{0x3a1b, 0x57, 0xff},
	{0x3a1e, 0x4d, 0xff},
};

static const CameraSensorRegister cameraNoise2Registers[2] = {
	{0x5306, 0x8, 0xff},
	{0x5307, 0x20, 0xff},
};

static const CameraSensorRegister cameraEv20Registers[4] = {
	{0x3a0f, 0xa4, 0xff},
	{0x3a10, 0x9b, 0xff},
	{0x3a1b, 0xa5, 0xff},
	{0x3a1e, 0x9a, 0xff},
};

static const CameraSensorRegister cameraEv10Registers[4] = {
	{0x3a0f, 0x7c, 0xff},
	{0x3a10, 0x73, 0xff},
	{0x3a1b, 0x7d, 0xff},
	{0x3a1e, 0x72, 0xff},
};

static const CameraSensorRegister cameraNight2Registers[7] = {
	{0x3a00, 0x4, 0x4},
	{0x3a05, 0x0, 0x40},
	{0x3a17, 0x0, 0x3},
	{0x3a14, 0x5, 0xff},
	{0x3a15, 0xf0, 0xff},
	{0x3a02, 0x5, 0xff},
	{0x3a03, 0xf4, 0xff},
};

static const CameraSensorRegister cameraSensorBasicTimingRegisters[6] = {
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0xfe, 0xff},
	{0x3a0d, 0x2, 0x3f},
	{0x3a05, 0x24, 0xff},
	{0x4800, 0x24, 0xff},
	{0x100, 0x0, 0xff},
};

static const CameraSensorRegister cameraImageQuality2Registers[38] = {
	{0x5481, 0xd, 0xff},
	{0x5482, 0x19, 0xff},
	{0x5483, 0x2a, 0xff},
	{0x5484, 0x45, 0xff},
	{0x5485, 0x50, 0xff},
	{0x5486, 0x5a, 0xff},
	{0x5302, 0x4e, 0xff},
	{0x5303, 0x30, 0xff},
	{0x5305, 0x1a, 0xff},
	{0x5306, 0xe, 0xff},
	{0x5307, 0x1c, 0xff},
	{0x5584, 0x44, 0xff},
	{0x5583, 0x44, 0xff},
	{0x5589, 0x10, 0xff},
	{0x558b, 0x20, 0xff},
	{0x518a, 0x23, 0xff},
	{0x3208, 0x0, 0xff},
	{0x5381, 0x1b, 0xff},
	{0x5382, 0x2b, 0xff},
	{0x5383, 0x28, 0xff},
	{0x5384, 0xc, 0xff},
	{0x5385, 0x77, 0xff},
	{0x5386, 0x7f, 0xff},
	{0x5387, 0x5a, 0xff},
	{0x5388, 0x47, 0xff},
	{0x5389, 0x12, 0xff},
	{0x538b, 0x98, 0xff},
	{0x538a, 0x1, 0xff},
	{0x4303, 0xac, 0xff},
	{0x4305, 0x40, 0xff},
	{0x4307, 0xc0, 0xff},
	{0x4309, 0x40, 0xff},
	{0x430b, 0xc0, 0xff},
	{0x430d, 0x40, 0xff},
	{0x5580, 0x6, 0xff},
	{0x5587, 0x10, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static const CameraSensorRegister cameraResolution1Registers[30] = {
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x8b, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xe7, 0xff},
	{0x3808, 0x2, 0xff},
	{0x3809, 0x80, 0xff},
	{0x380a, 0x1, 0xff},
	{0x380b, 0xe0, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x4, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x4, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0x14, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3814, 0x11, 0xff},
	{0x3815, 0x11, 0xff},
	{0x3820, 0x0, 0xff},
	{0x3821, 0x10, 0xff},
	{0x3622, 0x0, 0xff},
	{0x3660, 0x90, 0xff},
	{0x4001, 0x2, 0xff},
	{0x4004, 0x4, 0xff},
	{0x4608, 0x0, 0xff},
	{0x4609, 0x80, 0xff},
};

static const CameraSensorRegister cameraNoise0Registers[2] = {
	{0x5306, 0x0, 0xff},
	{0x5307, 0x0, 0xff},
};

static const CameraSensorTiming cameraFramerate60Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0x14, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3035, 0x21, 0xf0},
	{0x3a08, 0x1, 0x3},
	{0x3a09, 0x30, 0xff},
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0xfe, 0xff},
	{0x3a0d, 0x2, 0x3f},
	{0x3a0e, 0x1, 0x3f},
	{0x4837, 0x28, 0xff},
	{0x3632, 0xc9, 0xff},
	{0x100, 0x0, 0xff},
}};

static const CameraExposureGap cameraExposureGaps[11] = {
	{3, 526},
	{5, 394},
	{7, 263},
	{10, 197},
	{15, 131},
	{20, 99},
	{24, 82},
	{25, 79},
	{30, 66},
	{60, 33},
	{120, 31},
};

static const CameraSensorTiming cameraFramerate24Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0xd8, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3035, 0x41, 0xf0},
	{0x3a08, 0x0, 0x3},
	{0x3a09, 0x79, 0xff},
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0x65, 0xff},
	{0x3a0d, 0x5, 0x3f},
	{0x3a0e, 0x4, 0x3f},
	{0x4837, 0x52, 0xff},
	{0x3632, 0x29, 0xff},
	{0x100, 0x0, 0xff},
}};

static const CameraSavedSensorState cameraDefaultSensorState = {
	.registers = {
		{0x5580, 0x2, 0xff},
		{0x5583, 0x3f, 0xff},
		{0x5584, 0x20, 0xff},
		{0x3503, 0x0, 0x7},
		{0x3500, 0x0, 0xff},
		{0x3501, 0x1, 0xff},
		{0x3502, 0x30, 0xff},
		{0x350a, 0x0, 0xff},
		{0x350b, 0x10, 0xff},
		{0x5001, 0x1, 0x1},
		{0x5180, 0x54, 0x2},
		{0x5195, 0x4, 0xff},
		{0x5196, 0x0, 0xff},
		{0x5197, 0x4, 0xff},
		{0x5198, 0x0, 0xff},
		{0x5199, 0x4, 0xff},
		{0x519a, 0x0, 0xff},
		{0x5806, 0x1c, 0xff},
		{0x5807, 0x21, 0xff},
		{0x5808, 0x2e, 0xff},
	},
};

static const CameraSensorTiming cameraFramerate20Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0x4, 0xff},
	{0x380d, 0x9e, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3035, 0x41, 0xf0},
	{0x3a08, 0x0, 0x3},
	{0x3a09, 0x65, 0xff},
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0x54, 0xff},
	{0x3a0d, 0x6, 0x3f},
	{0x3a0e, 0x5, 0x3f},
	{0x4837, 0x52, 0xff},
	{0x3632, 0x29, 0xff},
	{0x100, 0x0, 0xff},
}};

static const CameraSensorRegister cameraSensorRaw8Registers[9] = {
	{0x3663, 0x2, 0xff},
	{0x3037, 0x2, 0xff},
	{0x3020, 0xb, 0xff},
	{0x3034, 0x18, 0xff},
	{0x3036, 0x22, 0xff},
	{0x4300, 0xf8, 0xff},
	{0x501f, 0x0, 0xff},
	{0x4301, 0x40, 0xff},
	{0x5000, 0x86, 0xff},
};

static const CameraSensorRegister cameraBacklightOffRegisters[8] = {
	{0x5688, 0x11, 0xff},
	{0x5689, 0x11, 0xff},
	{0x568a, 0x11, 0xff},
	{0x568b, 0x11, 0xff},
	{0x568c, 0x11, 0xff},
	{0x568d, 0x11, 0xff},
	{0x568e, 0x11, 0xff},
	{0x568f, 0x11, 0xff},
};

static const CameraSensorRegister cameraNoise4Registers[2] = {
	{0x5306, 0x4, 0xff},
	{0x5307, 0x20, 0xff},
};

static const CameraSensorRegister cameraNight3Registers[7] = {
	{0x3a00, 0x4, 0x4},
	{0x3a05, 0x0, 0x40},
	{0x3a17, 0x0, 0x3},
	{0x3a14, 0x3, 0xff},
	{0x3a15, 0x90, 0xff},
	{0x3a02, 0x3, 0xff},
	{0x3a03, 0xf8, 0xff},
};

static const CameraSensorTiming cameraFramerate7Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0xc, 0xff},
	{0x380d, 0x50, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3035, 0x41, 0xf0},
	{0x3a08, 0x0, 0x3},
	{0x3a09, 0x26, 0xff},
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0x1f, 0xff},
	{0x3a0d, 0x10, 0x3f},
	{0x3a0e, 0xd, 0x3f},
	{0x4837, 0x52, 0xff},
	{0x3632, 0x29, 0xff},
	{0x100, 0x0, 0xff},
}};

static const CameraSensorRegister cameraImageQuality1Registers[38] = {
	{0x5481, 0xd, 0xff},
	{0x5482, 0x19, 0xff},
	{0x5483, 0x2a, 0xff},
	{0x5484, 0x45, 0xff},
	{0x5485, 0x50, 0xff},
	{0x5486, 0x5a, 0xff},
	{0x5302, 0x4e, 0xff},
	{0x5303, 0x30, 0xff},
	{0x5305, 0x1a, 0xff},
	{0x5306, 0xe, 0xff},
	{0x5307, 0x1c, 0xff},
	{0x5584, 0x44, 0xff},
	{0x5583, 0x44, 0xff},
	{0x5589, 0x10, 0xff},
	{0x558b, 0x20, 0xff},
	{0x518a, 0x23, 0xff},
	{0x3208, 0x0, 0xff},
	{0x5381, 0x1b, 0xff},
	{0x5382, 0x2b, 0xff},
	{0x5383, 0x28, 0xff},
	{0x5384, 0xc, 0xff},
	{0x5385, 0x77, 0xff},
	{0x5386, 0x7f, 0xff},
	{0x5387, 0x5a, 0xff},
	{0x5388, 0x47, 0xff},
	{0x5389, 0x12, 0xff},
	{0x538b, 0x98, 0xff},
	{0x538a, 0x1, 0xff},
	{0x4303, 0xac, 0xff},
	{0x4305, 0x40, 0xff},
	{0x4307, 0xc0, 0xff},
	{0x4309, 0x40, 0xff},
	{0x430b, 0xc0, 0xff},
	{0x430d, 0x40, 0xff},
	{0x5580, 0x6, 0xff},
	{0x5587, 0x10, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static const CameraSensorRegister cameraSharpness1Registers[5] = {
	{0x5300, 0x18, 0xff},
	{0x5301, 0x20, 0xff},
	{0x5302, 0x38, 0xff},
	{0x5303, 0x18, 0xff},
	{0x5308, 0x0, 0x40},
};

static const CameraSensorRegister cameraBacklightOnRegisters[8] = {
	{0x5688, 0x11, 0xff},
	{0x5689, 0x11, 0xff},
	{0x568a, 0xf4, 0xff},
	{0x568b, 0x4f, 0xff},
	{0x568c, 0xf4, 0xff},
	{0x568d, 0x4f, 0xff},
	{0x568e, 0x44, 0xff},
	{0x568f, 0x44, 0xff},
};

static const CameraSensorTiming cameraFramerate5Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0x12, 0xff},
	{0x380d, 0x78, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3035, 0x41, 0xf0},
	{0x3a08, 0x0, 0x3},
	{0x3a09, 0x19, 0xff},
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0x15, 0xff},
	{0x3a0d, 0x18, 0x3f},
	{0x3a0e, 0x14, 0x3f},
	{0x4837, 0x52, 0xff},
	{0x3632, 0x29, 0xff},
	{0x100, 0x0, 0xff},
}};

static const CameraSensorRegister cameraAutoControlOffRegisters[4] = {
	{0x3503, 0x0, 0x1},
	{0x3503, 0x0, 0x2},
	{0x5001, 0x1, 0x1},
	{0x5180, 0x0, 0x2},
};

static const CameraSensorRegister cameraSharpness4Registers[5] = {
	{0x5300, 0x18, 0xff},
	{0x5301, 0x20, 0xff},
	{0x5302, 0x7f, 0xff},
	{0x5303, 0x18, 0xff},
	{0x5308, 0x0, 0x40},
};

static const CameraSensorRegister cameraSharpness3Registers[5] = {
	{0x5300, 0x18, 0xff},
	{0x5301, 0x20, 0xff},
	{0x5302, 0x60, 0xff},
	{0x5303, 0x18, 0xff},
	{0x5308, 0x0, 0x40},
};

static const CameraSensorRegister cameraSharpness2Registers[5] = {
	{0x5300, 0x18, 0xff},
	{0x5301, 0x20, 0xff},
	{0x5302, 0x48, 0xff},
	{0x5303, 0x18, 0xff},
	{0x5308, 0x0, 0x40},
};

static const CameraSensorTiming cameraFramerate120Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0x2, 0xff},
	{0x380d, 0xea, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xc, 0xff},
	{0x3035, 0x21, 0xf0},
	{0x3a08, 0x1, 0x3},
	{0x3a09, 0x41, 0xff},
	{0x3a0a, 0x1, 0x3},
	{0x3a0b, 0xc, 0xff},
	{0x3a0d, 0x1, 0x3f},
	{0x3a0e, 0x1, 0x3f},
	{0x4837, 0x28, 0xff},
	{0x3632, 0xc9, 0xff},
	{0x100, 0x0, 0xff},
}};

static const CameraSensorRegister cameraEvMinus7Registers[4] = {
	{0x3a0f, 0x41, 0xff},
	{0x3a10, 0x3c, 0xff},
	{0x3a1b, 0x42, 0xff},
	{0x3a1e, 0x3b, 0xff},
};

static const CameraSensorRegister cameraEvMinus5Registers[4] = {
	{0x3a0f, 0x47, 0xff},
	{0x3a10, 0x42, 0xff},
	{0x3a1b, 0x48, 0xff},
	{0x3a1e, 0x41, 0xff},
};

static const CameraSensorRegister cameraEvMinus3Registers[4] = {
	{0x3a0f, 0x4d, 0xff},
	{0x3a10, 0x48, 0xff},
	{0x3a1b, 0x4e, 0xff},
	{0x3a1e, 0x47, 0xff},
};

static const CameraSensorRegister cameraExposureRecoveryRegisters[4] = {
	{0x3503, 7, 0xff},
	{0x3501, 1, 0xff},
	{0x3502, 0x30, 0xff},
	{0x3503, 0, 0xff},
};

static const CameraSensorTiming cameraFramerate3Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0x18, 0xff},
	{0x380d, 0xa0, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3035, 0x41, 0xf0},
	{0x3a08, 0x0, 0x3},
	{0x3a09, 0x13, 0xff},
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0xf, 0xff},
	{0x3a0d, 0x20, 0x3f},
	{0x3a0e, 0x1a, 0x3f},
	{0x4837, 0x52, 0xff},
	{0x3632, 0x29, 0xff},
	{0x100, 0x0, 0xff},
}};

static const CameraSensorRegister cameraImageQuality3Registers[38] = {
	{0x5481, 0xa, 0xff},
	{0x5482, 0x12, 0xff},
	{0x5483, 0x21, 0xff},
	{0x5484, 0x3d, 0xff},
	{0x5485, 0x4b, 0xff},
	{0x5486, 0x58, 0xff},
	{0x5302, 0x38, 0xff},
	{0x5303, 0x18, 0xff},
	{0x5305, 0x20, 0xff},
	{0x5306, 0xa, 0xff},
	{0x5307, 0x20, 0xff},
	{0x5584, 0x20, 0xff},
	{0x5583, 0x3f, 0xff},
	{0x5589, 0x18, 0xff},
	{0x558b, 0x80, 0xff},
	{0x518a, 0xe, 0xff},
	{0x3208, 0x0, 0xff},
	{0x5381, 0x22, 0xff},
	{0x5382, 0x3a, 0xff},
	{0x5383, 0x24, 0xff},
	{0x5384, 0x6, 0xff},
	{0x5385, 0x7a, 0xff},
	{0x5386, 0x80, 0xff},
	{0x5387, 0x6c, 0xff},
	{0x5388, 0x59, 0xff},
	{0x5389, 0x13, 0xff},
	{0x538b, 0x98, 0xff},
	{0x538a, 0x1, 0xff},
	{0x4303, 0xff, 0xff},
	{0x4305, 0x0, 0xff},
	{0x4307, 0xff, 0xff},
	{0x4309, 0x0, 0xff},
	{0x430b, 0xff, 0xff},
	{0x430d, 0x0, 0xff},
	{0x5580, 0x2, 0xff},
	{0x5587, 0x0, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static const CameraSensorRegister cameraNoise1Registers[2] = {
	{0x5306, 0xa, 0xff},
	{0x5307, 0x20, 0xff},
};

static const CameraSensorRegister cameraResolution5Registers[30] = {
	{0x3800, 0x0, 0xff},
	{0x3801, 0x4, 0xff},
	{0x3802, 0x0, 0xff},
	{0x3803, 0x0, 0xff},
	{0x3804, 0x2, 0xff},
	{0x3805, 0x8b, 0xff},
	{0x3806, 0x1, 0xff},
	{0x3807, 0xe7, 0xff},
	{0x3808, 0x0, 0xff},
	{0x3809, 0xb0, 0xff},
	{0x380a, 0x0, 0xff},
	{0x380b, 0x90, 0xff},
	{0x3810, 0x0, 0xff},
	{0x3811, 0x1f, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x4, 0xff},
	{0x380c, 0x3, 0xff},
	{0x380d, 0x14, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3814, 0x11, 0xff},
	{0x3815, 0x11, 0xff},
	{0x3820, 0x0, 0xff},
	{0x3821, 0x10, 0xff},
	{0x3622, 0x0, 0xff},
	{0x3660, 0x90, 0xff},
	{0x4001, 0x2, 0xff},
	{0x4004, 0x4, 0xff},
	{0x4608, 0x2, 0xff},
	{0x4609, 0x50, 0xff},
};

static const CameraSensorRegister cameraEvMinus17Registers[4] = {
	{0x3a0f, 0x29, 0xff},
	{0x3a10, 0x24, 0xff},
	{0x3a1b, 0x2a, 0xff},
	{0x3a1e, 0x23, 0xff},
};

static const CameraSensorTiming cameraFramerate15Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0x6, 0xff},
	{0x380d, 0x28, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3035, 0x41, 0xf0},
	{0x3a08, 0x0, 0x3},
	{0x3a09, 0x4c, 0xff},
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0x3f, 0xff},
	{0x3a0d, 0x8, 0x3f},
	{0x3a0e, 0x6, 0x3f},
	{0x4837, 0x52, 0xff},
	{0x3632, 0x29, 0xff},
	{0x100, 0x0, 0xff},
}};

static const CameraSensorRegister cameraEvMinus15Registers[4] = {
	{0x3a0f, 0x2f, 0xff},
	{0x3a10, 0x2a, 0xff},
	{0x3a1b, 0x30, 0xff},
	{0x3a1e, 0x29, 0xff},
};

static const CameraSensorRegister cameraAntiflicker50ExtendedRegisters[2] = {
	{0x3c01, 0x0, 0x80},
	{0x3c00, 0x4, 0x4},
};

static const CameraSensorRegister cameraEvMinus13Registers[4] = {
	{0x3a0f, 0x35, 0xff},
	{0x3a10, 0x30, 0xff},
	{0x3a1b, 0x36, 0xff},
	{0x3a1e, 0x2f, 0xff},
};

static const CameraSensorTiming cameraFramerate10Registers = {{
	{0x100, 0x0, 0xff},
	{0x380c, 0x9, 0xff},
	{0x380d, 0x3c, 0xff},
	{0x380e, 0x1, 0xff},
	{0x380f, 0xfc, 0xff},
	{0x3035, 0x41, 0xf0},
	{0x3a08, 0x0, 0x3},
	{0x3a09, 0x32, 0xff},
	{0x3a0a, 0x0, 0x3},
	{0x3a0b, 0x2a, 0xff},
	{0x3a0d, 0xc, 0x3f},
	{0x3a0e, 0xa, 0x3f},
	{0x4837, 0x52, 0xff},
	{0x3632, 0x29, 0xff},
	{0x100, 0x0, 0xff},
}};

static CameraSensorRegister cameraWhiteBalance3Registers[11] = {
	{0x5001, 0x0, 0x1},
	{0x5180, 0x2, 0x2},
	{0x5195, 0x4, 0xff},
	{0x5196, 0x0, 0xff},
	{0x5197, 0x4, 0xff},
	{0x5198, 0xe0, 0xff},
	{0x5199, 0xa, 0xff},
	{0x519a, 0x60, 0xff},
	{0x5806, 0x18, 0xff},
	{0x5807, 0x1a, 0xff},
	{0x5808, 0x30, 0xff},
};

static CameraSensorRegister cameraEffect0Registers[6] = {
	{0x3208, 0x0, 0xff},
	{0x5580, 0x2, 0xff},
	{0x5583, 0x3f, 0xff},
	{0x5584, 0x20, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static CameraSensorRegister cameraEffect4Registers[6] = {
	{0x3208, 0x0, 0xff},
	{0x5580, 0x18, 0xff},
	{0x5583, 0xa0, 0xff},
	{0x5584, 0x40, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static CameraSensorRegister cameraSaturation20Registers[6] = {
	{0x5580, 0x2, 0xff},
	{0x5589, 0x18, 0xff},
	{0x558a, 0x0, 0xff},
	{0x558b, 0x80, 0xff},
	{0x5583, 0x50, 0xff},
	{0x5584, 0x20, 0xff},
};

static CameraSensorRegister cameraSaturation10Registers[6] = {
	{0x5580, 0x2, 0xff},
	{0x5589, 0x18, 0xff},
	{0x558a, 0x0, 0xff},
	{0x558b, 0x80, 0xff},
	{0x5583, 0x3f, 0xff},
	{0x5584, 0x20, 0xff},
};

static CameraSensorRegister cameraSaturation0Registers[6] = {
	{0x5580, 0x2, 0xff},
	{0x5589, 0x18, 0xff},
	{0x558a, 0x0, 0xff},
	{0x558b, 0x80, 0xff},
	{0x5583, 0x0, 0xff},
	{0x5584, 0x0, 0xff},
};

static CameraSensorRegister cameraGreenishEffectRegisters[6] = {
	{0x3208, 0, 0xff},
	{0x5580, 0x18, 0xff},
	{0x5583, 0x60, 0xff},
	{0x5584, 0x60, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static CameraSavedSensorState cameraSavedSensorState[2] = {
	{
		.colorAdjustment = {{0x5580, 0x2, 0xff}, {0x5583, 0x3f, 0xff},
			{0x5584, 0x20, 0xff}},
		.exposureControl = {0x3503, 0, 7},
		.exposure = {{0x3500, 0, 0xff}, {0x3501, 1, 0xff},
			{0x3502, 0x30, 0xff}},
		.gain = {{0x350a, 0, 0xff}, {0x350b, 0x10, 0xff}},
		.whiteBalanceControl = {{0x5001, 1, 1}, {0x5180, 0x54, 2}},
		.whiteBalance = {{0x5195, 4, 0xff}, {0x5196, 0, 0xff},
			{0x5197, 4, 0xff}, {0x5198, 0, 0xff},
			{0x5199, 4, 0xff}, {0x519a, 0, 0xff}},
		.whiteBalanceTail = {{0x5806, 0x1c, 0xff}, {0x5807, 0x21, 0xff},
			{0x5808, 0x2e, 0xff}},
	},
	{
		.colorAdjustment = {{0x5580, 0x2, 0xff}, {0x5583, 0x3f, 0xff},
			{0x5584, 0x20, 0xff}},
		.exposureControl = {0x3503, 0, 7},
		.exposure = {{0x3500, 0, 0xff}, {0x3501, 1, 0xff},
			{0x3502, 0x30, 0xff}},
		.gain = {{0x350a, 0, 0xff}, {0x350b, 0x10, 0xff}},
		.whiteBalanceControl = {{0x5001, 1, 1}, {0x5180, 0x54, 2}},
		.whiteBalance = {{0x5195, 4, 0xff}, {0x5196, 0, 0xff},
			{0x5197, 4, 0xff}, {0x5198, 0, 0xff},
			{0x5199, 4, 0xff}, {0x519a, 0, 0xff}},
		.whiteBalanceTail = {{0x5806, 0x1c, 0xff}, {0x5807, 0x21, 0xff},
			{0x5808, 0x2e, 0xff}},
	},
};

static CameraSensorRegister cameraSaturation5Registers[6] = {
	{0x5580, 0x2, 0xff},
	{0x5589, 0x18, 0xff},
	{0x558a, 0x0, 0xff},
	{0x558b, 0x80, 0xff},
	{0x5583, 0x20, 0xff},
	{0x5584, 0x10, 0xff},
};

static CameraSensorRegister cameraZoomRegisters[4] = {
	{0x3810, 0x0, 0xff},
	{0x3811, 0x0, 0xff},
	{0x3812, 0x0, 0xff},
	{0x3813, 0x0, 0xff},
};

static CameraSensorRegister cameraSensorTuningRegisters[79] = {
	{0x5025, 0x1, 0xff},
	{0x5481, 0xa, 0xff},
	{0x5482, 0x12, 0xff},
	{0x5483, 0x21, 0xff},
	{0x5484, 0x3d, 0xff},
	{0x5485, 0x4b, 0xff},
	{0x5486, 0x58, 0xff},
	{0x5487, 0x65, 0xff},
	{0x5488, 0x70, 0xff},
	{0x5489, 0x79, 0xff},
	{0x548a, 0x82, 0xff},
	{0x548b, 0x94, 0xff},
	{0x548c, 0xa3, 0xff},
	{0x548d, 0xbf, 0xff},
	{0x548e, 0xd6, 0xff},
	{0x548f, 0xeb, 0xff},
	{0x5490, 0x1c, 0xff},
	{0x5800, 0x0, 0xff},
	{0x5801, 0x0, 0xff},
	{0x5802, 0x1, 0xff},
	{0x5803, 0x48, 0xff},
	{0x5804, 0x0, 0xff},
	{0x5805, 0xf4, 0xff},
	{0x5806, 0x1c, 0xff},
	{0x5807, 0x21, 0xff},
	{0x5808, 0x2e, 0xff},
	{0x5809, 0x3, 0xff},
	{0x580a, 0xc, 0xff},
	{0x580b, 0x30, 0xff},
	{0x5381, 0x22, 0xff},
	{0x5382, 0x3a, 0xff},
	{0x5383, 0x24, 0xff},
	{0x5384, 0x6, 0xff},
	{0x5385, 0x7a, 0xff},
	{0x5386, 0x80, 0xff},
	{0x5387, 0x6c, 0xff},
	{0x5388, 0x59, 0xff},
	{0x5389, 0x13, 0xff},
	{0x538b, 0x98, 0xff},
	{0x538a, 0x1, 0xff},
	{0x5308, 0x0, 0xff},
	{0x5300, 0x18, 0xff},
	{0x5301, 0x20, 0xff},
	{0x5302, 0x38, 0xff},
	{0x5303, 0x18, 0xff},
	{0x5304, 0x18, 0xff},
	{0x5305, 0x20, 0xff},
	{0x5306, 0xa, 0xff},
	{0x5307, 0x20, 0xff},
	{0x5580, 0x2, 0xff},
	{0x5589, 0x18, 0xff},
	{0x558a, 0x0, 0xff},
	{0x558b, 0x80, 0xff},
	{0x5583, 0x3f, 0xff},
	{0x5584, 0x20, 0xff},
	{0x5180, 0x54, 0xff},
	{0x5182, 0x41, 0xff},
	{0x5183, 0x31, 0xff},
	{0x5184, 0x80, 0xff},
	{0x5185, 0x57, 0xff},
	{0x5186, 0xab, 0xff},
	{0x5187, 0x8d, 0xff},
	{0x5188, 0x11, 0xff},
	{0x5189, 0x1c, 0xff},
	{0x518a, 0xe, 0xff},
	{0x518b, 0x43, 0xff},
	{0x518c, 0x2e, 0xff},
	{0x518d, 0xf0, 0xff},
	{0x518e, 0x4, 0xff},
	{0x519b, 0x2, 0xff},
	{0x518f, 0x52, 0xff},
	{0x5190, 0x0, 0xff},
	{0x5191, 0xff, 0xff},
	{0x5192, 0x0, 0xff},
	{0x5193, 0xff, 0xff},
	{0x5194, 0x0, 0xff},
	{0x3a08, 0x1, 0x3},
	{0x3a09, 0x30, 0xff},
	{0x3a0e, 0x1, 0x3f},
};

static CameraAntiflickerState cameraAntiflickerState[2] = {{1, 1}, {1, 1}};

static CameraSensorRegister cameraEffect1Registers[4] = {
	{0x3208, 0x0, 0xff},
	{0x5580, 0x42, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static CameraSensorRegister cameraEffect2Registers[4] = {
	{0x3208, 0x0, 0xff},
	{0x5580, 0x22, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static CameraSensorRegister cameraAntiflickerRegisters[2][10] = {
	{
		{0x3503, 0x7, 0xff},
		{0x3208, 0x0, 0xff},
		{0x3a08, 0x1, 0xff},
		{0x3a09, 0x30, 0xff},
		{0x3a0a, 0x1, 0xff},
		{0x3a0b, 0x30, 0xff},
		{0x3a0d, 0x1, 0xff},
		{0x3a0e, 0x1, 0xff},
		{0x3208, 0x10, 0xff},
		{0x3208, 0xa0, 0xff},
	},
	{
		{0x3503, 0x7, 0xff},
		{0x3208, 0x0, 0xff},
		{0x3a08, 0x1, 0xff},
		{0x3a09, 0x30, 0xff},
		{0x3a0a, 0x1, 0xff},
		{0x3a0b, 0x30, 0xff},
		{0x3a0d, 0x1, 0xff},
		{0x3a0e, 0x1, 0xff},
		{0x3208, 0x10, 0xff},
		{0x3208, 0xa0, 0xff},
	},
};

static CameraSensorRegister cameraWhiteBalance2Registers[11] = {
	{0x5001, 0x0, 0x1},
	{0x5180, 0x2, 0x2},
	{0x5195, 0x4, 0xff},
	{0x5196, 0xd0, 0xff},
	{0x5197, 0x4, 0xff},
	{0x5198, 0x0, 0xff},
	{0x5199, 0x8, 0xff},
	{0x519a, 0x80, 0xff},
	{0x5806, 0x1a, 0xff},
	{0x5807, 0x1d, 0xff},
	{0x5808, 0x26, 0xff},
};

static CameraSensorRegister cameraExposureGroupRegisters[2][4] = {
	{
		{0x3208, 0x0, 0xff},
		{0x3503, 0x0, 0xff},
		{0x3208, 0x10, 0xff},
		{0x3208, 0xa0, 0xff},
	},
	{
		{0x3208, 0x0, 0xff},
		{0x3503, 0x0, 0xff},
		{0x3208, 0x10, 0xff},
		{0x3208, 0xa0, 0xff},
	},
};

static CameraSensorTiming cameraFramerateRegisters[2] = {
	{{
		{0x100, 0x0, 0xff},
		{0x380c, 0x3, 0xff},
		{0x380d, 0x14, 0xff},
		{0x380e, 0x1, 0xff},
		{0x380f, 0xfc, 0xff},
		{0x3035, 0x21, 0xf0},
		{0x3a08, 0x1, 0x3},
		{0x3a09, 0x30, 0xff},
		{0x3a0a, 0x1, 0x3},
		{0x3a0b, 0x30, 0xff},
		{0x3a0d, 0x1, 0x3f},
		{0x3a0e, 0x1, 0x3f},
		{0x4837, 0x28, 0xff},
		{0x3632, 0xc9, 0xff},
		{0x100, 0x1, 0xff},
	}},
	{{
		{0x100, 0x0, 0xff},
		{0x380c, 0x3, 0xff},
		{0x380d, 0x14, 0xff},
		{0x380e, 0x1, 0xff},
		{0x380f, 0xfc, 0xff},
		{0x3035, 0x21, 0xf0},
		{0x3a08, 0x1, 0x3},
		{0x3a09, 0x30, 0xff},
		{0x3a0a, 0x1, 0x3},
		{0x3a0b, 0x30, 0xff},
		{0x3a0d, 0x1, 0x3f},
		{0x3a0e, 0x1, 0x3f},
		{0x4837, 0x28, 0xff},
		{0x3632, 0xc9, 0xff},
		{0x100, 0x1, 0xff},
	}},
};

static CameraSensorRegister cameraWhiteBalance1Registers[11] = {
	{0x5001, 0x0, 0x1},
	{0x5180, 0x2, 0x2},
	{0x5195, 0x5, 0xff},
	{0x5196, 0x80, 0xff},
	{0x5197, 0x4, 0xff},
	{0x5198, 0x0, 0xff},
	{0x5199, 0x5, 0xff},
	{0x519a, 0xc0, 0xff},
	{0x5806, 0x20, 0xff},
	{0x5807, 0x21, 0xff},
	{0x5808, 0x30, 0xff},
};

static CameraSensorRegister cameraEffect3Registers[6] = {
	{0x3208, 0x0, 0xff},
	{0x5580, 0x18, 0xff},
	{0x5583, 0x40, 0xff},
	{0x5584, 0xa0, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static CameraSensorRegister cameraWhiteBalanceAutoRegisters[5] = {
	{0x5001, 0x1, 0x1},
	{0x5180, 0x0, 0x2},
	{0x5806, 0x1c, 0xff},
	{0x5807, 0x21, 0xff},
	{0x5808, 0x2e, 0xff},
};

static CameraSensorRegister cameraSaturation40Registers[6] = {
	{0x5580, 0x2, 0xff},
	{0x5589, 0x18, 0xff},
	{0x558a, 0x0, 0xff},
	{0x558b, 0x80, 0xff},
	{0x5583, 0x80, 0xff},
	{0x5584, 0x20, 0xff},
};

static CameraSensorRegister cameraSaturation30Registers[6] = {
	{0x5580, 0x2, 0xff},
	{0x5589, 0x18, 0xff},
	{0x558a, 0x0, 0xff},
	{0x558b, 0x80, 0xff},
	{0x5583, 0x60, 0xff},
	{0x5584, 0x20, 0xff},
};

static CameraSensorRegister cameraEffect5Registers[6] = {
	{0x3208, 0x0, 0xff},
	{0x5580, 0x18, 0xff},
	{0x5583, 0x80, 0xff},
	{0x5584, 0xc0, 0xff},
	{0x3208, 0x10, 0xff},
	{0x3208, 0xa0, 0xff},
};

static CameraSensorInfo cameraSensorInfo[2];

int cameraLockDevice(int devnum)
{
	SceUInt32 timeout = (SceUInt32)-1;

	return sceKernelLockMutex(cameraDriver->device[devnum].mutexId, 1, &timeout);
}

int cameraUnlockDevice(int devnum)
{
	return sceKernelUnlockMutex(cameraDriver->device[devnum].mutexId, 1);
}

int cameraWriteSensorByte(int devnum, SceUInt16 address, const SceUInt8 *value)
{
	SceUInt8 buffer[64];
	SceUInt8 data;
	int result = cameraDriver->i2cEnabled;

	if (result) {
		result = cameraDriver->clockEnabled;
		if (result) {
			memset(buffer, 0, 62);
			data = *value;
			buffer[1] = address;
			buffer[2] = data;
			buffer[0] = address >> 8;
			result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, 3);
		}
	}
	return result;
}

int cameraReadSensorByte(int devnum, SceUInt16 address, SceUInt8 *value)
{
	SceUInt8 buffer[2];
	int result = cameraDriver->i2cEnabled;

	if (result) {
		result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = address >> 8;
			buffer[1] = address;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, value, 1);
		}
	}
	return result;
}

int cameraWriteSensorRegisters(int devnum, const CameraSensorRegister *registers, int count)
{
	SceUInt8 readValue = 0;
	SceUInt8 values[60], buffer[64];
	SceUInt16 firstAddress = 0;
	unsigned int length = 0;
	int result, index = 0;

	memset(values, 0, sizeof(values));
	result = cameraDriver->i2cEnabled;
	if (!result)
		return result;
	result = cameraDriver->clockEnabled;
	if (!result)
		return result;
	if (count <= 0)
		return 0;
	for (;;) {
		unsigned int address = registers->address;
		SceUInt8 mask = registers->mask;

		if (mask == 0xff) {
			readValue = registers->value;
		} else {
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				buffer[1] = address;
				buffer[0] = address >> 8;
				result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
					0x74, &readValue, 1);
				if (result < 0)
					return result;
				mask = registers->mask;
				address = registers->address;
			}
			readValue = (readValue & ~mask) | (mask & registers->value);
		}
		if (devnum == 1 && address == 0x3821)
			readValue ^= 6;
		else if (address == 0x3820)
			readValue ^= 6;
		index++;
		if (length == 0)
			firstAddress = address;
		values[length++] = readValue;
		if (count > index && address + 1 == registers[1].address && length <= 59) {
			registers++;
			continue;
		}
		if ((SceUInt8)(length - 1) > 59) {
			length = 0x802e0000;
		} else if (cameraDriver->i2cEnabled) {
			if (cameraDriver->clockEnabled) {
				memset(buffer, 0, 62);
				buffer[1] = firstAddress;
				buffer[0] = firstAddress >> 8;
				length = (SceUInt8)length;
				if (length)
					memcpy(buffer + 2, values, length);
				length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
			} else {
				length = 0;
			}
		} else {
			length = 0;
		}
		memset(values, 0, sizeof(values));
		if (count <= index)
			return length;
		length = 0;
		registers++;
	}
}

int cameraSaveSensorAttribute(int devnum, int group, int readHardware,
		CameraProcess *process, const CameraSensorRegister *registers)
{
	SceUInt8 readValue = 0, buffer[2];
	int result;

	if (group == 2) {
		if (!readHardware) {
			SceUInt8 exposureControl = process->sensorRegisterValues[3];

			process->sensorRegisterValues[7] = registers[1].value;
			process->sensorRegisterValues[8] = registers[2].value;
			process->sensorRegisterValues[3] =
				(registers[0].value & 7) | (exposureControl & 0xf8);
			return 0;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x35;
			buffer[1] = 0xa;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].gain[0].value = readValue;
		} else {
			cameraSavedSensorState[devnum].gain[0].value = 0;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x35;
			buffer[1] = 0xb;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].gain[1].value = readValue;
		} else {
			cameraSavedSensorState[devnum].gain[1].value = readValue;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x35;
			buffer[1] = 0x3;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].exposureControl.value = readValue;
		} else {
			cameraSavedSensorState[devnum].exposureControl.value = readValue;
		}
		return result;
	}
	if (group == 3) {
		if (!readHardware) {
			SceUInt8 whiteBalanceControl = process->sensorRegisterValues[10];

			process->sensorRegisterValues[9] = (process->sensorRegisterValues[9] & 0xfe) | (registers[0].value & 1);
			process->sensorRegisterValues[10] = (whiteBalanceControl & 0xfd) | (registers[1].value & 2);
			process->sensorRegisterValues[11] = registers[2].value;
			process->sensorRegisterValues[12] = registers[3].value;
			process->sensorRegisterValues[13] = registers[4].value;
			process->sensorRegisterValues[14] = registers[5].value;
			process->sensorRegisterValues[15] = registers[6].value;
			process->sensorRegisterValues[16] = registers[7].value;
			process->sensorRegisterValues[17] = registers[8].value;
			process->sensorRegisterValues[18] = registers[9].value;
			process->sensorRegisterValues[19] = registers[10].value;
			return 0;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x50;
			buffer[1] = 0x1;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalanceControl[0].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalanceControl[0].value = 0;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x51;
			buffer[1] = 0x80;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalanceControl[1].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalanceControl[1].value = readValue;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x51;
			buffer[1] = 0x95;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalance[0].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalance[0].value = readValue;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x51;
			buffer[1] = 0x96;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalance[1].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalance[1].value = readValue;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x51;
			buffer[1] = 0x97;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalance[2].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalance[2].value = readValue;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x51;
			buffer[1] = 0x98;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalance[3].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalance[3].value = readValue;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x51;
			buffer[1] = 0x99;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalance[4].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalance[4].value = readValue;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x51;
			buffer[1] = 0x9a;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalance[5].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalance[5].value = readValue;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x58;
			buffer[1] = 0x6;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalanceTail[0].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalanceTail[0].value = readValue;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x58;
			buffer[1] = 0x7;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalanceTail[1].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalanceTail[1].value = readValue;
		}
		result = cameraDriver->i2cEnabled;
		if (result)
			result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = 0x58;
			buffer[1] = 0x8;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, &readValue, 1);
			if (result >= 0)
				cameraSavedSensorState[devnum].whiteBalanceTail[2].value = readValue;
		} else {
			cameraSavedSensorState[devnum].whiteBalanceTail[2].value = readValue;
		}
		return result;
	}
	if (group != 1)
		return (int)0x802e00ff;
	if (!readHardware)
		return 0;
	result = cameraDriver->i2cEnabled;
	if (result)
		result = cameraDriver->clockEnabled;
	if (result) {
		buffer[0] = 0x35;
		buffer[1] = 0x0;
		result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
		0x74, &readValue, 1);
		if (result >= 0)
		cameraSavedSensorState[devnum].exposure[0].value = readValue;
	} else {
		cameraSavedSensorState[devnum].exposure[0].value = 0;
	}
	result = cameraDriver->i2cEnabled;
	if (result)
		result = cameraDriver->clockEnabled;
	if (result) {
		buffer[0] = 0x35;
		buffer[1] = 0x1;
		result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
		0x74, &readValue, 1);
		if (result >= 0)
		cameraSavedSensorState[devnum].exposure[1].value = readValue;
	} else {
		cameraSavedSensorState[devnum].exposure[1].value = readValue;
	}
	result = cameraDriver->i2cEnabled;
	if (result)
		result = cameraDriver->clockEnabled;
	if (result) {
		buffer[0] = 0x35;
		buffer[1] = 0x2;
		result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
		0x74, &readValue, 1);
		if (result >= 0)
		cameraSavedSensorState[devnum].exposure[2].value = readValue;
	} else {
		cameraSavedSensorState[devnum].exposure[2].value = readValue;
	}
	result = cameraDriver->i2cEnabled;
	if (result)
		result = cameraDriver->clockEnabled;
	if (result) {
		buffer[0] = 0x35;
		buffer[1] = 0x3;
		result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
		0x74, &readValue, 1);
		if (result >= 0)
		cameraSavedSensorState[devnum].exposureControl.value = readValue;
	} else {
		cameraSavedSensorState[devnum].exposureControl.value = readValue;
	}
	return result;
}

int cameraSetAntiflickerOV7735(int devnum, int mode)
{
	CameraDriver *driver = cameraDriver;
	CameraDevice *device = &driver->device[devnum];
	const CameraSensorRegister *timing = NULL, *registers;
	SceUInt8 readValue = 0, restoreValue = 0;
	SceUInt8 values[60], buffer[64];
	SceUInt16 firstAddress;
	unsigned int length, high, low, ceiling;
	unsigned int wFramerate, wResolution;
	int exposureCeiling;
	int applied, result, index;

	if (mode == 1)
		applied = 1;
	else if (mode == 2)
		applied = 0;
	else
		applied = cameraAntiflickerState[devnum].detected;
	wFramerate = device->config.wFramerate;
	wResolution = device->config.wResolution;
	exposureCeiling = device->config.exposureCeiling;
	if (applied == 1) {
		high = 6;
		low = 7;
		ceiling = 11;
	} else {
		high = 8;
		low = 9;
		ceiling = 10;
	}
	switch (wFramerate) {
	case 3:
		timing = cameraFramerate3Registers.registers;
		break;
	case 5:
		timing = cameraFramerate5Registers.registers;
		break;
	case 7:
		timing = cameraFramerate7Registers.registers;
		break;
	case 10:
		timing = cameraFramerate10Registers.registers;
		break;
	case 15:
		timing = cameraFramerate15Registers.registers;
		break;
	case 20:
		timing = cameraFramerate20Registers.registers;
		break;
	case 24:
		timing = cameraFramerate24Registers.registers;
		break;
	case 25:
		timing = cameraFramerate25Registers.registers;
		break;
	case 30:
		timing = cameraFramerate30Registers.registers;
		break;
	case 60:
		timing = cameraFramerate60Registers.registers;
		break;
	case 120:
		if ((unsigned int)(wResolution - 2) > 3 ||
		    !(0xb & (1 << (wResolution - 2))))
			break;
		timing = cameraFramerate120Registers.registers;
		break;
	}
	if (timing != NULL) {
		SceUInt8 highValue = timing[high].value;
		SceUInt8 lowValue = timing[low].value;
		SceUInt8 ceilingValue = timing[ceiling].value;

		if (wFramerate == 60 && exposureCeiling == 1)
			ceilingValue = 1;
		cameraAntiflickerRegisters[devnum][2].value = highValue;
		cameraAntiflickerRegisters[devnum][3].value = lowValue;
		cameraAntiflickerRegisters[devnum][4].value = highValue;
		cameraAntiflickerRegisters[devnum][5].value = lowValue;
		cameraAntiflickerRegisters[devnum][7].value = ceilingValue;
		cameraAntiflickerRegisters[devnum][6].value = ceilingValue;
	}
	cameraExposureGroupRegisters[devnum][1].value = cameraSavedSensorState[devnum].exposureControl.value;
	firstAddress = 0;
	length = 0;
	index = 0;
	readValue = 0;
	memset(values, 0, sizeof(values));
	result = driver->i2cEnabled;
	if (result)
		result = driver->clockEnabled;
	if (result) {
		registers = cameraAntiflickerRegisters[devnum];
		for (;;) {
			unsigned int address = registers->address;
			SceUInt8 mask = registers->mask;

			if (mask == 0xff) {
				readValue = registers->value;
			} else {
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					buffer[1] = address;
					buffer[0] = address >> 8;
					result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
						0x74, &readValue, 1);
					if (result < 0)
						return result;
					mask = registers->mask;
					address = registers->address;
				}
				readValue = (readValue & ~mask) | (mask & registers->value);
			}
			if (devnum == 1 && address == 0x3821)
				readValue ^= 6;
			else if (address == 0x3820)
				readValue ^= 6;
			index++;
			if (length == 0)
				firstAddress = address;
			values[length++] = readValue;
			if (index != 10 && address + 1 == registers[1].address && length <= 59) {
				registers++;
				continue;
			}
			if ((SceUInt8)(length - 1) > 59) {
				result = (int)0x802e0000;
			} else {
				result = cameraDriver->i2cEnabled;
				if (result)
					result = cameraDriver->clockEnabled;
				if (result) {
					memset(buffer, 0, 62);
					buffer[1] = firstAddress;
					buffer[0] = firstAddress >> 8;
					length = (SceUInt8)length;
					if (length)
						memcpy(buffer + 2, values, length);
					result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
				}
			}
			memset(values, 0, sizeof(values));
			if (index == 10)
				break;
			length = 0;
			registers++;
		}
	}
	if (result < 0)
		return result;
	if (device->config.frameTimeout)
		sceKernelDelayThread(device->config.frameTimeout);
	firstAddress = 0;
	length = 0;
	index = 0;
	restoreValue = 0;
	memset(values, 0, sizeof(values));
	result = cameraDriver->i2cEnabled;
	if (result)
		result = cameraDriver->clockEnabled;
	if (result) {
		registers = cameraExposureGroupRegisters[devnum];
		for (;;) {
			unsigned int address = registers->address;
			SceUInt8 mask = registers->mask;

			if (mask == 0xff) {
				restoreValue = registers->value;
			} else {
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					buffer[1] = address;
					buffer[0] = address >> 8;
					result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
						0x74, &restoreValue, 1);
					if (result < 0)
						goto finishAntiflicker;
					mask = registers->mask;
					address = registers->address;
				}
				restoreValue = (restoreValue & ~mask) | (mask & registers->value);
			}
			if (devnum == 1 && address == 0x3821)
				restoreValue ^= 6;
			else if (address == 0x3820)
				restoreValue ^= 6;
			index++;
			if (length == 0)
				firstAddress = address;
			values[length++] = restoreValue;
			if (index != 4 && address + 1 == registers[1].address && length <= 59) {
				registers++;
				continue;
			}
			if ((SceUInt8)(length - 1) > 59) {
				result = (int)0x802e0000;
			} else {
				result = cameraDriver->i2cEnabled;
				if (result)
					result = cameraDriver->clockEnabled;
				if (result) {
					memset(buffer, 0, 62);
					buffer[1] = firstAddress;
					buffer[0] = firstAddress >> 8;
					length = (SceUInt8)length;
					if (length)
						memcpy(buffer + 2, values, length);
					result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
				}
			}
			memset(values, 0, sizeof(values));
			if (index == 4)
				break;
			length = 0;
			registers++;
		}
	}
finishAntiflicker:
	if (device->config.frameTimeout)
		sceKernelDelayThread(device->config.frameTimeout);
	if (result == 0)
		cameraAntiflickerState[devnum].applied = applied;
	return result;
}

int cameraWriteRegisterOV7735(int devnum, SceUInt16 address, SceUInt8 value)
{
	SceUInt8 buffer[64];
	int result = cameraDriver->i2cEnabled;

	if (result) {
		result = cameraDriver->clockEnabled;
		if (result) {
			memset(buffer, 0, 62);
			buffer[1] = address;
			buffer[0] = address >> 8;
			buffer[2] = value;
			result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, 3);
		}
	}
	return result;
}

int cameraReadRegisterOV7735(int devnum, SceUInt16 address, SceUInt8 *value)
{
	SceUInt8 buffer[4];
	int result = cameraDriver->i2cEnabled;

	if (result) {
		result = cameraDriver->clockEnabled;
		if (result) {
			buffer[0] = address >> 8;
			buffer[1] = address;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
				0x74, value, 1);
		}
	}
	return result;
}

int cameraWriteRegistersOV7735(int devnum, const void *registers, unsigned int count)
{
	return cameraWriteSensorRegisters(devnum, registers, count);
}


int cameraProbeOV7735(int devnum)
{
	SceUInt8 buffer[4];
	SceUInt8 chipId[8];
	SceUInt8 revision[8];
	int result;

	cameraSensorInfo[devnum].chipId = 0;
	cameraSensorInfo[devnum].revision = 0;
	cameraSensorInfo[devnum].probeResult = 0;
	if (!cameraDriver->i2cEnabled || !cameraDriver->clockEnabled ||
		(buffer[0] = 0x30, buffer[1] = 0x0a,
		(result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
			0x74, chipId, 2)) >= 0)) {
		cameraSensorInfo[devnum].chipId = chipId[1] | chipId[0] << 8;
		if (chipId[0] == 0x77 && chipId[1] == 0x35) {
			result = cameraDriver->i2cEnabled;
			if (!result || !(result = cameraDriver->clockEnabled) ||
				(buffer[0] = 0x30, buffer[1] = 0x2a,
				(result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
					0x74, revision, 1)) >= 0))
				cameraSensorInfo[devnum].revision = revision[0];
		} else {
			result = -1;
		}
	}
	return result;
}

int cameraSetControlModeOV7735(int devnum, int command, int mode)
{
	(void)command;
	cameraSensorInfo[devnum].controlMode = mode;
	return 0;
}

int cameraGetProbeResultOV7735(int devnum, int command, SceUInt16 *result)
{
	SceUInt16 chipId = cameraSensorInfo[devnum].chipId;
	SceUInt16 revision = cameraSensorInfo[devnum].revision;
	SceUInt16 probeResult = cameraSensorInfo[devnum].probeResult;

	(void)command;
	result[0] = chipId;
	result[1] = revision;
	result[2] = probeResult;
	return 0;
}

int cameraInitializeOV7735(int devnum)
{
	SceUInt8 resetRead[8], commonRead[8], tuningRead[8], timingRead[4];
	SceUInt8 values[60], buffer[64];
	CameraSensorInfo *sensor = &cameraSensorInfo[devnum];
	int mode = sensor->controlMode;
	int result;

	sensor->useExtendedTiming = mode == 3 ||
		(mode != 4 && (sensor->revision & 0xffef) != 0xe0);
	{
		const CameraSensorRegister *registers = cameraSensorResetRegisters;
		SceUInt16 firstAddress = 0;
		unsigned int length = 0;
		int index = 0;

		resetRead[0] = 0;
		memset(values, 0, sizeof(values));
		if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
			for (;;) {
				unsigned int address = registers->address;
				SceUInt8 mask = registers->mask;

				if (mask == 0xff) {
					resetRead[0] = registers->value;
				} else {
					if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
						buffer[1] = address;
						buffer[0] = address >> 8;
						result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
							0x74, &resetRead[0], 1);
						if (result < 0)
							return result;
						mask = registers->mask;
						address = registers->address;
					}
					resetRead[0] = (resetRead[0] & ~mask) | (mask & registers->value);
				}
				if (devnum == 1 && address == 0x3821)
					resetRead[0] ^= 6;
				else if (address == 0x3820)
					resetRead[0] ^= 6;
				index++;
				if (length == 0)
					firstAddress = address;
				values[length++] = resetRead[0];
				if (index != 2 && address + 1 == registers[1].address && length <= 59) {
					registers++;
					continue;
				}
				if ((SceUInt8)(length - 1) > 59) {
					length = 0x802e0000;
				} else if (cameraDriver->i2cEnabled) {
					if (cameraDriver->clockEnabled) {
						memset(buffer, 0, 62);
						buffer[1] = firstAddress;
						buffer[0] = firstAddress >> 8;
						length = (SceUInt8)length;
						if (length)
							memcpy(buffer + 2, values, length);
						length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
					} else {
						length = 0;
					}
				} else {
					length = 0;
				}
				memset(values, 0, sizeof(values));
				if (index == 2)
					break;
				length = 0;
				registers++;
			}
			result = length;
		} else {
			result = 0;
		}
	}
	if (result < 0)
		return result;
	sceKernelDelayThread(5000);
	{
		const CameraSensorRegister *registers = cameraSensorCommonRegisters;
		SceUInt16 firstAddress = 0;
		unsigned int length = 0;
		int index = 0;

		commonRead[0] = 0;
		memset(values, 0, sizeof(values));
		if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
			for (;;) {
				unsigned int address = registers->address;
				SceUInt8 mask = registers->mask;

				if (mask == 0xff) {
					commonRead[0] = registers->value;
				} else {
					if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
						buffer[1] = address;
						buffer[0] = address >> 8;
						result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
							0x74, &commonRead[0], 1);
						if (result < 0)
							return result;
						mask = registers->mask;
						address = registers->address;
					}
					commonRead[0] = (commonRead[0] & ~mask) | (mask & registers->value);
				}
				if (devnum == 1 && address == 0x3821)
					commonRead[0] ^= 6;
				else if (address == 0x3820)
					commonRead[0] ^= 6;
				index++;
				if (length == 0)
					firstAddress = address;
				values[length++] = commonRead[0];
				if (index != 128 && address + 1 == registers[1].address && length <= 59) {
					registers++;
					continue;
				}
				if ((SceUInt8)(length - 1) > 59) {
					length = 0x802e0000;
				} else if (cameraDriver->i2cEnabled) {
					if (cameraDriver->clockEnabled) {
						memset(buffer, 0, 62);
						buffer[1] = firstAddress;
						buffer[0] = firstAddress >> 8;
						length = (SceUInt8)length;
						if (length)
							memcpy(buffer + 2, values, length);
						length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
					} else {
						length = 0;
					}
				} else {
					length = 0;
				}
				memset(values, 0, sizeof(values));
				if (index == 128)
					break;
				length = 0;
				registers++;
			}
			result = length;
		} else {
			result = 0;
		}
	}
	if (result < 0)
		return result;
	{
		const CameraSensorRegister *registers = cameraSensorTuningRegisters;
		SceUInt16 firstAddress = 0;
		unsigned int length = 0;
		int index = 0;

		tuningRead[0] = 0;
		memset(values, 0, sizeof(values));
		if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
			for (;;) {
				unsigned int address = registers->address;
				SceUInt8 mask = registers->mask;

				if (mask == 0xff) {
					tuningRead[0] = registers->value;
				} else {
					if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
						buffer[1] = address;
						buffer[0] = address >> 8;
						result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
							0x74, &tuningRead[0], 1);
						if (result < 0)
							return result;
						mask = registers->mask;
						address = registers->address;
					}
					tuningRead[0] = (tuningRead[0] & ~mask) | (mask & registers->value);
				}
				if (devnum == 1 && address == 0x3821)
					tuningRead[0] ^= 6;
				else if (address == 0x3820)
					tuningRead[0] ^= 6;
				index++;
				if (length == 0)
					firstAddress = address;
				values[length++] = tuningRead[0];
				if (index != 79 && address + 1 == registers[1].address && length <= 59) {
					registers++;
					continue;
				}
				if ((SceUInt8)(length - 1) > 59) {
					length = 0x802e0000;
				} else if (cameraDriver->i2cEnabled) {
					if (cameraDriver->clockEnabled) {
						memset(buffer, 0, 62);
						buffer[1] = firstAddress;
						buffer[0] = firstAddress >> 8;
						length = (SceUInt8)length;
						if (length)
							memcpy(buffer + 2, values, length);
						length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
					} else {
						length = 0;
					}
				} else {
					length = 0;
				}
				memset(values, 0, sizeof(values));
				if (index == 79)
					break;
				length = 0;
				registers++;
			}
			result = length;
		} else {
			result = 0;
		}
	}
	if (result < 0)
		return result;
	if (sensor->useExtendedTiming == 1) {
		{
			const CameraSensorRegister *registers = cameraSensorExtendedTimingRegisters;
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;

			timingRead[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					unsigned int address = registers->address;
					SceUInt8 mask = registers->mask;

					if (mask == 0xff) {
						timingRead[0] = registers->value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[1] = address;
							buffer[0] = address >> 8;
							result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &timingRead[0], 1);
							if (result < 0)
								return result;
							mask = registers->mask;
							address = registers->address;
						}
						timingRead[0] = (timingRead[0] & ~mask) | (mask & registers->value);
					}
					if (devnum == 1 && address == 0x3821)
						timingRead[0] ^= 6;
					else if (address == 0x3820)
						timingRead[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = timingRead[0];
					if (index != 10 && address + 1 == registers[1].address && length <= 59) {
						registers++;
						continue;
					}
					if ((SceUInt8)(length - 1) > 59) {
						length = 0x802e0000;
					} else if (cameraDriver->i2cEnabled) {
						if (cameraDriver->clockEnabled) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						} else {
							length = 0;
						}
					} else {
						length = 0;
					}
					memset(values, 0, sizeof(values));
					if (index == 10)
						break;
					length = 0;
					registers++;
				}
				result = length;
			} else {
				result = 0;
			}
		}
	} else {
		{
			const CameraSensorRegister *registers = cameraSensorBasicTimingRegisters;
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;

			timingRead[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					unsigned int address = registers->address;
					SceUInt8 mask = registers->mask;

					if (mask == 0xff) {
						timingRead[0] = registers->value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[1] = address;
							buffer[0] = address >> 8;
							result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &timingRead[0], 1);
							if (result < 0)
								return result;
							mask = registers->mask;
							address = registers->address;
						}
						timingRead[0] = (timingRead[0] & ~mask) | (mask & registers->value);
					}
					if (devnum == 1 && address == 0x3821)
						timingRead[0] ^= 6;
					else if (address == 0x3820)
						timingRead[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = timingRead[0];
					if (index != 6 && address + 1 == registers[1].address && length <= 59) {
						registers++;
						continue;
					}
					if ((SceUInt8)(length - 1) > 59) {
						length = 0x802e0000;
					} else if (cameraDriver->i2cEnabled) {
						if (cameraDriver->clockEnabled) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						} else {
							length = 0;
						}
					} else {
						length = 0;
					}
					memset(values, 0, sizeof(values));
					if (index == 6)
						break;
					length = 0;
					registers++;
				}
				result = length;
			} else {
				result = 0;
			}
		}
	}
	return result;
}

int cameraResetOV7735(int devnum)
{
	SceUInt8 buffer[64];
	int result = cameraDriver->i2cEnabled;

	if (result) {
		result = cameraDriver->clockEnabled;
		if (result) {
			memset(buffer, 0, 62);
			buffer[1] = 3;
			buffer[0] = 1;
			buffer[2] = 1;
			result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, 3);
		}
	}
	sceKernelDelayThread(5000);
	return result;
}

int cameraSetFormatOV7735(int devnum, int format)
{
	SceUInt8 readValue[4], values[60], buffer[64];
	int result;

	if (format <= 0)
		return 0x802e0007;
	if (format > 5) {
		if (format != 6)
			return 0x802e0007;
		{
			const CameraSensorRegister *registers = cameraSensorRaw8Registers + 1;
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;

			readValue[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					unsigned int address = registers[-1].address;
					SceUInt8 mask = registers[-1].mask;

					if (mask == 0xff) {
						readValue[0] = registers[-1].value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[1] = address;
							buffer[0] = address >> 8;
							result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &readValue[0], 1);
							if (result < 0)
								return result;
							mask = registers[-1].mask;
							address = registers[-1].address;
						}
						readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
					}
					if (devnum == 1 && address == 0x3821)
						readValue[0] ^= 6;
					else if (address == 0x3820)
						readValue[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = readValue[0];
					if (index != 9 && address + 1 == registers[0].address && length <= 59) {
						registers++;
						continue;
					}
					if ((SceUInt8)(length - 1) > 59) {
						length = 0x802e0000;
					} else if (cameraDriver->i2cEnabled) {
						if (cameraDriver->clockEnabled) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						} else {
							length = 0;
						}
					} else {
						length = 0;
					}
					memset(values, 0, sizeof(values));
					if (index == 9)
						break;
					length = 0;
					registers++;
				}
				result = length;
			} else {
				result = 0;
			}
		}
	} else {
		{
			const CameraSensorRegister *registers = cameraSensorProcessedFormatRegisters + 1;
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;

			readValue[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					unsigned int address = registers[-1].address;
					SceUInt8 mask = registers[-1].mask;

					if (mask == 0xff) {
						readValue[0] = registers[-1].value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[1] = address;
							buffer[0] = address >> 8;
							result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &readValue[0], 1);
							if (result < 0)
								return result;
							mask = registers[-1].mask;
							address = registers[-1].address;
						}
						readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
					}
					if (devnum == 1 && address == 0x3821)
						readValue[0] ^= 6;
					else if (address == 0x3820)
						readValue[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = readValue[0];
					if (index != 9 && address + 1 == registers[0].address && length <= 59) {
						registers++;
						continue;
					}
					if ((SceUInt8)(length - 1) > 59) {
						length = 0x802e0000;
					} else if (cameraDriver->i2cEnabled) {
						if (cameraDriver->clockEnabled) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						} else {
							length = 0;
						}
					} else {
						length = 0;
					}
					memset(values, 0, sizeof(values));
					if (index == 9)
						break;
					length = 0;
					registers++;
				}
				result = length;
			} else {
				result = 0;
			}
		}
	}
	return result;
}

int cameraSetFramerateOV7735(int devnum, int framerate, int resolution)
{
	SceUInt8 timingRead[8], antiflickerRead[8], exposureRead[4];
	SceUInt8 values[60], buffer[64];
	CameraDriver *driver;
	CameraSensorTiming *timing = &cameraFramerateRegisters[devnum];
	CameraSensorRegister *antiflicker = cameraAntiflickerRegisters[devnum];
	SceUInt8 timingHigh, timingLow, exposureCeiling;
	int result;

	if (cameraSensorInfo[devnum].useExtendedTiming != 1) {
		switch (framerate) {
		case 3:
			{
				const CameraSensorRegister *registers = cameraFramerate3Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		case 5:
			{
				const CameraSensorRegister *registers = cameraFramerate5Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		case 7:
			{
				const CameraSensorRegister *registers = cameraFramerate7Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		case 10:
			{
				const CameraSensorRegister *registers = cameraFramerate10Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		case 15:
			{
				const CameraSensorRegister *registers = cameraFramerate15Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		case 20:
			{
				const CameraSensorRegister *registers = cameraFramerate20Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		case 24:
			{
				const CameraSensorRegister *registers = cameraFramerate24Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		case 25:
			{
				const CameraSensorRegister *registers = cameraFramerate25Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		case 30:
			{
				const CameraSensorRegister *registers = cameraFramerate30Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		case 60:
			{
				const CameraSensorRegister *registers = cameraFramerate60Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		case 120:
			if ((unsigned int)(resolution - 2) > 3 ||
				((1 << (resolution - 2)) & 0xb) == 0)
				return 0x802e0009;
			{
				const CameraSensorRegister *registers = cameraFramerate120Registers.registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;

				exposureRead[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						SceUInt8 mask = registers[-1].mask;
						unsigned int address = registers[-1].address;

						if (mask == 0xff) {
							exposureRead[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &exposureRead[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							exposureRead[0] ^= 6;
						else if (address == 0x3820)
							exposureRead[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = exposureRead[0];
						if (index != 15 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[1] = firstAddress;
									buffer[0] = firstAddress >> 8;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 15)
							break;
						length = 0;
						registers++;
					}
				} else {
					result = 0;
				}
			}
			return result;
		default:
			return 0x802e0009;
		}
	}
	driver = cameraDriver;
	switch (framerate) {
	case 3:
		*timing = cameraFramerate3Registers;
		timingHigh = 0;
		timingLow = 19;
		exposureCeiling = 26;
		break;
	case 5:
		*timing = cameraFramerate5Registers;
		timingHigh = 0;
		timingLow = 25;
		exposureCeiling = 20;
		break;
	case 7:
		*timing = cameraFramerate7Registers;
		timingHigh = 0;
		timingLow = 38;
		exposureCeiling = 13;
		break;
	case 10:
		*timing = cameraFramerate10Registers;
		timingHigh = 0;
		timingLow = 50;
		exposureCeiling = 10;
		break;
	case 15:
		*timing = cameraFramerate15Registers;
		timingHigh = 0;
		timingLow = 76;
		exposureCeiling = 6;
		break;
	case 20:
		*timing = cameraFramerate20Registers;
		timingHigh = 0;
		timingLow = 101;
		exposureCeiling = 5;
		break;
	case 24:
		*timing = cameraFramerate24Registers;
		timingHigh = 0;
		timingLow = 121;
		exposureCeiling = 4;
		break;
	case 25:
		*timing = cameraFramerate25Registers;
		timingHigh = 0;
		timingLow = 126;
		exposureCeiling = 4;
		break;
	case 30:
		*timing = cameraFramerate30Registers;
		timingHigh = 0;
		timingLow = 152;
		exposureCeiling = 3;
		break;
	case 60:
		*timing = cameraFramerate60Registers;
		timingHigh = 1;
		timingLow = 48;
		exposureCeiling = 1;
		break;
	case 120:
		if ((unsigned int)(resolution - 2) > 3 ||
			((1 << (resolution - 2)) & 0xb) == 0)
			return 0x802e0009;
		*timing = cameraFramerate120Registers;
		timingHigh = 1;
		timingLow = 65;
		exposureCeiling = 1;
		break;
	default:
		return 0x802e0009;
	}
	timing->registers[6].value = timingHigh;
	timing->registers[7].value = timingLow;
	timing->registers[8].value = timingHigh;
	timing->registers[9].value = timingLow;
	timing->registers[10].value = exposureCeiling;
	timing->registers[11].value = exposureCeiling;
	{
		const CameraSensorRegister *registers = timing->registers + 1;
		SceUInt16 firstAddress = 0;
		unsigned int length = 0;
		int index = 0;

		timingRead[0] = 0;
		memset(values, 0, sizeof(values));
		if (driver->i2cEnabled && driver->clockEnabled) {
			for (;;) {
				SceUInt8 mask = registers[-1].mask;
				unsigned int address = registers[-1].address;

				if (mask == 0xff) {
					timingRead[0] = registers[-1].value;
				} else {
					if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
						buffer[1] = address;
						buffer[0] = address >> 8;
						result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
							0x74, &timingRead[0], 1);
						if (result < 0)
							return result;
						mask = registers[-1].mask;
						address = registers[-1].address;
					}
					timingRead[0] = (timingRead[0] & ~mask) | (mask & registers[-1].value);
				}
				if (devnum == 1 && address == 0x3821)
					timingRead[0] ^= 6;
				else if (address == 0x3820)
					timingRead[0] ^= 6;
				index++;
				if (length == 0)
					firstAddress = address;
				values[length++] = timingRead[0];
				if (index != 15 && address + 1 == registers[0].address && length <= 59) {
					registers++;
					continue;
				}
				if ((SceUInt8)(length - 1) > 59) {
					result = (int)0x802e0000;
				} else {
					result = cameraDriver->i2cEnabled;
					if (result) {
						result = cameraDriver->clockEnabled;
						if (result) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						}
					}
				}
				memset(values, 0, sizeof(values));
				if (index == 15)
					break;
				length = 0;
				registers++;
			}
		} else {
			result = 0;
		}
	}
	if (result < 0)
		return result;
	result = cameraLockDevice(devnum);
	if (result < 0)
		return result;
	antiflicker[2].value = timingHigh;
	antiflicker[3].value = timingLow;
	antiflicker[4].value = timingHigh;
	antiflicker[5].value = timingLow;
	antiflicker[7].value = exposureCeiling;
	antiflicker[6].value = exposureCeiling;
	cameraExposureGroupRegisters[devnum][1].value =
		cameraSavedSensorState[devnum].exposureControl.value;
	{
		const CameraSensorRegister *registers = antiflicker + 1;
		SceUInt16 firstAddress = 0;
		unsigned int length = 0;
		int index = 0;

		antiflickerRead[0] = 0;
		memset(values, 0, sizeof(values));
		if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
			for (;;) {
				SceUInt8 mask = registers[-1].mask;
				unsigned int address = registers[-1].address;

				if (mask == 0xff) {
					antiflickerRead[0] = registers[-1].value;
				} else {
					if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
						buffer[1] = address;
						buffer[0] = address >> 8;
						result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
							0x74, &antiflickerRead[0], 1);
						if (result < 0)
							goto unlockDevice;
						mask = registers[-1].mask;
						address = registers[-1].address;
					}
					antiflickerRead[0] = (antiflickerRead[0] & ~mask) | (mask & registers[-1].value);
				}
				if (devnum == 1 && address == 0x3821)
					antiflickerRead[0] ^= 6;
				else if (address == 0x3820)
					antiflickerRead[0] ^= 6;
				index++;
				if (length == 0)
					firstAddress = address;
				values[length++] = antiflickerRead[0];
				if (index != 10 && address + 1 == registers[0].address && length <= 59) {
					registers++;
					continue;
				}
				if ((SceUInt8)(length - 1) > 59) {
					result = (int)0x802e0000;
				} else {
					result = cameraDriver->i2cEnabled;
					if (result) {
						result = cameraDriver->clockEnabled;
						if (result) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						}
					}
				}
				memset(values, 0, sizeof(values));
				if (index == 10)
					break;
				length = 0;
				registers++;
			}
		} else {
			result = 0;
		}
	}
	if (result < 0)
		goto unlockDevice;
	{
		const CameraSensorRegister *registers = cameraExposureGroupRegisters[devnum] + 1;
		SceUInt16 firstAddress = 0;
		unsigned int length = 0;
		int index = 0;
		int i2cEnabled;

		driver = cameraDriver;
		i2cEnabled = driver->i2cEnabled;
		exposureRead[0] = 0;
		memset(values, 0, sizeof(values));
		if (i2cEnabled && driver->clockEnabled) {
			for (;;) {
				SceUInt8 mask = registers[-1].mask;
				unsigned int address = registers[-1].address;

				if (mask == 0xff) {
					exposureRead[0] = registers[-1].value;
				} else {
					if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
						buffer[1] = address;
						buffer[0] = address >> 8;
						result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
							0x74, &exposureRead[0], 1);
						if (result < 0)
							goto unlockDevice;
						mask = registers[-1].mask;
						address = registers[-1].address;
					}
					exposureRead[0] = (exposureRead[0] & ~mask) | (mask & registers[-1].value);
				}
				if (devnum == 1 && address == 0x3821)
					exposureRead[0] ^= 6;
				else if (address == 0x3820)
					exposureRead[0] ^= 6;
				index++;
				if (length == 0)
					firstAddress = address;
				values[length++] = exposureRead[0];
				if (index != 4 && address + 1 == registers[0].address && length <= 59) {
					registers++;
					continue;
				}
				if ((SceUInt8)(length - 1) > 59) {
					result = (int)0x802e0000;
				} else {
					result = cameraDriver->i2cEnabled;
					if (result) {
						result = cameraDriver->clockEnabled;
						if (result) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						}
					}
				}
				memset(values, 0, sizeof(values));
				if (index == 4)
					break;
				length = 0;
				registers++;
			}
		} else {
			result = 0;
		}
	}
	if (result == 0)
		cameraAntiflickerState[devnum].applied = 1;
unlockDevice:
	cameraUnlockDevice(devnum);
	return result;
}

int cameraSetResolutionOV7735(int devnum, int resolution, int framerate)
{
	SceUInt8 readValue[4], values[60], buffer[64];
	int result;

	switch (resolution) {
	case 1:
		{
			const CameraSensorRegister *registers = cameraResolution1Registers + 1;
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;
	
			readValue[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					unsigned int address = registers[-1].address;
					SceUInt8 mask = registers[-1].mask;
	
					if (mask == 0xff) {
						readValue[0] = registers[-1].value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[1] = address;
							buffer[0] = address >> 8;
							result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &readValue[0], 1);
							if (result < 0)
								return result;
							mask = registers[-1].mask;
							address = registers[-1].address;
						}
						readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
					}
					if (devnum == 1 && address == 0x3821)
						readValue[0] ^= 6;
					else if (address == 0x3820)
						readValue[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = readValue[0];
					if (index != 30 && address + 1 == registers[0].address && length <= 59) {
						registers++;
						continue;
					}
					if ((SceUInt8)(length - 1) > 59) {
						length = 0x802e0000;
					} else if (cameraDriver->i2cEnabled) {
						if (cameraDriver->clockEnabled) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						} else {
							length = 0;
						}
					} else {
						length = 0;
					}
					memset(values, 0, sizeof(values));
					if (index == 30)
						break;
					length = 0;
					registers++;
				}
				result = length;
			} else {
				result = 0;
			}
		}
		return result;
	case 2:
		if (framerate <= 119) {
			{
				const CameraSensorRegister *registers = cameraResolution2Registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;
		
				readValue[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						unsigned int address = registers[-1].address;
						SceUInt8 mask = registers[-1].mask;
		
						if (mask == 0xff) {
							readValue[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &readValue[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							readValue[0] ^= 6;
						else if (address == 0x3820)
							readValue[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = readValue[0];
						if (index != 30 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							length = 0x802e0000;
						} else if (cameraDriver->i2cEnabled) {
							if (cameraDriver->clockEnabled) {
								memset(buffer, 0, 62);
								buffer[1] = firstAddress;
								buffer[0] = firstAddress >> 8;
								length = (SceUInt8)length;
								if (length)
									memcpy(buffer + 2, values, length);
								length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
							} else {
								length = 0;
							}
						} else {
							length = 0;
						}
						memset(values, 0, sizeof(values));
						if (index == 30)
							break;
						length = 0;
						registers++;
					}
					result = length;
				} else {
					result = 0;
				}
			}
		} else {
			{
				const CameraSensorRegister *registers = cameraResolution2HighFramerateRegisters + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;
		
				readValue[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						unsigned int address = registers[-1].address;
						SceUInt8 mask = registers[-1].mask;
		
						if (mask == 0xff) {
							readValue[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &readValue[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							readValue[0] ^= 6;
						else if (address == 0x3820)
							readValue[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = readValue[0];
						if (index != 30 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							length = 0x802e0000;
						} else if (cameraDriver->i2cEnabled) {
							if (cameraDriver->clockEnabled) {
								memset(buffer, 0, 62);
								buffer[1] = firstAddress;
								buffer[0] = firstAddress >> 8;
								length = (SceUInt8)length;
								if (length)
									memcpy(buffer + 2, values, length);
								length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
							} else {
								length = 0;
							}
						} else {
							length = 0;
						}
						memset(values, 0, sizeof(values));
						if (index == 30)
							break;
						length = 0;
						registers++;
					}
					result = length;
				} else {
					result = 0;
				}
			}
		}
		return result;
	case 3:
		if (framerate <= 119) {
			{
				const CameraSensorRegister *registers = cameraResolution3Registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;
		
				readValue[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						unsigned int address = registers[-1].address;
						SceUInt8 mask = registers[-1].mask;
		
						if (mask == 0xff) {
							readValue[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &readValue[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							readValue[0] ^= 6;
						else if (address == 0x3820)
							readValue[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = readValue[0];
						if (index != 30 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							length = 0x802e0000;
						} else if (cameraDriver->i2cEnabled) {
							if (cameraDriver->clockEnabled) {
								memset(buffer, 0, 62);
								buffer[1] = firstAddress;
								buffer[0] = firstAddress >> 8;
								length = (SceUInt8)length;
								if (length)
									memcpy(buffer + 2, values, length);
								length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
							} else {
								length = 0;
							}
						} else {
							length = 0;
						}
						memset(values, 0, sizeof(values));
						if (index == 30)
							break;
						length = 0;
						registers++;
					}
					result = length;
				} else {
					result = 0;
				}
			}
		} else {
			{
				const CameraSensorRegister *registers = cameraResolution3HighFramerateRegisters + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;
		
				readValue[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						unsigned int address = registers[-1].address;
						SceUInt8 mask = registers[-1].mask;
		
						if (mask == 0xff) {
							readValue[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &readValue[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							readValue[0] ^= 6;
						else if (address == 0x3820)
							readValue[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = readValue[0];
						if (index != 30 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							length = 0x802e0000;
						} else if (cameraDriver->i2cEnabled) {
							if (cameraDriver->clockEnabled) {
								memset(buffer, 0, 62);
								buffer[1] = firstAddress;
								buffer[0] = firstAddress >> 8;
								length = (SceUInt8)length;
								if (length)
									memcpy(buffer + 2, values, length);
								length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
							} else {
								length = 0;
							}
						} else {
							length = 0;
						}
						memset(values, 0, sizeof(values));
						if (index == 30)
							break;
						length = 0;
						registers++;
					}
					result = length;
				} else {
					result = 0;
				}
			}
		}
		return result;
	case 4:
		{
			const CameraSensorRegister *registers = cameraResolution4Registers + 1;
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;
	
			readValue[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					unsigned int address = registers[-1].address;
					SceUInt8 mask = registers[-1].mask;
	
					if (mask == 0xff) {
						readValue[0] = registers[-1].value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[1] = address;
							buffer[0] = address >> 8;
							result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &readValue[0], 1);
							if (result < 0)
								return result;
							mask = registers[-1].mask;
							address = registers[-1].address;
						}
						readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
					}
					if (devnum == 1 && address == 0x3821)
						readValue[0] ^= 6;
					else if (address == 0x3820)
						readValue[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = readValue[0];
					if (index != 30 && address + 1 == registers[0].address && length <= 59) {
						registers++;
						continue;
					}
					if ((SceUInt8)(length - 1) > 59) {
						length = 0x802e0000;
					} else if (cameraDriver->i2cEnabled) {
						if (cameraDriver->clockEnabled) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						} else {
							length = 0;
						}
					} else {
						length = 0;
					}
					memset(values, 0, sizeof(values));
					if (index == 30)
						break;
					length = 0;
					registers++;
				}
				result = length;
			} else {
				result = 0;
			}
		}
		return result;
	case 5:
		if (framerate <= 119) {
			{
				const CameraSensorRegister *registers = cameraResolution5Registers + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;
		
				readValue[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						unsigned int address = registers[-1].address;
						SceUInt8 mask = registers[-1].mask;
		
						if (mask == 0xff) {
							readValue[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &readValue[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							readValue[0] ^= 6;
						else if (address == 0x3820)
							readValue[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = readValue[0];
						if (index != 30 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							length = 0x802e0000;
						} else if (cameraDriver->i2cEnabled) {
							if (cameraDriver->clockEnabled) {
								memset(buffer, 0, 62);
								buffer[1] = firstAddress;
								buffer[0] = firstAddress >> 8;
								length = (SceUInt8)length;
								if (length)
									memcpy(buffer + 2, values, length);
								length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
							} else {
								length = 0;
							}
						} else {
							length = 0;
						}
						memset(values, 0, sizeof(values));
						if (index == 30)
							break;
						length = 0;
						registers++;
					}
					result = length;
				} else {
					result = 0;
				}
			}
		} else {
			{
				const CameraSensorRegister *registers = cameraResolution5HighFramerateRegisters + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;
		
				readValue[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						unsigned int address = registers[-1].address;
						SceUInt8 mask = registers[-1].mask;
		
						if (mask == 0xff) {
							readValue[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &readValue[0], 1);
								if (result < 0)
									return result;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							readValue[0] ^= 6;
						else if (address == 0x3820)
							readValue[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = readValue[0];
						if (index != 30 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							length = 0x802e0000;
						} else if (cameraDriver->i2cEnabled) {
							if (cameraDriver->clockEnabled) {
								memset(buffer, 0, 62);
								buffer[1] = firstAddress;
								buffer[0] = firstAddress >> 8;
								length = (SceUInt8)length;
								if (length)
									memcpy(buffer + 2, values, length);
								length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
							} else {
								length = 0;
							}
						} else {
							length = 0;
						}
						memset(values, 0, sizeof(values));
						if (index == 30)
							break;
						length = 0;
						registers++;
					}
					result = length;
				} else {
					result = 0;
				}
			}
		}
		return result;
	case 6:
		{
			const CameraSensorRegister *registers = cameraResolution6Registers + 1;
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;
	
			readValue[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					unsigned int address = registers[-1].address;
					SceUInt8 mask = registers[-1].mask;
	
					if (mask == 0xff) {
						readValue[0] = registers[-1].value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[1] = address;
							buffer[0] = address >> 8;
							result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &readValue[0], 1);
							if (result < 0)
								return result;
							mask = registers[-1].mask;
							address = registers[-1].address;
						}
						readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
					}
					if (devnum == 1 && address == 0x3821)
						readValue[0] ^= 6;
					else if (address == 0x3820)
						readValue[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = readValue[0];
					if (index != 30 && address + 1 == registers[0].address && length <= 59) {
						registers++;
						continue;
					}
					if ((SceUInt8)(length - 1) > 59) {
						length = 0x802e0000;
					} else if (cameraDriver->i2cEnabled) {
						if (cameraDriver->clockEnabled) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						} else {
							length = 0;
						}
					} else {
						length = 0;
					}
					memset(values, 0, sizeof(values));
					if (index == 30)
						break;
					length = 0;
					registers++;
				}
				result = length;
			} else {
				result = 0;
			}
		}
		return result;
	case 8:
		{
			const CameraSensorRegister *registers = cameraResolution8Registers + 1;
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;
	
			readValue[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					unsigned int address = registers[-1].address;
					SceUInt8 mask = registers[-1].mask;
	
					if (mask == 0xff) {
						readValue[0] = registers[-1].value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[1] = address;
							buffer[0] = address >> 8;
							result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &readValue[0], 1);
							if (result < 0)
								return result;
							mask = registers[-1].mask;
							address = registers[-1].address;
						}
						readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
					}
					if (devnum == 1 && address == 0x3821)
						readValue[0] ^= 6;
					else if (address == 0x3820)
						readValue[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = readValue[0];
					if (index != 30 && address + 1 == registers[0].address && length <= 59) {
						registers++;
						continue;
					}
					if ((SceUInt8)(length - 1) > 59) {
						length = 0x802e0000;
					} else if (cameraDriver->i2cEnabled) {
						if (cameraDriver->clockEnabled) {
							memset(buffer, 0, 62);
							buffer[1] = firstAddress;
							buffer[0] = firstAddress >> 8;
							length = (SceUInt8)length;
							if (length)
								memcpy(buffer + 2, values, length);
							length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
						} else {
							length = 0;
						}
					} else {
						length = 0;
					}
					memset(values, 0, sizeof(values));
					if (index == 30)
						break;
					length = 0;
					registers++;
				}
				result = length;
			} else {
				result = 0;
			}
		}
		return result;
	default:
		return 0x802e0008;
	}
}

int cameraSetRangeOV7735(int devnum, int range)
{
	SceUInt8 readValue[4], values[60], buffer[64];
	int result;

	if (range != 0) {
		if (range != 1)
			return 0x802e0011;
		{
			const CameraSensorRegister *registers = cameraBt601RangeRegisters + 1;
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;

			readValue[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					SceUInt8 mask = registers[-1].mask;
					unsigned int address = registers[-1].address;

					if (mask == 0xff) {
						readValue[0] = registers[-1].value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[1] = address;
							buffer[0] = address >> 8;
							result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &readValue[0], 1);
							if (result < 0)
								return result;
							mask = registers[-1].mask;
							address = registers[-1].address;
						}
						readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
					}
					if (devnum == 1 && address == 0x3821)
						readValue[0] ^= 6;
					else if (address == 0x3820)
						readValue[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = readValue[0];
					if (index != 20 && address + 1 == registers[0].address && length <= 59) {
						registers++;
						continue;
					}
					if ((SceUInt8)(length - 1) > 59) {
						result = (int)0x802e0000;
					} else {
						result = cameraDriver->i2cEnabled;
						if (result) {
							result = cameraDriver->clockEnabled;
							if (result) {
								memset(buffer, 0, 62);
								buffer[1] = firstAddress;
								buffer[0] = firstAddress >> 8;
								length = (SceUInt8)length;
								if (length)
									memcpy(buffer + 2, values, length);
								result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
							}
						}
					}
					memset(values, 0, sizeof(values));
					if (index == 20)
						break;
					length = 0;
					registers++;
				}
			} else {
				result = 0;
			}
		}
	} else {
		{
			const CameraSensorRegister *registers = cameraFullRangeRegisters + 1;
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;

			readValue[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					SceUInt8 mask = registers[-1].mask;
					unsigned int address = registers[-1].address;

					if (mask == 0xff) {
						readValue[0] = registers[-1].value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[1] = address;
							buffer[0] = address >> 8;
							result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &readValue[0], 1);
							if (result < 0)
								return result;
							mask = registers[-1].mask;
							address = registers[-1].address;
						}
						readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
					}
					if (devnum == 1 && address == 0x3821)
						readValue[0] ^= 6;
					else if (address == 0x3820)
						readValue[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = readValue[0];
					if (index != 20 && address + 1 == registers[0].address && length <= 59) {
						registers++;
						continue;
					}
					if ((SceUInt8)(length - 1) > 59) {
						result = (int)0x802e0000;
					} else {
						result = cameraDriver->i2cEnabled;
						if (result) {
							result = cameraDriver->clockEnabled;
							if (result) {
								memset(buffer, 0, 62);
								buffer[1] = firstAddress;
								buffer[0] = firstAddress >> 8;
								length = (SceUInt8)length;
								if (length)
									memcpy(buffer + 2, values, length);
								result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
							}
						}
					}
					memset(values, 0, sizeof(values));
					if (index == 20)
						break;
					length = 0;
					registers++;
				}
			} else {
				result = 0;
			}
		}
	}
	return result;
}

int cameraSetPowerStateOV7735(int devnum, int enabled)
{
	SceUInt8 readValue[4], values[60], buffer[64];
	int port = devnum ? 11 : 10;
	int result;

	result = cameraLockDevice(devnum);
	if (result < 0)
		return result;
	if (enabled == 0) {
		if (cameraHasStartedProcess(devnum, 0)) {
			{
				const CameraSensorRegister *registers = cameraSensorPowerOffRegisters + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;
	
				readValue[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						unsigned int address = registers[-1].address;
						SceUInt8 mask = registers[-1].mask;
	
						if (mask == 0xff) {
							readValue[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &readValue[0], 1);
								if (result < 0)
									goto unlockDevice;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							readValue[0] ^= 6;
						else if (address == 0x3820)
							readValue[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = readValue[0];
						if (index == 1 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							length = 0x802e0000;
						} else if (cameraDriver->i2cEnabled) {
							if (cameraDriver->clockEnabled) {
								memset(buffer, 0, 62);
								buffer[1] = firstAddress;
								buffer[0] = firstAddress >> 8;
								length = (SceUInt8)length;
								if (length)
									memcpy(buffer + 2, values, length);
								length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
							} else {
								length = 0;
							}
						} else {
							length = 0;
						}
						memset(values, 0, sizeof(values));
						if (index == 2)
							break;
						length = 0;
						registers++;
					}
					result = length;
				} else {
					result = 0;
				}
			}
			if (result < 0)
				goto unlockDevice;
		}
		cameraDriver->i2cEnabled = 0;
		result = SceGpioForDriver_D454A584(0, port);
	} else if (enabled == 1) {
		result = SceGpioForDriver_F6310435(0, port);
		sceKernelDelayThread(2000);
		cameraDriver->i2cEnabled = 1;
		if (result >= 0 && cameraHasStartedProcess(devnum, 0)) {
			{
				const CameraSensorRegister *registers = cameraSensorPowerOnRegisters + 1;
				SceUInt16 firstAddress = 0;
				unsigned int length = 0;
				int index = 0;
	
				readValue[0] = 0;
				memset(values, 0, sizeof(values));
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					for (;;) {
						unsigned int address = registers[-1].address;
						SceUInt8 mask = registers[-1].mask;
	
						if (mask == 0xff) {
							readValue[0] = registers[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[1] = address;
								buffer[0] = address >> 8;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &readValue[0], 1);
								if (result < 0)
									goto unlockDevice;
								mask = registers[-1].mask;
								address = registers[-1].address;
							}
							readValue[0] = (readValue[0] & ~mask) | (mask & registers[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							readValue[0] ^= 6;
						else if (address == 0x3820)
							readValue[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = readValue[0];
						if (index == 1 && address + 1 == registers[0].address && length <= 59) {
							registers++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							length = 0x802e0000;
						} else if (cameraDriver->i2cEnabled) {
							if (cameraDriver->clockEnabled) {
								memset(buffer, 0, 62);
								buffer[1] = firstAddress;
								buffer[0] = firstAddress >> 8;
								length = (SceUInt8)length;
								if (length)
									memcpy(buffer + 2, values, length);
								length = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
							} else {
								length = 0;
							}
						} else {
							length = 0;
						}
						memset(values, 0, sizeof(values));
						if (index == 2)
							break;
						length = 0;
						registers++;
					}
					result = length;
				} else {
					result = 0;
				}
			}
		}
	} else {
		result = 0x802e0000;
	}
unlockDevice:
	cameraUnlockDevice(devnum);
	return result;
}

int cameraGetAttributeOV7735(int devnum, int attribute, int *value, int flags,
			   void *option)
{
	CameraConfig *config = &cameraDriver->device[devnum].config;
	CameraAttributeOption *attributeOption = option;
	SceUInt8 sign[8], brightness[8], buffer[4];
	int range = attributeOption->wRange;
	int result;

	(void)flags;
	sign[0] = 0;
	brightness[0] = 0;
	switch (attribute) {
	case 11:
		result = cameraDriver->i2cEnabled;
		if (result) {
			if (cameraDriver->clockEnabled) {
				int bus = devnum == 0;

				buffer[0] = 0x55;
				buffer[1] = 0x87;
				result = SceI2cForDriver_0A40B7BF(bus, 0x74, buffer, 2,
								0x74, brightness, 1);
				if (result < 0)
					return result;
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					buffer[0] = 0x55;
					buffer[1] = 0x88;
					result = SceI2cForDriver_0A40B7BF(bus, 0x74, buffer, 2,
									0x74, sign, 1);
					if (result < 0)
						return result;
				} else {
					result = 0;
				}
			} else {
				result = 0;
			}
		}
		if (sign[0] & 8)
			brightness[0] = -brightness[0];
		if (value)
			*value = brightness[0] + 128 - (range == 1 ? 16 : 0);
		return result;
	case 12:
		result = cameraDriver->i2cEnabled;
		if (result) {
			result = cameraDriver->clockEnabled;
			if (result) {
				buffer[0] = 0x55;
				buffer[1] = 0x86;
				result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, brightness, 1);
				if (result < 0)
					return result;
			}
		}
		if (value)
			*value = brightness[0];
		return result;
	case 10:
		if (value)
			*value = config->saturation;
		return 0;
	case 13:
		if (value)
			*value = config->sharpness;
		return 0;
	case 14:
		if (value)
			*value = config->reverse;
		return 0;
	case 15:
		if (value)
			*value = config->effect;
		return 0;
	case 16:
		if (value)
			*value = config->ev;
		return 0;
	case 17:
		if (value)
			*value = config->wZoom;
		return 0;
	case 18:
		if (value)
			*value = config->antiflicker;
		return 0;
	case 20:
		if (value)
			*value = config->iso;
		return 0;
	case 22:
		if (value)
			*value = config->whiteBalance;
		return 0;
	case 23:
		if (value)
			*value = config->backlight;
		return 0;
	case 24:
		if (value)
			*value = config->nightMode;
		return 0;
	case 25:
		if (value)
			*value = config->noiseReduction;
		return 0;
	case 26:
		if (value)
			*value = config->autoControlHold;
		return 0;
	case 27:
		if (value)
			*value = config->exposureCeiling;
		return 0;
	case 28:
		if (value)
			*value = config->imageQuality;
		return 0;
	default:
		return 0x802e000c;
	}
}

int cameraGetCsiFormatOV7735(int devnum, int framerate, int format)
{
	(void)devnum;
	if (framerate > 30) {
		if (format == 6)
			return 20;
		return 17;
	}
	if (format == 6)
		return 25;
	return 20;
}

int sceCameraGetExposureTimeOV7735(int devnum, int framerate, unsigned int *exposure)
{
	SceUInt8 registers[3], buffer[8];
	float lineTime;
	unsigned int exposureTime, exposureGap;
	int result, index;

	registers[0] = 0;
	registers[1] = 0;
	registers[2] = 0;
	switch (framerate) {
	case 3:
		lineTime = 5255440.0f;
		break;
	case 5:
		lineTime = 3941580.0f;
		break;
	case 7:
		lineTime = 2627720.0f;
		break;
	case 10:
		lineTime = 1970790.0f;
		break;
	case 15:
		lineTime = 1313860.0f;
		break;
	case 20:
		lineTime = 985395.0f;
		break;
	case 24:
		lineTime = 821162.5f;
		break;
	case 25:
		lineTime = 788316.0f;
		break;
	case 30:
		lineTime = 656930.0f;
		break;
	case 120:
		lineTime = 164232.5f;
		break;
	default:
		lineTime = 328465.0f;
		break;
	}
	result = cameraDriver->i2cEnabled;
	if (result)
		result = cameraDriver->clockEnabled;
	if (result) {
		buffer[0] = 0x35;
		buffer[1] = 0;
		result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
						0x74, registers, 3);
		if (result < 0)
			return result;
		lineTime *= (float)((registers[2] >> 4) +
			16 * (registers[1] + (registers[0] << 8)));
		exposureTime = (unsigned int)(lineTime / 10000.0f);
	} else {
		exposureTime = result;
	}
	exposure[0] = exposureTime;
	if (framerate == 3)
		index = 0;
	else if (framerate == 5)
		index = 1;
	else if (framerate == 7)
		index = 2;
	else if (framerate == 10)
		index = 3;
	else if (framerate == 15)
		index = 4;
	else if (framerate == 20)
		index = 5;
	else if (framerate == 24)
		index = 6;
	else if (framerate == 25)
		index = 7;
	else if (framerate == 30)
		index = 8;
	else if (framerate == 60)
		index = 9;
	else if (framerate == 120)
		index = 10;
	else {
		exposureGap = 0;
		goto setExposureGap;
	}
	exposureGap = cameraExposureGaps[index].dwExposureTimeGap;
setExposureGap:
	exposure[1] = exposureGap;
	return result;
}

int cameraGetRaw8FormatOV7735(int devnum, int format, unsigned int *value)
{
	(void)devnum;
	(void)format;
	*value = 1;
	return 0;
}

int cameraHasColorChangeOV7735(int devnum)
{
	CameraSensorInfo *info = &cameraSensorInfo[devnum];
	CameraConfig *config;
	SceUInt8 readValue[8], buffer[4];
	int detected, changed, result;

	readValue[0] = 0;
	if (info->useExtendedTiming != 1)
		return info->revision != 1;
	config = &cameraDriver->device[devnum].config;
	if (config->antiflicker != 0)
		return 0;
	if (config->autoControlHold == 1)
		return 0;
	if (cameraDriver->i2cEnabled) {
		detected = cameraDriver->clockEnabled;
		if (detected) {
			buffer[1] = 0x0c;
			buffer[0] = 0x3c;
			result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
							0x74, readValue, 1);
			if (result < 0)
				return result;
			detected = readValue[0] & 1;
		}
	} else {
		detected = 0;
	}
	result = cameraLockDevice(devnum);
	if (result < 0)
		return result;
	if (cameraAntiflickerState[devnum].applied == detected) {
		changed = 0;
	} else {
		cameraAntiflickerState[devnum].detected = detected;
		changed = 1;
	}
	cameraUnlockDevice(devnum);
	return changed;
}

int cameraSetAttributeOV7735(int devnum, int attribute, int value, int flags,
		void *argument)
{
	CameraAttributeOption *option = argument;
	CameraDevice *device = &cameraDriver->device[devnum];
	CameraProcess *process = option->process;
	CameraConfig *config;
	CameraSavedSensorState *state = &cameraSavedSensorState[devnum];
	const CameraSensorRegister *registers;
	CameraSensorRegister *mutableRegisters;
	CameraSensorRegister savedWhiteBalance[11];
	CameraSensorRegister savedExposure[4];
	CameraSensorRegister savedGain[3];
	SceUInt32 frameTimeout;
	unsigned char readValue = 0, controlValue = 0, writeValue;
	int updateDevice = option->updateDevice;
	int deferRegisterUpdate = option->deferRegisterUpdate;
	int range = option->wRange;
	int count, result = 0;

	(void)flags;
	switch (attribute) {
	case 18:
		switch (value) {
		case 0:
			registers = cameraAntiflickerAutoRegisters;
			break;
		case 1:
			registers = cameraSensorInfo[devnum].useExtendedTiming == 1 ?
				cameraAntiflicker50ExtendedRegisters : cameraAntiflicker50Registers;
			break;
		case 2:
			registers = cameraSensorInfo[devnum].useExtendedTiming == 1 ?
				cameraAntiflicker60ExtendedRegisters : cameraAntiflicker60Registers;
			break;
		default:
			return (int)0x802e0000;
		}
		if (!updateDevice)
			return 0;
		result = cameraWriteSensorRegisters(devnum, registers, 2);
		if (result < 0)
			return result;
		device->config.antiflicker = value;
		if (cameraSensorInfo[devnum].useExtendedTiming == 1 &&
		    (value != 0 || (result = cameraHasColorChangeOV7735(devnum)) > 0)) {
			result = cameraLockDevice(devnum);
			if (result >= 0) {
				result = cameraSetAntiflickerOV7735(devnum, value);
				cameraUnlockDevice(devnum);
			}
		}
		return result;
	case 25:
		config = updateDevice ? &device->config : &process->config;
		if (config->imageQuality != 0)
			return (int)0x802e0000;
		switch (value) {
		case 0: registers = cameraNoise0Registers; break;
		case 1: registers = cameraNoise1Registers; break;
		case 2: registers = cameraNoise2Registers; break;
		case 4: registers = cameraNoise4Registers; break;
		default: return (int)0x802e0000;
		}
		if (!updateDevice)
			return 0;
		result = cameraWriteSensorRegisters(devnum, registers, 2);
		if (result >= 0)
			device->config.noiseReduction = value;
		return result;
	case 22:
		config = updateDevice ? &device->config : &process->config;
		if (config->imageQuality != 0)
			return (int)0x802e0000;
		switch (value) {
		case 0: registers = cameraWhiteBalanceAutoRegisters; count = 5; break;
		case 1: registers = cameraWhiteBalance1Registers; count = 11; break;
		case 2: registers = cameraWhiteBalance2Registers; count = 11; break;
		case 3: registers = cameraWhiteBalance3Registers; count = 11; break;
		default: return (int)0x802e0000;
		}
		if (updateDevice) {
			result = cameraWriteSensorRegisters(devnum, registers, count);
			if (result < 0)
				return result;
			device->config.whiteBalance = value;
		} else if (value == 0) {
			savedWhiteBalance[0] = registers[0];
			savedWhiteBalance[1] = registers[1];
			savedWhiteBalance[2] = (CameraSensorRegister){0x5195, 4, 0xff};
			savedWhiteBalance[3] = (CameraSensorRegister){0x5196, 0, 0xff};
			savedWhiteBalance[4] = (CameraSensorRegister){0x5197, 4, 0xff};
			savedWhiteBalance[5] = (CameraSensorRegister){0x5198, 0, 0xff};
			savedWhiteBalance[6] = (CameraSensorRegister){0x5199, 4, 0xff};
			savedWhiteBalance[7] = (CameraSensorRegister){0x519a, 0, 0xff};
			savedWhiteBalance[8] = registers[2];
			savedWhiteBalance[9] = registers[3];
			savedWhiteBalance[10] = registers[4];
		} else {
			memcpy(savedWhiteBalance, registers, sizeof(savedWhiteBalance));
		}
		if (!deferRegisterUpdate)
			result = cameraSaveSensorAttribute(devnum, 3, updateDevice,
				process, savedWhiteBalance);
		return result;
	case 23:
		if (value == 0)
			registers = cameraBacklightOffRegisters;
		else if (value == 1)
			registers = cameraBacklightOnRegisters;
		else
			return (int)0x802e0000;
		if (!updateDevice)
			return 0;
		result = cameraWriteSensorRegisters(devnum, registers, 8);
		if (result >= 0)
			device->config.backlight = value;
		return result;
	case 24: {
		CameraDriver *driver = cameraDriver;

		switch (value) {
		case 0: registers = cameraNight0Registers; count = 1; break;
		case 1: registers = cameraNight1Registers; count = 7; break;
		case 2: registers = cameraNight2Registers; count = 7; break;
		case 3: registers = cameraNight3Registers; count = 7; break;
		default: return (int)0x802e0000;
		}
		if (!updateDevice)
			return 0;
		if (value == 2) {
			const CameraSensorRegister *night = cameraNight2Registers + 1;
			SceUInt8 nightValue[4], values[60], buffer[64];
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;

			nightValue[0] = 0;
			memset(values, 0, sizeof(values));
			result = driver->i2cEnabled;
			if (result) {
				result = driver->clockEnabled;
				if (result) {
					for (;;) {
						SceUInt8 mask = night[-1].mask;
						unsigned int address = night[-1].address;

						if (mask == 0xff) {
							nightValue[0] = night[-1].value;
						} else {
							if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
								buffer[0] = address >> 8;
								buffer[1] = address;
								result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
									0x74, &nightValue[0], 1);
								if (result < 0)
									return result;
								mask = night[-1].mask;
								address = night[-1].address;
							}
							nightValue[0] = (nightValue[0] & ~mask) | (mask & night[-1].value);
						}
						if (devnum == 1 && address == 0x3821)
							nightValue[0] ^= 6;
						else if (address == 0x3820)
							nightValue[0] ^= 6;
						index++;
						if (length == 0)
							firstAddress = address;
						values[length++] = nightValue[0];
						if (index != 7 && address + 1 == night[0].address && length <= 59) {
							night++;
							continue;
						}
						if ((SceUInt8)(length - 1) > 59) {
							result = (int)0x802e0000;
						} else {
							result = cameraDriver->i2cEnabled;
							if (result) {
								result = cameraDriver->clockEnabled;
								if (result) {
									memset(buffer, 0, 62);
									buffer[0] = firstAddress >> 8;
									buffer[1] = firstAddress;
									length = (SceUInt8)length;
									if (length)
										memcpy(buffer + 2, values, length);
									result = SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
								}
							}
						}
						memset(values, 0, sizeof(values));
						if (index == 7)
							break;
						length = 0;
						night++;
					}
				}
			}
		} else {
			result = cameraWriteSensorRegisters(devnum, registers, count);
		}
		if (result >= 0)
			driver->device[devnum].config.nightMode = value;
		return result;
	}
	case 20:
		frameTimeout = device->config.frameTimeout;
		result = cameraLockDevice(devnum);
		if (result < 0)
			return result;
		config = updateDevice ? &device->config : &process->config;
		if (config->imageQuality == 4)
			goto invalidLocked;
		switch (value) {
		case 1: registers = cameraIsoAutoRegisters; break;
		case 100: registers = cameraIso100Registers; break;
		case 200: registers = cameraIso200Registers; break;
		case 400: registers = cameraIso400Registers; break;
		case 800: registers = cameraIso800Registers; break;
		default: goto invalidLocked;
		}
		if (updateDevice) {
			if (value == 1) {
				result = cameraWriteSensorRegisters(devnum, registers, 4);
			} else {
				cameraWriteSensorRegisters(devnum, registers, 6);
				if (frameTimeout)
					sceKernelDelayThread(frameTimeout);
				result = cameraWriteSensorRegisters(devnum, cameraIsoApplyRegisters, 4);
			}
			if (result < 0)
				goto unlock;
			if (frameTimeout)
				sceKernelDelayThread(frameTimeout);
			device->config.iso = value;
		} else {
			savedGain[0] = (CameraSensorRegister){0x3503, 0, 0xff};
			savedGain[1] = (CameraSensorRegister){0x350a, 0, value == 1 ? 0 : 3};
			savedGain[2] = (CameraSensorRegister){0x350b,
				value == 1 ? 0x10 : registers[2].value, value == 1 ? 0 : 0xff};
		}
		if (!deferRegisterUpdate)
			result = cameraSaveSensorAttribute(devnum, 2, updateDevice,
				process, savedGain);
		goto unlock;
	case 26:
		result = cameraLockDevice(devnum);
		if (result < 0)
			return result;
		if ((unsigned int)value > 1)
			goto invalidLocked;
		if (updateDevice) {
			registers = value ? cameraAutoControlHoldRegisters : cameraAutoControlOffRegisters;
			result = cameraWriteSensorRegisters(devnum, registers, 4);
			if (result < 0)
				goto unlock;
			device->config.autoControlHold = value;
		} else {
			savedGain[0] = (CameraSensorRegister){0x3503, value ? 3 : 0, 3};
			savedGain[1] = (CameraSensorRegister){0x350a, 0, 0};
			savedGain[2] = (CameraSensorRegister){0x350b, 0x10, 0};
			savedExposure[0] = (CameraSensorRegister){0x3500, 0, 0};
			savedExposure[1] = (CameraSensorRegister){0x3501, 1, 0};
			savedExposure[2] = (CameraSensorRegister){0x3502, 0x30, 0};
			savedExposure[3] = (CameraSensorRegister){0x3503, value ? 3 : 0, 3};
			savedWhiteBalance[0] = (CameraSensorRegister){0x5001, value ? 0 : 1, 1};
			savedWhiteBalance[1] = (CameraSensorRegister){0x5180, value ? 2 : 0, 2};
			savedWhiteBalance[2] = (CameraSensorRegister){0x5195, 4, 0xff};
			savedWhiteBalance[3] = (CameraSensorRegister){0x5196, 0, 0xff};
			savedWhiteBalance[4] = (CameraSensorRegister){0x5197, 4, 0xff};
			savedWhiteBalance[5] = (CameraSensorRegister){0x5198, 0, 0xff};
			savedWhiteBalance[6] = (CameraSensorRegister){0x5199, 4, 0xff};
			savedWhiteBalance[7] = (CameraSensorRegister){0x519a, 0, 0xff};
			savedWhiteBalance[8] = (CameraSensorRegister){0x5806, 0x1c, 0};
			savedWhiteBalance[9] = (CameraSensorRegister){0x5807, 0x21, 0};
			savedWhiteBalance[10] = (CameraSensorRegister){0x5808, 0x2e, 0};
		}
		if (!deferRegisterUpdate) {
			cameraSaveSensorAttribute(devnum, 2, updateDevice, process, savedGain);
			cameraSaveSensorAttribute(devnum, 1, updateDevice, process, savedExposure);
			result = cameraSaveSensorAttribute(devnum, 3, updateDevice,
				process, savedWhiteBalance);
		}
		goto unlock;
	case 27:
		result = cameraLockDevice(devnum);
		if (result < 0)
			return result;
		config = updateDevice ? &device->config : &process->config;
		if (config->wFramerate != 60 || (unsigned int)value > 1)
			goto invalidLocked;
		if (!updateDevice)
			goto unlock;
		if (value == 1 || device->config.antiflicker == 1 ||
		    (device->config.antiflicker == 0 && cameraAntiflickerState[devnum].applied == 1))
			registers = cameraExposureCeiling50Registers;
		else if (device->config.antiflicker == 2 || device->config.antiflicker == 0)
			registers = cameraExposureCeiling60Registers;
		else
			goto storeExposureCeiling;
		result = cameraWriteSensorRegisters(devnum, registers, 2);
		if (result < 0)
			goto unlock;
storeExposureCeiling:
		device->config.exposureCeiling = value;
		goto unlock;
	case 28:
		result = cameraLockDevice(devnum);
		if (result < 0)
			return result;
		config = updateDevice ? &device->config : &process->config;
		if (config->sharpness != 1 || config->noiseReduction != 1 ||
		    config->saturation != 10 || config->effect != 0 || config->backlight != 0 ||
		    config->brightness != 128 || config->contrast != 32 ||
		    (config->iso != 1 && value == 4))
			goto invalidLocked;
		switch (value) {
		case 0: registers = cameraImageQuality0Registers; count = 38; break;
		case 1: registers = cameraImageQuality1Registers; count = 38; break;
		case 2: registers = cameraImageQuality2Registers; count = 38; break;
		case 3: registers = cameraImageQuality3Registers; count = 38; break;
		case 4: registers = cameraImageQuality4Registers; count = 40; break;
		default: goto invalidLocked;
		}
		if (updateDevice) {
			result = cameraWriteSensorRegisters(devnum, registers, count);
			if (result < 0)
				goto unlock;
			device->config.imageQuality = value;
			if (!deferRegisterUpdate) {
				if (cameraReadSensorByte(devnum, 0x5583, &readValue) >= 0)
					state->colorAdjustment[1].value = readValue;
				result = cameraReadSensorByte(devnum, 0x5584, &readValue);
				if (result >= 0)
					state->colorAdjustment[2].value = readValue;
			}
		} else if (!deferRegisterUpdate) {
			process->sensorRegisterValues[1] = cameraSaturation10Registers[4].value;
			process->sensorRegisterValues[2] = value == 1 || value == 2 ? 68 : 32;
		}
		goto unlock;
	case 240:
		{
			const CameraSensorRegister *restore = state->registers + 1;
			SceUInt8 restoreValue[4], values[60], buffer[64];
			SceUInt16 firstAddress = 0;
			unsigned int length = 0;
			int index = 0;

			restoreValue[0] = 0;
			memset(values, 0, sizeof(values));
			if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
				for (;;) {
					SceUInt8 mask = restore[-1].mask;
					unsigned int address = restore[-1].address;

					if (mask == 0xff) {
						restoreValue[0] = restore[-1].value;
					} else {
						if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
							buffer[0] = address >> 8;
							buffer[1] = address;
							if (SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
								0x74, &restoreValue[0], 1) < 0)
								goto restoreExposure;
							mask = restore[-1].mask;
							address = restore[-1].address;
						}
						restoreValue[0] = (restoreValue[0] & ~mask) |
							(mask & restore[-1].value);
					}
					if (devnum == 1 && address == 0x3821)
						restoreValue[0] ^= 6;
					else if (address == 0x3820)
						restoreValue[0] ^= 6;
					index++;
					if (length == 0)
						firstAddress = address;
					values[length++] = restoreValue[0];
					if (index != 4 && address + 1 == restore[0].address && length <= 59) {
						restore++;
						continue;
					}
					if ((SceUInt8)(length - 1) <= 59 &&
					    cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
						memset(buffer, 0, 62);
						buffer[0] = firstAddress >> 8;
						buffer[1] = firstAddress;
						length = (SceUInt8)length;
						if (length)
							memcpy(buffer + 2, values, length);
						SceI2cForDriver_CA94A759(devnum == 0, 0x74, buffer, length + 2);
					}
					memset(values, 0, sizeof(values));
					if (index == 4)
						break;
					length = 0;
					restore++;
				}
			}
		}
restoreExposure:
		if (state->exposureControl.value & 1)
			cameraWriteSensorRegisters(devnum, state->exposure, 3);
		if (state->exposureControl.value & 2)
			cameraWriteSensorRegisters(devnum, state->gain, 2);
		cameraWriteSensorRegisters(devnum, state->whiteBalanceControl, 2);
		if (!(state->whiteBalanceControl[0].value & 1) &&
		    (state->whiteBalanceControl[1].value & 2))
			cameraWriteSensorRegisters(devnum, state->whiteBalance, 6);
		return cameraWriteSensorRegisters(devnum, state->whiteBalanceTail, 3);
	case 13:
		config = updateDevice ? &device->config : &process->config;
		if (config->imageQuality != 0)
			return (int)0x802e0000;
		switch (value) {
		case 1: registers = cameraSharpness1Registers; count = 5; break;
		case 2: registers = cameraSharpness2Registers; count = 5; break;
		case 3: registers = cameraSharpness3Registers; count = 5; break;
		case 4: registers = cameraSharpness4Registers; count = 5; break;
		case 61440: registers = cameraSharpness61440Registers; count = 2; break;
		default: return (int)0x802e0000;
		}
		if (!updateDevice)
			return 0;
		result = cameraWriteSensorRegisters(devnum, registers, count);
		if (result >= 0)
			device->config.sharpness = value;
		return result;
	case 15:
		result = cameraLockDevice(devnum);
		if (result < 0)
			return result;
		config = updateDevice ? &device->config : &process->config;
		if (config->imageQuality != 0)
			goto invalidLocked;
		switch (value) {
		case 0: mutableRegisters = cameraEffect0Registers; count = 6; writeValue = range == 1 ? 6 : 2; break;
		case 1: mutableRegisters = cameraEffect1Registers; count = 4; writeValue = range == 1 ? 70 : 66; break;
		case 2: mutableRegisters = cameraEffect2Registers; count = 4; writeValue = range == 1 ? 38 : 34; break;
		case 3: mutableRegisters = cameraEffect3Registers; count = 6; writeValue = range == 1 ? 30 : 24; break;
		case 4: mutableRegisters = cameraEffect4Registers; count = 6; writeValue = range == 1 ? 30 : 24; break;
		case 5: mutableRegisters = cameraEffect5Registers; count = 6; writeValue = range == 1 ? 30 : 24; break;
		case 6: mutableRegisters = cameraGreenishEffectRegisters; count = 6; writeValue = range == 1 ? 30 : 24; break;
		default: goto invalidLocked;
		}
		if (updateDevice) {
			mutableRegisters[1].value = writeValue;
			result = cameraWriteSensorRegisters(devnum, mutableRegisters, count);
			if (result < 0)
				goto unlock;
			if (device->config.frameTimeout)
				sceKernelDelayThread(device->config.frameTimeout);
			device->config.effect = value;
			if (!deferRegisterUpdate) {
				if (cameraReadSensorByte(devnum, 0x5580, &readValue) >= 0)
					state->colorAdjustment[0].value = readValue;
				if (cameraReadSensorByte(devnum, 0x5583, &readValue) >= 0)
					state->colorAdjustment[1].value = readValue;
				result = cameraReadSensorByte(devnum, 0x5584, &readValue);
				if (result >= 0)
					state->colorAdjustment[2].value = readValue;
			}
		} else if (!deferRegisterUpdate) {
			if (value != 2) {
				process->sensorRegisterValues[1] = mutableRegisters[2].value;
				process->sensorRegisterValues[2] = mutableRegisters[3].value;
			}
			process->sensorRegisterValues[0] =
				(process->sensorRegisterValues[0] & 0x81) | writeValue;
		}
		goto unlock;
	case 14:
		if ((unsigned int)value > 3)
			return (int)0x802e0000;
		if (!updateDevice)
			return 0;
		result = cameraReadSensorByte(devnum, 0x3821, &readValue);
		if (result < 0)
			return result;
		readValue = (value & 1) == (devnum == 0) ? readValue | 6 : readValue & 0xf9;
		result = cameraWriteSensorByte(devnum, 0x3821, &readValue);
		if (result < 0)
			return result;
		result = cameraReadSensorByte(devnum, 0x3820, &readValue);
		if (result < 0)
			return result;
		readValue = value & 2 ? readValue & 0xf9 : readValue | 6;
		result = cameraWriteSensorByte(devnum, 0x3820, &readValue);
		if (result >= 0)
			device->config.reverse = value;
		return result;
	case 16:
		switch (value) {
		case -20: registers = cameraEvMinus20Registers; break;
		case -17: registers = cameraEvMinus17Registers; break;
		case -15: registers = cameraEvMinus15Registers; break;
		case -13: registers = cameraEvMinus13Registers; break;
		case -10: registers = cameraEvMinus10Registers; break;
		case -7: registers = cameraEvMinus7Registers; break;
		case -5: registers = cameraEvMinus5Registers; break;
		case -3: registers = cameraEvMinus3Registers; break;
		case 0: registers = cameraEv0Registers; break;
		case 3: registers = cameraEv3Registers; break;
		case 5: registers = cameraEv5Registers; break;
		case 7: registers = cameraEv7Registers; break;
		case 10: registers = cameraEv10Registers; break;
		case 13: registers = cameraEv13Registers; break;
		case 15: registers = cameraEv15Registers; break;
		case 17: registers = cameraEv17Registers; break;
		case 20: registers = cameraEv20Registers; break;
		default: return (int)0x802e0000;
		}
		if (!updateDevice)
			return 0;
		result = cameraWriteSensorRegisters(devnum, registers, 4);
		if (result >= 0)
			device->config.ev = value;
		return result;
	case 17:
		config = updateDevice ? &device->config : &process->config;
		if (config->wFramerate == 120) {
			switch (config->wResolution) {
			case 2: count = 0; break;
			case 3: count = 10; break;
			case 5: count = 6; break;
			default: return (int)0x802e0009;
			}
		} else {
			switch (config->wResolution) {
			case 1: case 8: count = 0; break;
			case 2: count = 10; break;
			case 3: count = 30; break;
			case 4: count = 6; break;
			case 5: count = 23; break;
			case 6: count = 3; break;
			default: return (int)0x802e0008;
			}
		}
		if ((unsigned int)(value - 10) > (unsigned int)count)
			return (int)0x802e0000;
		if (updateDevice) {
			int width = device->config.wWidth;
			int height = device->config.wHeight;
			int framerate = device->config.wFramerate;
			int horizontalScale = 655360 / width;
			int verticalScale = 491520 / height;
			int cropWidth, cropHeight, horizontalOffset, verticalOffset;

			if (horizontalScale <= verticalScale) {
				cropWidth = 6400;
				cropHeight = horizontalScale == verticalScale ? 4800 :
					10 * (((horizontalScale * height + 512) >> 10) & ~1);
			} else {
				cropHeight = 4800;
				cropWidth = 10 * (((verticalScale * width + 512) >> 10) & ~1);
			}
			cropWidth = (cropWidth + value - 1) / value;
			cropHeight = (cropHeight + value - 1) / value;
			if (framerate == 120) {
				cropWidth = (((cropWidth + 1) >> 1) + 1) & ~1;
				cropHeight = (((cropHeight + 1) >> 1) + 1) & ~1;
				horizontalOffset = 328 - cropWidth;
				verticalOffset = 248 - cropHeight;
			} else {
				horizontalOffset = 648 - ((cropWidth + 1) & ~1);
				verticalOffset = 488 - ((cropHeight + 1) & ~1);
			}
			cameraZoomRegisters[0].value = horizontalOffset >> 9;
			cameraZoomRegisters[1].value = horizontalOffset >> 1;
			cameraZoomRegisters[2].value = verticalOffset >> 9;
			cameraZoomRegisters[3].value = verticalOffset >> 1;
			result = cameraWriteSensorRegisters(devnum, cameraZoomRegisters, 4);
			if (result >= 0)
				device->config.wZoom = value;
		}
		return result;
	case 11:
	case 12:
		config = updateDevice ? &device->config : &process->config;
		if (config->imageQuality != 0 || (unsigned int)value > 255)
			return (int)0x802e0000;
		if (updateDevice) {
			result = cameraReadSensorByte(devnum, 0x5580, &controlValue);
			if (result < 0)
				return result;
			if (!(controlValue & 4)) {
				controlValue |= 4;
				result = cameraWriteSensorByte(devnum, 0x5580, &controlValue);
				if (result < 0)
					return result;
			}
			if (attribute == 11) {
				int brightness = range == 1 ? value + 16 : value;

				result = cameraReadSensorByte(devnum, 0x5588, &readValue);
				if (result < 0)
					return result;
				if (brightness > 127) {
					readValue &= 0xf7;
					writeValue = brightness + 128;
				} else {
					readValue |= 8;
					writeValue = -(brightness + 128);
				}
				result = cameraWriteSensorByte(devnum, 0x5587, &writeValue);
				if (result < 0)
					return result;
				result = cameraWriteSensorByte(devnum, 0x5588, &readValue);
				if (result < 0)
					return result;
				device->config.brightness = value;
			} else {
				writeValue = value;
				result = cameraWriteSensorByte(devnum, 0x5586, &writeValue);
				if (result < 0)
					return result;
				device->config.contrast = value;
			}
		} else {
			controlValue = 4;
		}
		if (!deferRegisterUpdate) {
			if (updateDevice)
				state->colorAdjustment[0].value = controlValue;
			else
				process->sensorRegisterValues[0] =
					(process->sensorRegisterValues[0] & 0xfb) | (controlValue & 4);
		}
		return result;
	case 10:
		config = updateDevice ? &device->config : &process->config;
		if (config->imageQuality != 0)
			return (int)0x802e0000;
		writeValue = range == 1 ? 6 : 2;
		switch (value) {
		case 0: mutableRegisters = cameraSaturation0Registers; break;
		case 5: mutableRegisters = cameraSaturation5Registers; break;
		case 10: mutableRegisters = cameraSaturation10Registers; break;
		case 20: mutableRegisters = cameraSaturation20Registers; break;
		case 30: mutableRegisters = cameraSaturation30Registers; break;
		case 40: mutableRegisters = cameraSaturation40Registers; break;
		default: return (int)0x802e0000;
		}
		if (updateDevice) {
			mutableRegisters[0].value = writeValue;
			result = cameraWriteSensorRegisters(devnum, mutableRegisters, 6);
			if (result < 0)
				return result;
			device->config.saturation = value;
			if (!deferRegisterUpdate) {
				if (cameraReadSensorByte(devnum, 0x5580, &readValue) >= 0)
					state->colorAdjustment[0].value = readValue;
				if (cameraReadSensorByte(devnum, 0x5583, &readValue) >= 0)
					state->colorAdjustment[1].value = readValue;
				result = cameraReadSensorByte(devnum, 0x5584, &readValue);
				if (result >= 0)
					state->colorAdjustment[2].value = readValue;
			}
		} else if (!deferRegisterUpdate) {
			process->sensorRegisterValues[2] = mutableRegisters[5].value;
			process->sensorRegisterValues[1] = mutableRegisters[4].value;
			process->sensorRegisterValues[0] =
				(process->sensorRegisterValues[0] & 0xf9) | writeValue;
		}
		return result;
	default:
		return (int)0x802e000c;
	}
invalidLocked:
	result = (int)0x802e0000;
unlock:
	cameraUnlockDevice(devnum);
	return result;
}

int cameraRecoverOV7735(int devnum)
{
	unsigned char registerValue[8];
	unsigned char sensorValues[6];
	unsigned char data[60];
	unsigned char buffer[68];
	int result = 0;

	cameraLockDevice(devnum);
	if (cameraSensorInfo[devnum].useExtendedTiming == 1) {
		CameraDevice *device = &cameraDriver->device[devnum];

		if (device->config.antiflicker == 0 && device->config.autoControlHold != 1)
			result = cameraSetAntiflickerOV7735(devnum, 0);
		goto out;
	}
	if (cameraSensorInfo[devnum].revision == 1)
		goto out;
	sensorValues[0] = 0;
	sensorValues[1] = 0;
	sensorValues[2] = 0;
	sensorValues[3] = 0;
	sensorValues[4] = 0;
	sensorValues[5] = 0;
	if (!cameraDriver->i2cEnabled || !cameraDriver->clockEnabled)
		goto inspectExposure;
	buffer[0] = 0x35;
	buffer[1] = 0;
	result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
		0x74, &sensorValues[0], 1);
	if (result < 0)
		goto out;
	if (!cameraDriver->i2cEnabled || !cameraDriver->clockEnabled)
		goto inactive;
	buffer[0] = 0x35;
	buffer[1] = 1;
	result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
		0x74, &sensorValues[1], 1);
	if (result < 0)
		goto out;
	if (!cameraDriver->i2cEnabled || !cameraDriver->clockEnabled)
		goto inactive;
	buffer[0] = 0x35;
	buffer[1] = 2;
	result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
		0x74, &sensorValues[2], 1);
	if (result < 0)
		goto out;
	if (!cameraDriver->i2cEnabled || !cameraDriver->clockEnabled)
		goto inactive;
	buffer[0] = 0x35;
	buffer[1] = 0xa;
	result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
		0x74, &sensorValues[3], 1);
	if (result < 0)
		goto out;
	if (!cameraDriver->i2cEnabled || !cameraDriver->clockEnabled)
		goto inactive;
	buffer[0] = 0x35;
	buffer[1] = 0xb;
	result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
		0x74, &sensorValues[4], 1);
	if (result < 0)
		goto out;
	if (!cameraDriver->i2cEnabled || !cameraDriver->clockEnabled)
		goto inactive;
	buffer[0] = 0x56;
	buffer[1] = 0x93;
	result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74, buffer, 2,
		0x74, &sensorValues[5], 1);
	if (result < 0)
		goto out;
	goto inspectExposure;
inactive:
	result = 0;
inspectExposure:
	if (sensorValues[0] == 0 && sensorValues[1] == 0 && sensorValues[2] == 0) {
		const CameraSensorRegister *registers = cameraExposureRecoveryRegisters + 1;
		unsigned short firstAddress = 0;
		unsigned int size = 0;
		int completed = 0;

		registerValue[0] = 0;
		memset(data, 0, sizeof(data));
		if (!cameraDriver->i2cEnabled || !cameraDriver->clockEnabled) {
			result = 0;
			goto out;
		}
		for (;;) {
			unsigned int mask = registers[-1].mask;
			unsigned int address = registers[-1].address;
			unsigned char value;

			if (mask == 0xff) {
				value = registers[-1].value;
				registerValue[0] = value;
			} else {
				if (cameraDriver->i2cEnabled && cameraDriver->clockEnabled) {
					buffer[0] = address >> 8;
					buffer[1] = address;
					result = SceI2cForDriver_0A40B7BF(devnum == 0, 0x74,
						buffer, 2, 0x74, registerValue, 1);
					if (result < 0)
						goto out;
					mask = registers[-1].mask;
					address = registers[-1].address;
				}
				value = (registerValue[0] & ~mask) | (mask & registers[-1].value);
				registerValue[0] = value;
			}
			if ((devnum == 1 && address == 0x3821) || address == 0x3820) {
				value ^= 6;
				registerValue[0] = value;
			}
			++completed;
			if (size == 0)
				firstAddress = address;
			data[size++] = value;
			if (completed != 4 && address + 1 == registers[0].address && size <= 59) {
				++registers;
				continue;
			}
			if ((unsigned char)(size - 1) > 59) {
				result = (int)0x802e0000;
			} else {
				result = cameraDriver->i2cEnabled;
				if (result)
					result = cameraDriver->clockEnabled;
				if (result) {
					memset(buffer, 0, 62);
					buffer[0] = firstAddress >> 8;
					buffer[1] = firstAddress;
					size = (unsigned char)size;
					if (size)
						memcpy(buffer + 2, data, size);
					result = SceI2cForDriver_CA94A759(devnum == 0, 0x74,
						buffer, size + 2);
				}
			}
			memset(data, 0, sizeof(data));
			if (completed == 4)
				break;
			size = 0;
			++registers;
		}
	}
out:
	cameraUnlockDevice(devnum);
	return result;
}

int cameraGetSensorRegistersOV7735(int devnum, SceUInt8 *values, int defaults)
{
	int i;

	if (defaults) {
		for (i = 0; i < 20; ++i)
			values[i] = cameraDefaultSensorState.registers[i].value;
	} else {
		for (i = 0; i < 20; ++i)
			values[i] = cameraSavedSensorState[devnum].registers[i].value;
	}
	return 0;
}

int cameraSetSensorRegistersOV7735(int devnum, const SceUInt8 *values)
{
	CameraSavedSensorState *state = &cameraSavedSensorState[devnum];
	int i;

	for (i = 0; i < 20; ++i)
		state->registers[i].value = values[i];
	return 0;
}


