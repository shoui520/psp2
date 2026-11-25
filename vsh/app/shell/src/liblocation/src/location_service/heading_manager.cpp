/*
 * heading_manager.cpp: HeadingManager ("SceLocationHeadInternalLoc", SceMotion quaternion
 * SLERP smoothing, magnetic declination adjustment via IGRF-11, and heading angle/hysteresis
 * calculation; 0x81194E8C-0x81196BB8, sti 0x81196B0E). The file and class names are ours
 * (PLACEHOLDER).
 */
#include "location_service.h"
#include <math.h>
#include <regmgr/regmgr.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <system/system.h>

extern "C" {
SceUInt64 sceRtcGetAccumulativeTimeForUser(void);
float sce_paf_sinf(float x);
float sce_paf_cosf(float x);
float sce_paf_atan2f(float y, float x);
int SceMotion_ACEFA121(void);
int SceMotion_BC5C86C5(void);
int SceMotion_AD0B62B7(void *state);
int sceMotionMagnetometerOn(void);
int sceMotionMagnetometerOff(void);
}

extern void *g_locationManager;				/* 0x815496B4 */
extern void shellRegMgrLogError(int err, int flag);	/* 0x81345038 */
extern int locationTaskSchedule(void *task, unsigned int delayMs); /* 0x8118A780 */
extern int locationTaskCancel(void *task);		/* 0x8118A800 */
extern int locationMgrRegisterTask(void *mgr, void *task); /* 0x8118AACE */
extern int locationMgrUnregisterTask(void *mgr, void *task); /* 0x8118AB62 */
extern int locationPosMgrUpdateHeadingState(void *pm);	/* 0x8118C108 */
extern int locationPosMgrNotifyHeadingTimeout(void *pm, int err, unsigned int elapsed); /* 0x8118DAE4 */
extern float locationDecodeHeadingAngle(const SceUInt32 *hmethod); /* 0x8118F3CC */
extern int locationGetCurrentTick(SceRtcTick *tick);	/* 0x8118F49E */
extern void wmmSetDate(void *wmm, int month, int day, int year); /* 0x8119924C */
extern void wmmCalcDeclination(void *wmm, float lat, float lon, float alt); /* 0x8119934C */

struct Quat4 {
	float x;
	float y;
	float z;
	float w;
};

static float wrapDeg360(float deg)
{
	float r = deg - (float)(int)(deg / 360.0f) * 360.0f;
	if (r >= 360.0f)
		return (float)((double)r - 360.0);
	if (r < 0.0f)
		return (float)((double)r + 360.0);
	return r;
}

static Quat4 slerpHalf(const Quat4 &a, const Quat4 &b)
{
	float bx = b.x, by = b.y, bz = b.z, bw = b.w;
	float dot = a.x * bx + a.w * bw + a.y * by + a.z * bz;
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
		float s = sqrtf(1.0f - dot * dot);
		float theta = sce_paf_atan2f(s, dot);
		float invSin = 1.0f / s;
		wa = sce_paf_sinf(theta * 0.5f) * invSin;
		wb = sce_paf_sinf(theta * 0.5f) * invSin;
	}
	Quat4 out;
	out.x = wb * bx + a.x * wa;
	out.y = wb * by + a.y * wa;
	out.z = wb * bz + a.z * wa;
	out.w = wb * bw + a.w * wa;
	return out;
}

class HeadingManager;

class HeadingPollTask {
public:
	virtual ~HeadingPollTask() {}
	virtual int OnNotify();				/* 0x81196AC6 */

	int m_queued;					/* +4 */
	int m_active;					/* +8 */
	HeadingManager *m_mgr;				/* +12 */
};

class HeadingSusListener {
public:
	virtual void OnResume() = 0;			/* +0 */
	virtual void OnSuspend() = 0;			/* +4 */
	virtual ~HeadingSusListener() {}
};

