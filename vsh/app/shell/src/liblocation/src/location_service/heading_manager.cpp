/*
 * heading_manager.cpp: HeadingManager ("SceLocationHeadInternalLoc", SceMotion quaternion
 * SLERP smoothing, magnetic declination adjustment via IGRF-11, and heading angle/hysteresis
 * calculation; 0x81194E8C-0x81196BB8, sti 0x81196B0E). The file and class names are ours
 * (PLACEHOLDER).
 */
#include "location_internal.h"
#include <math.h>
#include <regmgr/regmgr.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <system/system.h>

extern "C" {
SceUInt64 sceRtcGetAccumulativeTime(void);
float sce_paf_sinf(float x);
float sce_paf_cosf(float x);
float sce_paf_atan2f(float y, float x);
int SceMotion_ACEFA121(void);
int SceMotion_BC5C86C5(void);
int SceMotion_AD0B62B7(SceMotionState *state);
int sceMotionMagnetometerOn(void);
int sceMotionMagnetometerOff(void);
}

extern void shellRegMgrLogError(int err, int flag);		/* 0x81345038 */

static float wrapDeg360(float deg)
{
	float r = deg - (float)(int)(deg / 360.0f) * 360.0f;
	if (r >= 360.0f)
		return (float)((double)r - 360.0);
	if (r < 0.0f)
		return (float)((double)r + 360.0);
	return r;
}

static LocationQuat slerpHalf(const LocationQuat &lhs, const LocationQuat &rhs)
{
	float bx = rhs.x, by = rhs.y, bz = rhs.z, bw = rhs.w;
	float dot = lhs.x * bx + lhs.w * bw + lhs.y * by + lhs.z * bz;
	if (dot < 0.0f) {
		bx = -bx;
		by = -by;
		bz = -bz;
		bw = -bw;
		dot = -dot;
	}
	float wa = 0.5f;
	float wb = 0.5f;
	if (dot <= 0.9999f) {
		float s = __builtin_sqrtf(1.0f - dot * dot);
		float theta = sce_paf_atan2f(s, dot);
		float invSin = 1.0f / s;
		wa = sce_paf_sinf(theta * 0.5f) * invSin;
		wb = sce_paf_sinf(theta * 0.5f) * invSin;
	}
	LocationQuat out;
	out.x = wb * bx + lhs.x * wa;
	out.y = wb * by + lhs.y * wa;
	out.z = wb * bz + lhs.z * wa;
	out.w = wb * bw + lhs.w * wa;
	return out;
}

/* 0x81194E8C */
HeadingManager::HeadingManager(PositionManager *posMgr)
	: m_posMgr(posMgr)
	, m_initialized(0)
	, m_pollRefCount(0)
	, m_motionStarted(0)
	, m_magEnabled(0)
	, m_lastLat(0.0)
	, m_lastLon(0.0)
	, m_mutexResult(-1)
	, m_orientation(0)
	, m_valid(0)
	, m_ringHead(0)
	, m_ringTail(0)
	, m_ringLast(0)
	, m_lastPollTime(0)
{
	m_wmm.declination = 0.0f;
	m_curQuat.x = 0.0f;
	m_curQuat.y = 0.0f;
	m_curQuat.z = 0.0f;
	m_curQuat.w = 1.0f;
	m_pollTask.m_mgr = NULL;
	m_pollTask.m_slotIndex = 0;
	m_pollTask.m_headingMgr = this;
}

/* 0x81194F14 */
int HeadingManager::StopMotion()
{
	if (paf::system::Model::GetType() != 0) {
		m_motionStarted = 0;
		return 0;
	}
	int ret = SceMotion_BC5C86C5();
	if (ret == 0)
		m_motionStarted = 0;
	return ret;
}

/* 0x81194F34 */
int HeadingManager::IsMotionStarted()
{
	return m_motionStarted;
}

/* 0x81194F38 */
int HeadingManager::Term()
{
	int ret = 0;
	if (g_locationManager != NULL)
		g_locationManager->UnregisterTask(&m_pollTask);
	else
		ret = SCE_LOCATION_ERROR_FATAL_ERROR;

	if (m_mutexResult == 0) {
		int dret = sceKernelDeleteLwMutex(&m_mutex);
		if (dret != 0)
			ret = dret;
		else
			m_mutexResult = -1;
	}

	if (m_motionStarted != 0) {
		if (paf::system::Model::GetType() != 0) {
			m_motionStarted = 0;
		} else {
			int mret = SceMotion_BC5C86C5();
			if (mret == 0)
				m_motionStarted = 0;
			if (mret < 0)
				ret = mret;
		}
	}
	m_initialized = 0;
	return ret;
}

