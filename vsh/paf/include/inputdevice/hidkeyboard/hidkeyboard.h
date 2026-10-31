/*
 * paf::inputdevice::hidkeyboard: the cursor, enter and escape keys of USB and
 * Bluetooth keyboards, read through SceHid.
 */
#ifndef PAF_INPUTDEVICE_HIDKEYBOARD_HIDKEYBOARD_H
#define PAF_INPUTDEVICE_HIDKEYBOARD_HIDKEYBOARD_H

#include <stdint.h>

namespace paf {
namespace inputdevice {
namespace hidkeyboard {

class InitParam {
public:
	int32_t m_mode;
};

/* The keys held down, in the order they were pressed. */
class Data {
public:
	enum Keycode {
		Keycode_Up = 1,
		Keycode_Down,
		Keycode_Left,
		Keycode_Right,
		Keycode_Return,
		Keycode_Escape
	};

	enum {
		KEY_MAX = 13
	};

	Data &operator=(const Data &other)
	{
		for (int32_t i = 0; i < KEY_MAX; i++)
			keycode[i] = other.keycode[i];
		num = other.num;
		return *this;
	}

	bool operator==(const Data &other) const
	{
		for (int32_t i = 0; i < KEY_MAX; i++) {
			if (keycode[i] != other.keycode[i])
				return false;
		}
		return true;
	}

	void Clear()
	{
		for (int32_t i = 0; i < KEY_MAX; i++)
			keycode[i] = 0;
		num = 0;
	}

	bool IsEmpty() const
	{
		for (int32_t i = 0; i < KEY_MAX; i++) {
			if (keycode[i] != 0)
				return false;
		}
		return true;
	}

	bool Find(uint32_t key) const
	{
		bool found = false;

		for (uint32_t i = 0; i < num; i++) {
			if (keycode[i] == key)
				found = true;
		}
		return found;
	}

	void Add(uint32_t key)
	{
		if (num != KEY_MAX)
			keycode[num++] = key;
	}

	/* key out of the list, the keys after it moved up */
	void Remove(uint32_t key)
	{
		bool removed = false;
		uint32_t last = num - 1;

		for (uint32_t i = 0; i < KEY_MAX && i < num; i++) {
			if (keycode[i] == key) {
				removed = true;
				keycode[i] = 0;
			} else if (!removed) {
				continue;
			}
			if (i < KEY_MAX - 1 && i < last) {
				keycode[i] = keycode[i + 1];
				keycode[i + 1] = 0;
			}
		}
		if (removed)
			num = last;
	}

	uint32_t keycode[KEY_MAX];
	uint32_t num;
};

int32_t Init(InitParam &init_param);
int32_t End();
int32_t Activate();
int32_t Deactivate();
bool IsActive();
int32_t Update(float diff_time, const Data **data, const Data **data_pre);
void Lock();
void Reserved();
void LockTimer(float time);
int32_t Unlock();
bool IsValid();
float GetIdleTime();

}
}
}

#endif /* PAF_INPUTDEVICE_HIDKEYBOARD_HIDKEYBOARD_H */
