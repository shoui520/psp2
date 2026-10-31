/*
 * paf::inputdevice::InputListener: an object told of every frame's input,
 * registered with AddInputListener. InputGestureListener keeps, per port, the
 * GestureListeners attached to it by priority, the highest first.
 */
#ifndef PAF_INPUTDEVICE_INPUTLISTENER_H
#define PAF_INPUTDEVICE_INPUTLISTENER_H

#include <stdint.h>
#include <inputdevice/inputdevice.h>
#include <common/shared_ptr.h>
#include <std/map>

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
	InputListener() : m_init(false), m_type(DEVICE_TYPE_TOUCHSCREEN_FRONT)
	{
		Init();
	}

	InputListener(int32_t type) : m_init(false), m_type(type)
	{
		Init();
	}

	virtual ~InputListener()
	{
	}

	virtual void Init()
	{
		m_init = true;
	}

	virtual void OnUpdate(const Data *data)
	{
	}

	virtual void OnUpdateGesture(const Data *data, bool event)
	{
	}

	/* no libpaf caller */
	virtual void Reserved()
	{
	}

	bool m_init;
	int32_t m_type;
};

int32_t AddInputListener(common::SharedPtr<InputListener> &input_listener);
int32_t DelInputListener(common::SharedPtr<InputListener> &input_listener);

class InputGestureListener;

class GestureParam {
public:
	GestureParam()
	{
	}

	virtual ~GestureParam()
	{
	}

	virtual void AdjustParam()
	{
	}
};

class GestureListener {
public:
	/* What OnUpdate asks of the InputGestureListener, as bits. */
	enum {
		UPDATE_CAPTURE = 0x2,		/* hold the ports it is attached to */
		UPDATE_CLEAR_INPUT_MASK = 0x4,	/* SetInputMask(port, 0) */
		UPDATE_DETACH_SELF = 0x10,
		UPDATE_DETACH_LOWER = 0x20,	/* and stop this port's update */
		UPDATE_DETACH_HIGHER = 0x40,
		UPDATE_DETACH_MASK = 0x70
	};

	GestureListener()
		: m_input(NULL), m_on_delete(false), m_attached_bitmap(0),
		  m_attached_bitmap_pre(0), m_attached_counter(0), m_orientation(0),
		  m_user_data(NULL)
	{
	}

	virtual ~GestureListener()
	{
		if (m_user_data != NULL)
			DecrUserDataList(m_user_data);
	}

	virtual void Init()
	{
	}

	virtual void PreUpdate()
	{
	}

	virtual int32_t OnUpdate(const Data *data, uint32_t port_id)
	{
		m_attached_bitmap_pre = m_attached_bitmap;
		return 0;
	}

	virtual void SetGestureParam(const GestureParam *param)
	{
	}

	bool IsAttached(uint32_t attach_id) const
	{
		return m_attached_counter != 0 && (m_attached_bitmap & (1 << attach_id)) != 0;
	}

	int32_t Attach(uint32_t attach_id);
	int32_t Detach(uint32_t attach_id);
	void SetUserData(void *data);

	static void AddUserDataList(void *data);
	static void IncrUserDataList(void *data);
	static void DelUserDataList(void *data);
	static void DecrUserDataList(void *data);
	static bool IsValidUserData(void *data);

	int32_t get_id_from_bitmap(uint32_t bitmap, uint32_t idx);

	InputGestureListener *m_input;
	bool m_on_delete;
	uint32_t m_attached_bitmap;
	uint32_t m_attached_bitmap_pre;
	uint32_t m_attached_counter;
	uint32_t m_orientation;
	void *m_user_data;
};

/*
 * InputGestureListener's listeners of a port with this user data (all with
 * NULL) marked for detaching; no Sony name found for the NID.
 */
extern "C" int32_t ScePafMisc_CB4DB658(InputGestureListener *listener, uint32_t port_id, void *user_data);

class GestureListenerPack {
public:
	GestureListenerPack(const common::SharedPtr<GestureListener> &listener)
		: gesture_listener(listener), on_detach(false)
	{
	}

	common::SharedPtr<GestureListener> gesture_listener;
	bool on_detach;
};

class InputGestureListener : public InputListener {
public:
	enum {
		PORT_NUM = 8
	};

	typedef map<int32_t, GestureListenerPack, greater<int32_t> > GestureMap;

	InputGestureListener() : m_update_port(0)
	{
	}

	virtual ~InputGestureListener()
	{
		for (int32_t i = 0; i < PORT_NUM; i++) {
			ScePafMisc_CB4DB658(this, i, NULL);
			update_gesture_delete(i);
		}
	}

	virtual void Init();
	virtual void OnUpdateGesture(const Data *data, bool event);

	virtual int32_t SetInputMask(uint32_t port, uint32_t mask)
	{
		return 0;
	}

	void PreUpdateGesturePort(uint32_t port_id);
	uint32_t OnUpdateGesturePort(const Data *data, uint32_t port_id);
	common::SharedPtr<GestureListener> GetGestureListener(uint32_t port_id, int32_t priority, void *user_data);
	int32_t AttachGestureListener(uint32_t port_id, uint32_t priority,
				      common::SharedPtr<GestureListener> listener);
	int32_t DetachGestureListener(uint32_t port_id, uint32_t priority, void *user_data);
	int32_t DetachGestureListener(uint32_t port_id, common::SharedPtr<GestureListener> listener);
	void update_gesture_delete(uint32_t port_id);

	GestureMap m_gesture[PORT_NUM];
	/* the listener holding each port (UPDATE_CAPTURE) until its next update */
	common::SharedPtr<GestureListener> m_capture[PORT_NUM];
	/* ports with a listener attached since the last OnUpdateGesture */
	uint32_t m_update_port;

private:
	void update_gesture_detach(const GestureListenerPack *pack, uint32_t flags);
};

}
}

#endif /* PAF_INPUTDEVICE_INPUTLISTENER_H */
