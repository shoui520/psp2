/*
 * paf::inputdevice::InputListener: an object told of every frame's input,
 * registered with AddInputListener.
 */
#ifndef PAF_INPUTDEVICE_INPUTLISTENER_H
#define PAF_INPUTDEVICE_INPUTLISTENER_H

#include <stdint.h>
#include <inputdevice/inputdevice.h>
#include <common/shared_ptr.h>

namespace paf {
namespace inputdevice {

/* What an InputListener listens to; the device it activates. */
enum {
	DEVICE_TYPE_PAD = 0,
	DEVICE_TYPE_TOUCHSCREEN_FRONT = 3,
	DEVICE_TYPE_TOUCHSCREEN_BACK = 4,
	DEVICE_TYPE_MOTIONSENSOR = 5,
	DEVICE_TYPE_BTRM = 8,
	DEVICE_TYPE_HIDRM = 9,
	DEVICE_TYPE_KEYBOARD = 10,
	DEVICE_TYPE_NUM = 11
};

class InputListener {
public:
	virtual ~InputListener();
	virtual void Init();
	virtual void OnUpdate(const Data *data);
	virtual void OnUpdateGesture(const Data *data, bool event);

	bool m_init;
	int32_t m_type;
};

int32_t AddInputListener(common::SharedPtr<InputListener> &input_listener);
int32_t DelInputListener(common::SharedPtr<InputListener> &input_listener);

}
}

#endif /* PAF_INPUTDEVICE_INPUTLISTENER_H */
