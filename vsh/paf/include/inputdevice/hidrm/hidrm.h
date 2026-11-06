/*
 * paf::inputdevice::hidrm: the BD remote, read through SceCtrl's BD remote
 * callback. The callback keeps the key held down; Update hands it to the input
 * device once per frame.
 */
#ifndef PAF_INPUTDEVICE_HIDRM_HIDRM_H
#define PAF_INPUTDEVICE_HIDRM_HIDRM_H

#include <stdint.h>

namespace paf {
namespace inputdevice {
namespace hidrm {

class InitParam {
public:
	int32_t m_mode;
};

/* The remote's key held down (one at a time), as hidkeyboard's Data. */
class Data {
public:
	enum {
		KEY_MAX = 1
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

		for (uint32_t i = 0; i < KEY_MAX && i < num; i++) {
			if (keycode[i] == key) {
				removed = true;
				keycode[i] = 0;
			} else if (!removed) {
				continue;
			}
			if (i + 1 < KEY_MAX && i + 1 < num) {
				keycode[i] = keycode[i + 1];
				keycode[i + 1] = 0;
			}
		}
		if (removed)
			num--;
	}

	uint32_t keycode[KEY_MAX];
	uint32_t num;
};

int32_t Init(InitParam &init_param);
int32_t Activate();
int32_t Deactivate();
int32_t End();
int32_t Unlock();
int32_t Update(float diff_time, const Data **data, const Data **data_pre);
/* the keys held down when the remote was locked */
Data GetLockedData();
void Lock();
void LockTimer(float time);
bool IsOwnershipReleasable();

}
}
}

#endif /* PAF_INPUTDEVICE_HIDRM_HIDRM_H */