class HeadingManager : public HeadingSusListener {
public:
	HeadingManager(void *posMgr);			/* 0x81194E8C */
	virtual ~HeadingManager();			/* 0x81194F9E / 0x81195032 */
	virtual void OnResume();			/* 0x81196AB0 */
	virtual void OnSuspend();			/* 0x81196A9C */
	virtual void OnHeadingConfigChanged();		/* 0x81196A90 */

	int StopMotion();				/* 0x81194F14 */
	int IsMotionStarted();				/* 0x81194F34 */
	int Term();					/* 0x81194F38 */
	int LoadLastLocation();				/* 0x81195042 */
	int Init();					/* 0x81195164 */
	int StartMotion();				/* 0x811951BE */
	int EnableMagnetometer();			/* 0x811951E0 */
	int DisableMagnetometer();			/* 0x81195202 */
	int PollMotion();				/* 0x81195226 */
	int ComputeHeadingAngle(const float *vec2, float *outHeading, const SceUInt32 *hmethod,
				float prevHeading);	/* 0x81195AFC */
	int RotateBasisVector(const Quat4 *q, const SceUInt32 *hmethod, float *outVec4); /* 0x81196386 */
	int GetHeading(const SceUInt32 *hmethod, SceLocationHeadingInfo *outInfo); /* 0x811964BE */
	int StartPolling();				/* 0x8119685E */
	int StopPolling();				/* 0x8119688A */
	int IsMagnetometerEnabled();			/* 0x811968B2 */
	int SaveLastLocation();				/* 0x811968B6 */
	void UpdateLocation(const double *latLon);	/* 0x811969CA */

	void *m_posMgr;					/* +8 */
	unsigned char m_initialized;			/* +12 */
	signed char m_pollRefCount;			/* +13 */
	unsigned char m_motionStarted;			/* +14 */
	unsigned char m_magEnabled;			/* +15 */
	double m_lastLat;				/* +16 */
	double m_lastLon;				/* +24 */
	int m_mutexResult;				/* +32 */
	int m_pad36;					/* +36 */
	SceKernelLwMutexWork m_mutex;			/* +40..+71 */
	int m_orientation;				/* +72 */
	char m_wmmState[784];				/* +76..+859 */
	float m_declination;				/* +860 */
	char m_wmmTail[8];				/* +864..+871 */
	unsigned char m_motionState[248];		/* +872..+1119 */
	Quat4 m_curQuat;				/* +1120..+1135 */
	int m_valid;					/* +1136 */
	Quat4 m_smoothQuat;				/* +1140..+1155 */
	Quat4 m_ring[9];				/* +1156..+1299 */
	unsigned int m_ringHead;			/* +1300 */
	unsigned int m_ringTail;			/* +1304 */
	unsigned int m_ringLast;			/* +1308 */
	SceUInt64 m_lastPollTime;			/* +1312 */
	HeadingPollTask m_pollTask;			/* +1320..+1335 */
};