/* 0x81194F9E / 0x81195032 */
HeadingManager::~HeadingManager()
{
	if (m_initialized != 0)
		Term();
}

/* 0x81195042 */
int HeadingManager::LoadLastLocation()
{
	char buf[12];
	int ret = sceRegMgrGetKeyStr("/CONFIG/LOCATION", "last_latitude", buf, sizeof(buf));
	if (ret != 0) {
		m_lastLat = -9999.0;
		shellRegMgrLogError(ret, 1);
	} else {
		m_lastLat = sce_paf_atof(buf);
	}

	ret = sceRegMgrGetKeyStr("/CONFIG/LOCATION", "last_longitude", buf, sizeof(buf));
	if (ret != 0) {
		m_lastLon = -9999.0;
		shellRegMgrLogError(ret, 1);
	} else {
		m_lastLon = sce_paf_atof(buf);
	}

	if (m_lastLat != -9999.0 && m_lastLon != -9999.0) {
		SceDateTime dt;
		sceRtcGetCurrentClock(&dt, 0);
		wmmSetDate(&m_wmm, dt.month, dt.day, dt.year);
		wmmCalcDeclination(&m_wmm, (float)m_lastLat, (float)m_lastLon, 0.0f);
	}
	return ret;
}

/* 0x81195164 */
int HeadingManager::Init()
{
	m_mutexResult = sceKernelCreateLwMutex(&m_mutex, "SceLocationHeadInternalLoc", 2, 0, NULL);
	if (m_mutexResult != 0)
		return m_mutexResult;
	if (g_locationManager == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	int ret = g_locationManager->RegisterTask(&m_pollTask);
	if (ret == 0) {
		LoadLastLocation();
		m_initialized = 1;
	}
	return ret;
}

/* 0x811951BE */
int HeadingManager::StartMotion()
{
	if (paf::system::Model::GetType() != 0) {
		m_motionStarted = 1;
		return 0;
	}
	int ret = SceMotion_ACEFA121();
	if (ret == 0)
		m_motionStarted = 1;
	return ret;
}

/* 0x811951E0 */
int HeadingManager::EnableMagnetometer()
{
	if (paf::system::Model::GetType() != 0) {
		m_magEnabled = 1;
		return 0;
	}
	int ret = sceMotionMagnetometerOn();
	if (ret == 0)
		m_magEnabled = 1;
	return ret;
}

/* 0x81195202 */
int HeadingManager::DisableMagnetometer()
{
	if (paf::system::Model::GetType() != 0) {
		m_magEnabled = 0;
		return 0;
	}
	int ret = sceMotionMagnetometerOff();
	if (ret == 0) {
		m_magEnabled = 0;
		m_valid = 0;
	}
	return ret;
}

/* 0x81195226 */
int HeadingManager::PollMotion()
{
	if (paf::system::Model::GetType() != 0) {
		m_curQuat.x = 0.0f;
		m_curQuat.y = 0.0f;
		m_curQuat.z = 0.0f;
		m_curQuat.w = 1.0f;
		m_valid = 0;
		m_smoothQuat.x = 0.0f;
		m_smoothQuat.y = 0.0f;
		m_smoothQuat.z = 0.0f;
		m_smoothQuat.w = 1.0f;
		m_ringHead = 0;
		m_ringTail = 0;
		m_ringLast = 0;
		return SCE_LOCATION_INFO_UNDETERMINED_HEADING;
	}

	int ret = SceMotion_AD0B62B7(&m_motionState);
	if (ret != 0) {
		m_curQuat.x = 0.0f;
		m_curQuat.y = 0.0f;
		m_curQuat.z = 0.0f;
		m_curQuat.w = 1.0f;
		m_valid = 0;
		m_smoothQuat.x = 0.0f;
		m_smoothQuat.y = 0.0f;
		m_smoothQuat.z = 0.0f;
		m_smoothQuat.w = 1.0f;
		m_ringHead = 0;
		m_ringTail = 0;
		m_ringLast = 0;
		return ret;
	}

	float rxx = m_motionState.nedMatrix.fm.x.x;
	float rxy = m_motionState.nedMatrix.fm.x.y;
	float rxz = m_motionState.nedMatrix.fm.x.z;
	float ryx = m_motionState.nedMatrix.fm.y.x;
	float ryy = m_motionState.nedMatrix.fm.y.y;
	float ryz = m_motionState.nedMatrix.fm.y.z;
	float rzx = m_motionState.nedMatrix.fm.z.x;
	float rzy = m_motionState.nedMatrix.fm.z.y;
	float rzz = m_motionState.nedMatrix.fm.z.z;

	float diagX = (rxx - ryy - rzz) + 1.0f;
	float diagY = (ryy - rxx - rzz) + 1.0f;
	float diagZ = (-rxx - ryy + rzz) + 1.0f;
	float diagW = (rxx + ryy + rzz) + 1.0f;

	unsigned int idx = 0;
	float best = diagX;
	if (diagY > best) { idx = 1; best = diagY; }
	if (diagZ > best) { idx = 2; best = diagZ; }
	if (diagW > best) { idx = 3; best = diagW; }

	float s = (float)(__builtin_sqrtf(best) * 0.5);
	float inv = 0.25f / s;
	LocationQuat q;
	if (idx == 0) {
		q.x = s;
		q.y = (ryx + rxy) * inv;
		q.z = (rzx + rxz) * inv;
		q.w = (rzy - ryz) * inv;
	} else if (idx == 1) {
		q.x = (ryx + rxy) * inv;
		q.y = s;
		q.z = (rzy + ryz) * inv;
		q.w = (rxz - rzx) * inv;
	} else if (idx == 2) {
		q.x = (rzx + rxz) * inv;
		q.y = (rzy + ryz) * inv;
		q.z = s;
		q.w = (ryx - rxy) * inv;
	} else {
		q.x = (rzy - ryz) * inv;
		q.y = (rxz - rzx) * inv;
		q.z = (ryx - rxy) * inv;
		q.w = s;
	}

	int isValid = 0;
	if (m_motionState.dataInfo != 1) {
		if (m_valid != 0 || q.w != m_curQuat.w || q.x != m_curQuat.x ||
		    q.y != m_curQuat.y || q.z != m_curQuat.z)
			isValid = 1;
	}
	m_valid = isValid;
	if (isValid != 1) {
		m_ringHead = 0;
		m_ringTail = 0;
		m_ringLast = 0;
		return SCE_LOCATION_INFO_UNDETERMINED_HEADING;
	}

	m_curQuat = q;
	m_ring[m_ringTail] = q;
	unsigned int nextTail = (m_ringTail + 1) % 9;
	if (nextTail == m_ringHead && m_ringTail != m_ringHead)
		m_ringHead = (m_ringHead + 1) % 9;
	if (nextTail != m_ringHead) {
		m_ringLast = m_ringTail;
		m_ringTail = nextTail;
	}

	unsigned int count = (m_ringTail >= m_ringHead) ? (m_ringTail - m_ringHead)
							: (m_ringTail - m_ringHead + 9);
	LocationQuat pairAvg[4];
	for (unsigned int i = 0; i < 4; i++) {
		unsigned int firstOff = 2 * i;
		unsigned int secondOff = 2 * i + 1;
		if (firstOff >= count) firstOff = count - 1;
		if (secondOff >= count) secondOff = count - 1;
		unsigned int firstIdx = (m_ringHead + firstOff) % 9;
		unsigned int secondIdx = (m_ringHead + secondOff) % 9;
		pairAvg[i] = slerpHalf(m_ring[firstIdx], m_ring[secondIdx]);
	}
	LocationQuat quadFirst = slerpHalf(pairAvg[0], pairAvg[1]);
	LocationQuat quadSecond = slerpHalf(pairAvg[2], pairAvg[3]);
	m_smoothQuat = slerpHalf(quadFirst, quadSecond);
	return ret;
}

/* 0x81195AFC */
int HeadingManager::ComputeHeadingAngle(const float *xyVec, float *outHeading,
					const SceUInt32 *hmethod, float prevHeading)
{
	float rawDeg =
		(float)((double)sce_paf_atan2f(xyVec[1], xyVec[0]) * 180.0 / 3.141592653589793);
	float deg;
	switch (*hmethod & 7) {
	case 2:
		deg = wrapDeg360(rawDeg);
		break;
	case 4:
		deg = wrapDeg360(rawDeg + 180.0f);
		break;
	case 3:
	default:
		deg = wrapDeg360(rawDeg - 90.0f);
		break;
	}

	float step = locationDecodeHeadingAngle(hmethod);
	if ((*hmethod & 0x2000) != 0) {
		float half = step * 0.5f;
		float quarter = half * 0.5f;
		float thresh = wrapDeg360(half + quarter);
		if (half != 0.0f) {
			float snapped = (float)(int)((prevHeading + quarter) / half) * half;
			prevHeading = wrapDeg360(snapped);
		}
		float center = wrapDeg360(prevHeading);
		float hi = wrapDeg360(center + thresh);
		float lo = wrapDeg360(center - thresh);

		bool aboveHi = (hi < 180.0f) ? (deg >= hi && deg < hi + 180.0f)
					     : (deg >= hi || deg <= hi - 180.0f);
		bool insideLo = (lo < 180.0f) ? (deg > lo && deg < lo + 180.0f)
					      : (deg > lo || deg <= lo - 180.0f);
		if (aboveHi || !insideLo) {
			if (step != 0.0f)
				deg = wrapDeg360((float)(int)((deg + half) / step) * step);
			*outHeading = deg;
		} else {
			*outHeading = center;
		}
	} else {
		if (step != 0.0f)
			deg = wrapDeg360((float)(int)((deg + step * 0.5f) / step) * step);
		*outHeading = deg;
	}
	return 0;
}

/* 0x81196386 */
int HeadingManager::RotateBasisVector(const LocationQuat *quat, const SceUInt32 *hmethod,
				      float *outVec)
{
	float vx = 0.0f, vy = 0.0f, vz = 0.0f;
	switch (*hmethod & 7) {
	case 2:
		vy = 1.0f;
		break;
	case 4:
		vz = 1.0f;
		break;
	case 3:
	default:
		vx = 1.0f;
		break;
	}

	float qx = quat->x, qy = quat->y, qz = quat->z, qw = quat->w;
	float tx = qy * vz - qz * vy + qw * vx;
	float ty = qz * vx - qx * vz + qw * vy;
	float tz = qx * vy - qy * vx + qw * vz;
	float tw = -(qx * vx) - (qy * vy) - (qz * vz);

	outVec[0] = (tz * qy - ty * qz) + tx * qw - tw * qx;
	outVec[1] = (tx * qz - tz * qx) + ty * qw - tw * qy;
	outVec[2] = (ty * qx - tx * qy) + tz * qw - tw * qz;
	outVec[3] = 0.0f;
	return 0;
}

/* 0x811964BE */
int HeadingManager::GetHeading(const SceUInt32 *hmethod, SceLocationHeadingInfo *outInfo)
{
	if (m_valid == 0 || paf::system::Model::GetType() != 0) {
		outInfo->heading = -9999.0f;
		outInfo->nedQuat.x = -9999.0f;
		outInfo->nedQuat.y = -9999.0f;
		outInfo->nedQuat.z = -9999.0f;
		outInfo->nedQuat.w = -9999.0f;
		outInfo->stability = 0;
		locationGetCurrentTick(&outInfo->timestamp);
		return SCE_LOCATION_INFO_UNDETERMINED_HEADING;
	}

	SceUInt32 method = *hmethod;
	LocationQuat q = ((method & 0x1000) != 0) ? m_smoothQuat : m_curQuat;

	if ((int)method >= 0) {
		float rad = (float)((double)m_wmm.declination * 3.14159265 / 180.0);
		float c = sce_paf_cosf(rad);
		float s = sce_paf_sinf(rad);
		float diagX = (c - c - 1.0f) + 1.0f;
		float diagY = (c - c - 1.0f) + 1.0f;
		float diagZ = (-c - c + 1.0f) + 1.0f;
		float diagW = (c + c + 1.0f) + 1.0f;
		unsigned int idx = 0;
		float best = diagX;
		if (diagY > best) { idx = 1; best = diagY; }
		if (diagZ > best) { idx = 2; best = diagZ; }
		if (diagW > best) { idx = 3; best = diagW; }
		float qs = (float)(__builtin_sqrtf(best) * 0.5);
		float inv = 0.25f / qs;
		LocationQuat rot = { 0.0f, 0.0f, 0.0f, 1.0f };
		if (idx == 0) { rot.x = qs; rot.y = (s - s) * inv; }
		else if (idx == 1) { rot.x = (s - s) * inv; rot.y = qs; }
		else if (idx == 2) { rot.z = qs; rot.w = (s + s) * inv; }
		else { rot.z = (s + s) * inv; rot.w = qs; }

		LocationQuat r;
		r.x = rot.x * q.w + rot.w * q.x + rot.y * q.z - rot.z * q.y;
		r.y = rot.y * q.w + rot.w * q.y + rot.z * q.x - rot.x * q.z;
		r.z = rot.z * q.w + rot.w * q.z + rot.x * q.y - rot.y * q.x;
		r.w = rot.w * q.w - rot.x * q.x - rot.y * q.y - rot.z * q.z;
		q = r;
	}

	float gx = m_motionState.basicOrientation.x;
	float gy = m_motionState.basicOrientation.y;
	if (gy != 0.0f)
		m_orientation = 0;
	else if (gx != 0.0f)
		m_orientation = 1;

	SceUInt32 effMethod = method;
	if ((method & 7) == 1)
		effMethod = (method & ~7U) | ((m_orientation == 0) ? 3U : 2U);

	float basisVec[4];
	RotateBasisVector(&q, &effMethod, basisVec);
	float norm = __builtin_sqrtf(basisVec[0] * basisVec[0] + basisVec[1] * basisVec[1]);
	float xyVec[2] = { basisVec[0] / norm, basisVec[1] / norm };
	ComputeHeadingAngle(xyVec, &outInfo->heading, &effMethod, 0.0f);

	outInfo->nedQuat.x = q.x;
	outInfo->nedQuat.y = q.y;
	outInfo->nedQuat.z = q.z;
	outInfo->nedQuat.w = q.w;
	outInfo->stability = m_motionState.magnFieldStability;
	locationGetCurrentTick(&outInfo->timestamp);
	return 0;
}

/* 0x8119685E */
int HeadingManager::StartPolling()
{
	if (m_pollRefCount == 0) {
		m_lastPollTime = sceRtcGetAccumulativeTime();
		m_pollTask.Schedule(0);
	}
	m_pollRefCount++;
	return 0;
}

/* 0x8119688A */
int HeadingManager::StopPolling()
{
	if (m_pollRefCount > 0) {
		m_pollRefCount--;
		if (m_pollRefCount == 0)
			m_pollTask.Cancel();
		return 0;
	}
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* 0x811968B2 */
int HeadingManager::IsMagnetometerEnabled()
{
	return m_magEnabled;
}

/* 0x811968B6 */
int HeadingManager::SaveLastLocation()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		if (m_lastLat != -9999.0 && m_lastLon != -9999.0) {
			char buf[12];
			sce_paf_memset(buf, 0, sizeof(buf));
			sce_paf_snprintf(buf, 11, "%-.5f", m_lastLat);
			ret = sceRegMgrSetKeyStr("/CONFIG/LOCATION", "last_latitude", buf, sizeof(buf));
			if (ret == 0) {
				sce_paf_snprintf(buf, 11, "%-.5f", m_lastLon);
				ret = sceRegMgrSetKeyStr("/CONFIG/LOCATION", "last_longitude", buf, sizeof(buf));
			}
			if (ret != 0)
				shellRegMgrLogError(ret, 1);
		}
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x811969CA */
void HeadingManager::UpdateLocation(const double *latLon)
{
	if (sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
		m_lastLat = latLon[0];
		m_lastLon = latLon[1];
		sceKernelUnlockLwMutex(&m_mutex, 1);
		if (m_lastLat != -9999.0 && m_lastLon != -9999.0) {
			SceDateTime dt;
			sceRtcGetCurrentClock(&dt, 0);
			wmmSetDate(&m_wmm, dt.month, dt.day, dt.year);
			wmmCalcDeclination(&m_wmm, (float)m_lastLat, (float)m_lastLon, 0.0f);
		}
	}
}

/* 0x81196A90 */
void HeadingManager::OnHeadingConfigChanged()
{
	m_posMgr->UpdateHeadingState();
}

/* 0x81196A9C */
void HeadingManager::OnSuspend()
{
	if (m_pollRefCount > 0)
		m_pollTask.Cancel();
}

/* 0x81196AB0 */
void HeadingManager::OnResume()
{
	if (m_pollRefCount > 0)
		m_pollTask.Schedule(0);
}

/* 0x81196AC6 */
int HeadingPollTask::OnNotify()
{
	int err = m_headingMgr->PollMotion();
	SceUInt64 now = sceRtcGetAccumulativeTime();
	unsigned int elapsed = (unsigned int)((now - m_headingMgr->m_lastPollTime) / 1000U);
	m_headingMgr->m_lastPollTime = now;
	m_headingMgr->m_posMgr->NotifyHeadingUpdate(err, elapsed);
	return 1;
}
