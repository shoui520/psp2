/*
 * paf::inputdevice::motionsensor: SceMotion's state taken once per frame,
 * the frame before kept beside it, and the basic orientation of the device
 * worked out from the accelerometer with a tilt threshold per axis.
 */
#include <inputdevice/motionsensor/motionsensor.h>
#include <std/list>
#include <std/string.h>
#include <timer/timer.h>
#include <kernel/processmgr.h>

using namespace paf;
using namespace paf::inputdevice;
using namespace paf::inputdevice::motionsensor;

/* InitParam::m_mode of the common dialogs: SceMotion's own sampling for them */
#define MODE_COMMON_DIALOG	2

extern "C" {
int SceMotion_0D9FA7EA(void);
int SceMotion_D88B731D(void);
int SceMotion_D23686B0(SceMotionState *state);
int SceMotion_A408BC69(const SceFVector3 *threshold, const SceFVector3 *acceleration,
		       SceFVector3 *basic_orientation, const SceFVector3 *basic_orientation_pre);
}

static DeviceHandler s_device_handler;
static Data s_data_pre;
static Data s_data;
static int32_t s_lock_count;
static list<Timer> s_lock_timer;
static SceFVector3 s_threshold;
static SceUInt64 s_idle_time;
static InitParam s_init_param;

int32_t paf::inputdevice::motionsensor::Init(InitParam &init_param)
{
	Data data;

	s_init_param = init_param;
	data.timestamp = 0;
	data.accelerometer = math::v3::_000();
	data.gyroscope = math::v3::_000();
	data.device_quat = math::quaternion(0.0f, 0.0f, 0.0f, 0.0f);
	data.rotation_matrix = math::matrix::Zero();
	data.basic_orientation = math::v3::_010();
	s_data_pre = data;
	s_data = data;
	s_lock_count = 0;
	s_threshold.x = 20.0f;
	s_threshold.y = 20.0f;
	s_threshold.z = 20.0f;
	s_idle_time = 0;
	return 0;
}

int32_t paf::inputdevice::motionsensor::Activate()
{
	int32_t ret;

	if (s_init_param.m_mode == MODE_COMMON_DIALOG) {
		ret = SceMotion_0D9FA7EA();
		if (ret != 0)
			return ret;
	} else {
		ret = sceMotionStartSampling();
		if (ret != 0)
			return ret;
	}
	return 0;
}

int32_t paf::inputdevice::motionsensor::Deactivate()
{
	int32_t ret;

	if (s_init_param.m_mode == MODE_COMMON_DIALOG) {
		ret = SceMotion_D88B731D();
		if (ret != 0)
			return ret;
	} else {
		ret = sceMotionStopSampling();
		if (ret != 0)
			return ret;
	}
	return 0;
}

int32_t paf::inputdevice::motionsensor::End()
{
	return 0;
}

void paf::inputdevice::motionsensor::Unlock()
{
	if (s_lock_count > 0)
		s_lock_count--;
}

int32_t paf::inputdevice::motionsensor::Update(float diff_time, const Data **data,
					       const Data **data_pre)
{
	if (s_lock_timer.size() != 0) {
		list<Timer>::iterator it = s_lock_timer.begin();

		while (it != s_lock_timer.end()) {
			if (it->m_state == Timer::State_Finished) {
				it = s_lock_timer.erase(it);
				Unlock();
			} else {
				it->DoTick();
				++it;
			}
		}
	}

	if (data != NULL)
		*data = &s_data;
	if (data_pre != NULL)
		*data_pre = &s_data_pre;
	s_data_pre = s_data;

	SceMotionState state;
	int32_t ret;

	sce_paf_memset(&state, 0, sizeof(state));
	if (s_init_param.m_mode == MODE_COMMON_DIALOG) {
		ret = SceMotion_D23686B0(&state);
		if (ret < 0)
			return ret;
	} else {
		ret = sceMotionGetState(&state);
		if (ret < 0)
			return ret;
	}

	s_data.timestamp = state.timestamp;
	s_data.accelerometer = math::v3(state.acceleration.x, state.acceleration.y,
					state.acceleration.z);
	s_data.gyroscope = math::v3(state.angularVelocity.x, state.angularVelocity.y,
				    state.angularVelocity.z);
	s_data.device_quat = math::quaternion(state.deviceQuat.x, state.deviceQuat.y,
					      state.deviceQuat.z, state.deviceQuat.w);
	s_data.rotation_matrix = math::matrix(
		math::v4(state.rotationMatrix.fm.x.x, state.rotationMatrix.fm.x.y,
			 state.rotationMatrix.fm.x.z, state.rotationMatrix.fm.x.w),
		math::v4(state.rotationMatrix.fm.y.x, state.rotationMatrix.fm.y.y,
			 state.rotationMatrix.fm.y.z, state.rotationMatrix.fm.y.w),
		math::v4(state.rotationMatrix.fm.z.x, state.rotationMatrix.fm.z.y,
			 state.rotationMatrix.fm.z.z, state.rotationMatrix.fm.z.w),
		math::v4(state.rotationMatrix.fm.w.x, state.rotationMatrix.fm.w.y,
			 state.rotationMatrix.fm.w.z, state.rotationMatrix.fm.w.w));

	SceFVector3 threshold = s_threshold;
	SceFVector3 basic_orientation;
	SceFVector3 basic_orientation_pre;

	sce_paf_memset(&basic_orientation_pre, 0, sizeof(basic_orientation_pre));
	sce_paf_memset(&basic_orientation, 0, sizeof(basic_orientation));
	basic_orientation_pre.x = s_data.basic_orientation.extract_x();
	basic_orientation_pre.y = s_data.basic_orientation.extract_y();
	basic_orientation_pre.z = s_data.basic_orientation.extract_z();
	SceMotion_A408BC69(&threshold, &state.acceleration, &basic_orientation,
			   &basic_orientation_pre);
	s_data.data_info = state.dataInfo;
	s_data.basic_orientation = math::v3(basic_orientation.x, basic_orientation.y,
					    basic_orientation.z);

	if (s_idle_time == 0)
		s_idle_time = sceKernelGetProcessTimeWide();
	if (s_device_handler != NULL)
		s_device_handler(&s_data);
	return 0;
}

DeviceHandler paf::inputdevice::motionsensor::SetDeviceHandler(DeviceHandler new_handler)
{
	DeviceHandler old_handler = s_device_handler;

	s_device_handler = new_handler;
	return old_handler;
}

void paf::inputdevice::motionsensor::Lock()
{
	s_lock_count++;
}

bool paf::inputdevice::motionsensor::IsLocked()
{
	return s_lock_count > 0;
}

void ScePafMisc_BB74B3B8(float time)
{
	Timer timer;

	timer.InitTimer(time, 0, 0, -1, NULL);
	s_lock_timer.push_back(timer);
	s_lock_count++;
}

void ScePafMisc_DC13D542(const SceFVector3 *threshold)
{
	if (threshold != NULL)
		s_threshold = *threshold;
}

void ScePafMisc_B4E6CA68(SceFVector3 *threshold)
{
	if (threshold != NULL)
		*threshold = s_threshold;
}

bool paf::inputdevice::motionsensor::IsOwnershipReleasable()
{
	return true;
}
