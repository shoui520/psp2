/*
 * paf::inputdevice::GestureListener and InputGestureListener: per port, the
 * gesture listeners attached by priority, updated highest first; a listener
 * can hold a port, or detach the listeners above or below it. The user data
 * of the listeners is counted in one list, so a widget can tell whether a
 * listener still refers to it.
 */
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>
#include <std/map>

using namespace paf;
using namespace paf::inputdevice;

typedef InputGestureListener::GestureMap GestureMap;

static map<void *, uint32_t> s_user_data_list;

int32_t GestureListener::get_id_from_bitmap(uint32_t bitmap, uint32_t idx)
{
	uint32_t count = 0;

	for (int32_t id = 0; bitmap != 0; id++) {
		if ((bitmap & 1) != 0) {
			if (count == idx)
				return id;
			count++;
		}
		bitmap >>= 1;
	}
	return -1;
}

void GestureListener::AddUserDataList(void *data)
{
	if (s_user_data_list.find(data) == s_user_data_list.end())
		s_user_data_list[data] = 1;
}

void GestureListener::IncrUserDataList(void *data)
{
	map<void *, uint32_t>::iterator it = s_user_data_list.find(data);

	if (it == s_user_data_list.end())
		AddUserDataList(data);
	else
		it->second++;
}

void GestureListener::DelUserDataList(void *data)
{
	if (s_user_data_list.size() != 0) {
		map<void *, uint32_t>::iterator it = s_user_data_list.find(data);

		if (it != s_user_data_list.end())
			s_user_data_list.erase(it);
	}
}

void GestureListener::DecrUserDataList(void *data)
{
	if (s_user_data_list.size() != 0) {
		map<void *, uint32_t>::iterator it = s_user_data_list.find(data);

		if (it != s_user_data_list.end()) {
			it->second--;
			if (it->second == 0)
				DelUserDataList(data);
		}
	}
}

void GestureListener::SetUserData(void *data)
{
	if (m_user_data != data) {
		if (m_user_data != NULL)
			DecrUserDataList(m_user_data);
		IncrUserDataList(data);
		m_user_data = data;
	}
}

int32_t GestureListener::Attach(uint32_t attach_id)
{
	if (IsAttached(attach_id))
		return m_attached_counter;
	m_attached_counter++;
	m_attached_bitmap |= 1 << attach_id;
	return m_attached_counter;
}

int32_t GestureListener::Detach(uint32_t attach_id)
{
	if (!IsAttached(attach_id))
		return -1;
	m_attached_bitmap &= ~(1 << attach_id);
	m_attached_counter--;
	return m_attached_counter;
}

bool GestureListener::IsValidUserData(void *data)
{
	return s_user_data_list.find(data) != s_user_data_list.end();
}

/*
 * The listeners marked for detaching, and those no longer attached to the
 * port, leave it; so does the port's holder once it is not attached.
 */
void InputGestureListener::update_gesture_delete(uint32_t port_id)
{
	if (port_id >= PORT_NUM)
		return;

	GestureMap &gesture = m_gesture[port_id];

	if (gesture.size() == 0)
		return;

	GestureMap::iterator it = gesture.begin();
	GestureMap::iterator end = gesture.end();

	while (it != end) {
		if (it->second.on_detach) {
			GestureMap::iterator del = it++;

			del->second.gesture_listener->Detach(port_id);
			gesture.erase(del);
		} else if (it->second.gesture_listener->IsAttached(port_id)) {
			++it;
		} else {
			GestureMap::iterator del = it++;

			gesture.erase(del);
		}
	}

	common::SharedPtr<GestureListener> &capture = m_capture[port_id];

	if (capture.get() != NULL && !capture->IsAttached(port_id))
		capture.reset();
}

int32_t paf::inputdevice::ScePafMisc_CB4DB658(InputGestureListener *listener, uint32_t port_id, void *user_data)
{
	if (port_id >= InputGestureListener::PORT_NUM)
		return -1;

	GestureMap &gesture = listener->m_gesture[port_id];
	GestureMap::iterator end = gesture.end();

	for (GestureMap::iterator it = gesture.begin(); it != end; ++it) {
		if (user_data == NULL || it->second.gesture_listener->m_user_data == user_data)
			it->second.on_detach = true;
	}
	return 0;
}

void InputGestureListener::Init()
{
	for (uint32_t i = 0; i < PORT_NUM; i++) {
		GestureMap &gesture = m_gesture[i];
		GestureMap::iterator end = gesture.end();

		for (GestureMap::iterator it = gesture.begin(); it != end; ++it)
			it->second.gesture_listener->Init();
	}
}

/* Before a port's update: its listeners' PreUpdate, and the port held by none. */
void InputGestureListener::PreUpdateGesturePort(uint32_t port_id)
{
	if (port_id >= PORT_NUM)
		return;

	GestureMap &gesture = m_gesture[port_id];

	if (gesture.size() == 0)
		return;

	GestureMap::iterator end = gesture.end();

	for (GestureMap::iterator it = gesture.begin(); it != end; ++it)
		it->second.gesture_listener->PreUpdate();
	m_capture[port_id].reset();
}

static void SetOnDetach(GestureMap::iterator first, GestureMap::iterator last)
{
	for (; first != last; ++first)
		first->second.on_detach = true;
}

