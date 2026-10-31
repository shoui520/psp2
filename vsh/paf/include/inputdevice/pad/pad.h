/*
 * paf::inputdevice::pad: the buttons and sticks of the Vita and of the
 * controllers on ports 1 to 4, read through SceCtrl. The port last used is
 * the current one; its sample is what the pad reports.
 */
#ifndef PAF_INPUTDEVICE_PAD_PAD_H
#define PAF_INPUTDEVICE_PAD_PAD_H

#include <stdint.h>

namespace paf {
namespace inputdevice {
namespace pad {

class InitParam {
public:
	enum Mode {
		Mode_Normal = 0,
		Mode_DigitalAnalog = 1,	/* the PS button is reported */
		Mode_CommonDialog = 2,	/* SceCtrl's Ext sampling */
		Mode_DigitalOnly = 3
	};

	int32_t m_mode;
};

/* One port's sample, in paf's keys. */
class Data {
public:
	enum Keycode {
		PAD_UP = 0x1,
		PAD_DOWN = 0x2,
		PAD_LEFT = 0x4,
		PAD_RIGHT = 0x8,
		PAD_ENTER = 0x10,
		PAD_ESCAPE = 0x20,
		PAD_MENU = 0x40,	/* triangle */
		PAD_DISPLAY = 0x80,	/* square */
		PAD_SELECT = 0x100,
		PAD_START = 0x200,
		PAD_L = 0x400,
		PAD_R = 0x800,
		PAD_PS = 0x1000,
		PAD_KEY_OPEN = 0x2000,
		PAD_VOL_PLUS = 0x4000,
		PAD_VOL_MINUS = 0x8000,
		PAD_L2 = 0x10000,
		PAD_R2 = 0x20000,
		PAD_L3 = 0x40000,
		PAD_R3 = 0x80000,
		PAD_DIRECTION = PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT
	};

	enum {
		STICK_CENTER = 128
	};

	void Clear()
	{
		buttons = 0;
	}

	/* never written: SceCtrl's time stamp is not kept */
	uint64_t timestamp;
	uint32_t buttons;
	uint16_t lx;
	uint16_t ly;
	uint16_t rx;
	uint16_t ry;
};

typedef void (*DeviceHandler)(Data *data);

int32_t Init(InitParam &init_param);
int32_t End();
void Unlock();
void SetMask(uint32_t mask);
int32_t Update(float diff_time, const Data **data, const Data **data_pre);
DeviceHandler SetDeviceHandler(DeviceHandler new_handler);
uint32_t GetMask();
void Lock();
bool IsLocked();
int32_t GetCurrPort();
int32_t SetCurrPort(int32_t port);
bool IsOwnershipReleasable();
void ResetIdleTime();

}
}
}

/*
 * No Sony names found for these NIDs: the current port's held keys added to
 * the mask; the pad locked for a time; the enter button (0: circle) asked for
 * and the one in use; the main port set and read; the controller type of a
 * port (1: DualShock 3, 2: DualShock 4); the time with no key held, in
 * seconds.
 */
extern "C" void ScePafMisc_C4E8CBE6(uint32_t mask);
extern "C" void ScePafMisc_FA86D1FA(float time);
extern "C" int32_t ScePafMisc_0D1B028E(int32_t enter_button);
extern "C" int32_t ScePafMisc_9CB5D72E(void);
extern "C" int32_t ScePafMisc_57080F75(int32_t port);
extern "C" int32_t ScePafMisc_333C86DC(void);
extern "C" int32_t ScePafMisc_CD2B0967(int32_t port);
extern "C" float ScePafMisc_7C9C7E59(void);

#endif /* PAF_INPUTDEVICE_PAD_PAD_H */