/* 0x81194E8C */
HeadingManager::HeadingManager(void *posMgr)
	: m_posMgr(posMgr)
	, m_initialized(0)
	, m_pollRefCount(0)
	, m_motionStarted(0)
	, m_magEnabled(0)
	, m_lastLat(0.0)
	, m_lastLon(0.0)
	, m_mutexResult(-1)
	, m_orientation(0)
	, m_declination(0.0f)
	, m_valid(0)
	, m_ringHead(0)
	, m_ringTail(0)
	, m_ringLast(0)
	, m_lastPollTime(0)
{
	m_curQuat.x = 0.0f;
	m_curQuat.y = 0.0f;
	m_curQuat.z = 0.0f;
	m_curQuat.w = 1.0f;
	m_pollTask.m_queued = 0;
	m_pollTask.m_active = 0;
	m_pollTask.m_mgr = this;
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
		locationMgrUnregisterTask(g_locationManager, &m_pollTask);
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
		wmmSetDate(m_wmmState, dt.month, dt.day, dt.year);
		wmmCalcDeclination(m_wmmState, (float)m_lastLat, (float)m_lastLon, 0.0f);
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

	int ret = locationMgrRegisterTask(g_locationManager, &m_pollTask);
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

	int ret = SceMotion_AD0B62B7(m_motionState);
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

	const float *m = (const float *)(m_motionState + 120);
	float m00 = m[0], m01 = m[1], m02 = m[2];
	float m10 = m[4], m11 = m[5], m12 = m[6];
	float m20 = m[8], m21 = m[9], m22 = m[10];

	float d0 = (m00 - m11 - m22) + 1.0f;
	float d1 = (m11 - m00 - m22) + 1.0f;
	float d2 = (-m00 - m11 + m22) + 1.0f;
	float d3 = (m00 + m11 + m22) + 1.0f;

	unsigned int idx = 0;
	float best = d0;
	if (d1 > best) { idx = 1; best = d1; }
	if (d2 > best) { idx = 2; best = d2; }
	if (d3 > best) { idx = 3; best = d3; }

	float s = (float)(sqrtf(best) * 0.5);
	float inv = 0.25f / s;
	Quat4 q;
	if (idx == 0) {
		q.x = s;
		q.y = (m10 + m01) * inv;
		q.z = (m20 + m02) * inv;
		q.w = (m21 - m12) * inv;
	} else if (idx == 1) {
		q.x = (m10 + m01) * inv;
		q.y = s;
		q.z = (m21 + m12) * inv;
		q.w = (m02 - m20) * inv;
	} else if (idx == 2) {
		q.x = (m20 + m02) * inv;
		q.y = (m21 + m12) * inv;
		q.z = s;
		q.w = (m10 - m01) * inv;
	} else {
		q.x = (m21 - m12) * inv;
		q.y = (m02 - m20) * inv;
		q.z = (m10 - m01) * inv;
		q.w = s;
	}

	int isValid = 0;
	if (m_motionState[245] != 1) {
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
	Quat4 lvl1[4];
	for (unsigned int i = 0; i < 4; i++) {
		unsigned int o0 = 2 * i;
		unsigned int o1 = 2 * i + 1;
		if (o0 >= count) o0 = count - 1;
		if (o1 >= count) o1 = count - 1;
		unsigned int i0 = (m_ringHead + o0) % 9;
		unsigned int i1 = (m_ringHead + o1) % 9;
		lvl1[i] = slerpHalf(m_ring[i0], m_ring[i1]);
	}
	Quat4 lvl2_0 = slerpHalf(lvl1[0], lvl1[1]);
	Quat4 lvl2_1 = slerpHalf(lvl1[2], lvl1[3]);
	m_smoothQuat = slerpHalf(lvl2_0, lvl2_1);
	return ret;
}

/* 0x81195AFC */
int HeadingManager::ComputeHeadingAngle(const float *vec2, float *outHeading,
					const SceUInt32 *hmethod, float prevHeading)
{
	float rawDeg = (float)((double)sce_paf_atan2f(vec2[1], vec2[0]) * 180.0 / 3.141592653589793);
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
int HeadingManager::RotateBasisVector(const Quat4 *q, const SceUInt32 *hmethod, float *outVec4)
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

	float qx = q->x, qy = q->y, qz = q->z, qw = q->w;
	float tx = qy * vz - qz * vy + qw * vx;
	float ty = qz * vx - qx * vz + qw * vy;
	float tz = qx * vy - qy * vx + qw * vz;
	float tw = -(qx * vx) - (qy * vy) - (qz * vz);

	outVec4[0] = (tz * qy - ty * qz) + tx * qw - tw * qx;
	outVec4[1] = (tx * qz - tz * qx) + ty * qw - tw * qy;
	outVec4[2] = (ty * qx - tx * qy) + tz * qw - tw * qz;
	outVec4[3] = 0.0f;
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
	Quat4 q = ((method & 0x1000) != 0) ? m_smoothQuat : m_curQuat;

	if ((int)method >= 0) {
		float rad = (float)((double)m_declination * 3.14159265 / 180.0);
		float c = sce_paf_cosf(rad);
		float s = sce_paf_sinf(rad);
		float d0 = (c - c - 1.0f) + 1.0f;
		float d1 = (c - c - 1.0f) + 1.0f;
		float d2 = (-c - c + 1.0f) + 1.0f;
		float d3 = (c + c + 1.0f) + 1.0f;
		unsigned int idx = 0;
		float best = d0;
		if (d1 > best) { idx = 1; best = d1; }
		if (d2 > best) { idx = 2; best = d2; }
		if (d3 > best) { idx = 3; best = d3; }
		float qs = (float)(sqrtf(best) * 0.5);
		float inv = 0.25f / qs;
		Quat4 rot = { 0.0f, 0.0f, 0.0f, 1.0f };
		if (idx == 0) { rot.x = qs; rot.y = (s - s) * inv; }
		else if (idx == 1) { rot.x = (s - s) * inv; rot.y = qs; }
		else if (idx == 2) { rot.z = qs; rot.w = (s + s) * inv; }
		else { rot.z = (s + s) * inv; rot.w = qs; }

		Quat4 r;
		r.x = rot.x * q.w + rot.w * q.x + rot.y * q.z - rot.z * q.y;
		r.y = rot.y * q.w + rot.w * q.y + rot.z * q.x - rot.x * q.z;
		r.z = rot.z * q.w + rot.w * q.z + rot.x * q.y - rot.y * q.x;
		r.w = rot.w * q.w - rot.x * q.x - rot.y * q.y - rot.z * q.z;
		q = r;
	}

	float gx = *(const float *)(m_motionState + 188);
	float gy = *(const float *)(m_motionState + 192);
	if (gy != 0.0f)
		m_orientation = 0;
	else if (gx != 0.0f)
		m_orientation = 1;

	SceUInt32 effMethod = method;
	if ((method & 7) == 1)
		effMethod = (method & ~7U) | ((m_orientation == 0) ? 3U : 2U);

	float vec4[4];
	RotateBasisVector(&q, &effMethod, vec4);
	float norm = sqrtf(vec4[0] * vec4[0] + vec4[1] * vec4[1]);
	float vec2[2] = { vec4[0] / norm, vec4[1] / norm };
	ComputeHeadingAngle(vec2, &outInfo->heading, &effMethod, 0.0f);

	outInfo->nedQuat.x = q.x;
	outInfo->nedQuat.y = q.y;
	outInfo->nedQuat.z = q.z;
	outInfo->nedQuat.w = q.w;
	outInfo->stability = m_motionState[244];
	locationGetCurrentTick(&outInfo->timestamp);
	return 0;
}

/* 0x8119685E */
int HeadingManager::StartPolling()
{
	if (m_pollRefCount == 0) {
		m_lastPollTime = sceRtcGetAccumulativeTimeForUser();
		locationTaskSchedule(&m_pollTask, 0);
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
			locationTaskCancel(&m_pollTask);
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
			wmmSetDate(m_wmmState, dt.month, dt.day, dt.year);
			wmmCalcDeclination(m_wmmState, (float)m_lastLat, (float)m_lastLon, 0.0f);
		}
	}
}

/* 0x81196A90 */
void HeadingManager::OnHeadingConfigChanged()
{
	locationPosMgrUpdateHeadingState(m_posMgr);
}

/* 0x81196A9C */
void HeadingManager::OnSuspend()
{
	if (m_pollRefCount > 0)
		locationTaskCancel(&m_pollTask);
}

/* 0x81196AB0 */
void HeadingManager::OnResume()
{
	if (m_pollRefCount > 0)
		locationTaskSchedule(&m_pollTask, 0);
}

/* 0x81196AC6 */
int HeadingPollTask::OnNotify()
{
	int err = m_mgr->PollMotion();
	SceUInt64 now = sceRtcGetAccumulativeTimeForUser();
	unsigned int elapsed = (unsigned int)((now - m_mgr->m_lastPollTime) / 1000U);
	m_mgr->m_lastPollTime = now;
	locationPosMgrNotifyHeadingTimeout(m_mgr->m_posMgr, err, elapsed);
	return 1;
}