/* In every port: the listeners above or below pack's listener, or itself, marked for detaching. */
void InputGestureListener::update_gesture_detach(const GestureListenerPack *pack, uint32_t flags)
{
	uint32_t detach_higher = flags & GestureListener::UPDATE_DETACH_HIGHER;
	uint32_t detach_lower = flags & GestureListener::UPDATE_DETACH_LOWER;
	uint32_t detach_self = flags & GestureListener::UPDATE_DETACH_SELF;

	for (int32_t i = 0; i < PORT_NUM; i++) {
		GestureMap &gesture = m_gesture[i];
		GestureMap::iterator end = gesture.end();
		GestureMap::iterator it = gesture.begin();

		for (; it != end; ++it) {
			if (it->second.gesture_listener.get() == pack->gesture_listener.get())
				break;
		}
		if (it == end)
			continue;

		if (detach_higher != 0)
			SetOnDetach(gesture.begin(), it);
		if (detach_lower != 0) {
			GestureMap::iterator next = it;

			SetOnDetach(++next, gesture.end());
		}
		if (detach_self != 0)
			it->second.on_detach = true;
	}
}

/*
 * The port's listeners updated by priority. What each returns is acted on:
 * the input mask cleared, the ports it is attached to held, listeners
 * detached; detaching those below ends the port's update. Bit 0: updated.
 */
uint32_t InputGestureListener::OnUpdateGesturePort(const Data *data, uint32_t port_id)
{
	uint32_t ret = 0;

	if (port_id >= PORT_NUM)
		return ret;

	update_gesture_delete(port_id);

	GestureMap &gesture = m_gesture[port_id];

	if (gesture.size() == 0)
		return ret;

	GestureMap::iterator it = gesture.begin();
	GestureMap::iterator end = gesture.end();

	while (it != end) {
		common::SharedPtr<GestureListener> listener = it->second.gesture_listener;

		if (!listener->m_on_delete)
			ret |= listener->OnUpdate(data, port_id);
		if ((ret & GestureListener::UPDATE_CLEAR_INPUT_MASK) != 0) {
			SetInputMask(port_id, 0);
			ret &= ~GestureListener::UPDATE_CLEAR_INPUT_MASK;
		}
		if ((ret & GestureListener::UPDATE_CAPTURE) != 0) {
			uint32_t count = listener->m_attached_counter;

			for (uint32_t i = 0; i < count; i++) {
				int32_t id = listener->get_id_from_bitmap(listener->m_attached_bitmap, i);

				if (m_capture[id].get() == NULL)
					m_capture[id] = listener;
			}
			ret &= ~GestureListener::UPDATE_CAPTURE;
		}
		if ((ret & GestureListener::UPDATE_DETACH_MASK) != 0) {
			uint32_t flags = ret;

			update_gesture_detach(&it->second, flags);
			ret &= ~GestureListener::UPDATE_DETACH_MASK;
			if ((flags & GestureListener::UPDATE_DETACH_LOWER) != 0)
				break;
		}
		++it;
	}
	return ret | 1;
}

/* The ports with a new listener are pre-updated and updated, or every port. */
void InputGestureListener::OnUpdateGesture(const Data *data, bool event)
{
	if (event) {
		if (m_update_port == 0)
			return;
		for (int32_t i = 0; i < PORT_NUM; i++)
			PreUpdateGesturePort(i);
	}
	for (int32_t i = 0; i < PORT_NUM; i++) {
		if (event && (m_update_port & (1 << i)) == 0)
			continue;
		OnUpdateGesturePort(data, i);
	}
	m_update_port = 0;
}

common::SharedPtr<GestureListener>
InputGestureListener::GetGestureListener(uint32_t port_id, int32_t priority, void *user_data)
{
	if (port_id >= PORT_NUM || priority < 0)
		return common::SharedPtr<GestureListener>();

	GestureMap &gesture = m_gesture[port_id];
	GestureMap::iterator it = gesture.find(priority);

	if (it == gesture.end())
		return common::SharedPtr<GestureListener>();
	if (user_data != NULL && it->second.gesture_listener->m_user_data != user_data)
		return common::SharedPtr<GestureListener>();
	return it->second.gesture_listener;
}

/* A listener of the same priority still attached to the port is kept: -1. */
int32_t InputGestureListener::AttachGestureListener(uint32_t port_id, uint32_t priority,
						    common::SharedPtr<GestureListener> listener)
{
	if (port_id >= PORT_NUM)
		return -1;

	GestureMap &gesture = m_gesture[port_id];
	GestureMap::iterator it = gesture.find(priority);

	if (it != gesture.end()) {
		if (!it->second.on_detach && it->second.gesture_listener->IsAttached(port_id))
			return -1;
		gesture.erase(it);
	}

	GestureListenerPack pack(listener);

	gesture.insert(GestureMap::value_type(priority, pack));
	listener->m_input = this;
	listener->Attach(port_id);
	m_update_port |= 1 << port_id;
	return 0;
}

int32_t InputGestureListener::DetachGestureListener(uint32_t port_id, uint32_t priority, void *user_data)
{
	if (port_id >= PORT_NUM)
		return -1;

	GestureMap &gesture = m_gesture[port_id];
	GestureMap::iterator it = gesture.find(priority);

	if (it == gesture.end())
		return -1;
	if (user_data != NULL && it->second.gesture_listener->m_user_data != user_data)
		return -1;
	it->second.on_detach = true;
	return 0;
}

int32_t InputGestureListener::DetachGestureListener(uint32_t port_id,
						    common::SharedPtr<GestureListener> listener)
{
	if (port_id >= PORT_NUM)
		return -1;

	GestureMap &gesture = m_gesture[port_id];
	GestureMap::iterator end = gesture.end();

	for (GestureMap::iterator it = gesture.begin(); it != end; ++it) {
		if (it->second.gesture_listener.get() == listener.get()) {
			it->second.on_detach = true;
			break;
		}
	}
	return 0;
}
