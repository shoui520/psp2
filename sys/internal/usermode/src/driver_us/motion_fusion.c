/*
 * SceDriverUser: SceMotion orientation filter.
 *
 * The orientation quaternion is integrated from the gyro, pulled towards
 * gravity by the accelerometer (tilt correction), turned about the vertical
 * on request, and summarised as a basic orientation and a north/east/down
 * frame from the magnetometer. Vector arithmetic follows the library's NEON
 * vector math: reciprocal square roots are the estimate refined by two
 * Newton-Raphson steps, and sums are formed in the same order.
 */

#include <arm_neon.h>
#include "driver_user.h"
#include "motion.h"

#define MOTION_PI	3.1415926535	/* as Sony wrote it, short of M_PI */

static const SceFVector3 s_motionDown = { 0.0f, -1.0f, 0.0f };

/* a * b, the library's term order */
static inline SceFQuaternion QuatMul(const SceFQuaternion *a, const SceFQuaternion *b)
{
	SceFQuaternion r;

	r.x = ((a->w * b->x + a->x * b->w) + a->y * b->z) - a->z * b->y;
	r.y = ((a->w * b->y + a->y * b->w) + a->z * b->x) - a->x * b->z;
	r.z = ((a->w * b->z + a->z * b->w) + a->x * b->y) - a->y * b->x;
	r.w = (a->w * b->w - a->z * b->z) - (a->y * b->y + a->x * b->x);
	return r;
}

/* sine and cosine about the nearest quadrant, NaN for infinities */
static inline void SinCos(float angle, float *sine, float *cosine)
{
	float32x2_t x = vdup_n_f32(angle);
	uint32x2_t bits = vreinterpret_u32_f32(x);
	float32x2_t half = vreinterpret_f32_u32(vorr_u32(vand_u32(bits, vdup_n_u32(0x80000000)),
							  vdup_n_u32(0x3f000000)));
	int32x2_t q = vcvt_s32_f32(vadd_f32(vmul_f32(x, vdup_n_f32(0x1.45f306p-1f)), half));
	float32x2_t qf = vcvt_f32_s32(q);
	float32x2_t r = vsub_f32(vsub_f32(x, vmul_f32(qf, vdup_n_f32(0x1.921fb4p+0f))),
				 vmul_f32(qf, vdup_n_f32(0x1.4442d0p-24f)));
	float32x2_t r2 = vmul_f32(r, r);
	float32x2_t r3 = vmul_f32(r2, r);
	float32x2_t sp = vadd_f32(vdup_n_f32(0x1.110684p-7f), vmul_f32(vdup_n_f32(-0x1.9918dcp-13f), r2));
	float32x2_t cp = vadd_f32(vdup_n_f32(0x1.55406cp-5f), vmul_f32(vdup_n_f32(-0x1.649326p-10f), r2));
	float32x2_t sx = vadd_f32(r, vmul_f32(vadd_f32(vdup_n_f32(-0x1.555542p-3f), vmul_f32(sp, r2)), r3));
	float32x2_t cx = vadd_f32(vdup_n_f32(1.0f),
				  vmul_f32(vadd_f32(vdup_n_f32(-0x1.ffffbep-2f), vmul_f32(cp, r2)), r2));
	int32x2_t offsetSin = vand_s32(q, vdup_n_s32(3));
	int32x2_t offsetCos = vadd_s32(vdup_n_s32(1), offsetSin);
	uint32x2_t sinEven = vceq_s32(vand_s32(offsetSin, vdup_n_s32(1)), vdup_n_s32(0));
	uint32x2_t cosEven = vceq_s32(vand_s32(offsetCos, vdup_n_s32(1)), vdup_n_s32(0));
	float32x2_t s = vbsl_f32(sinEven, sx, cx);
	float32x2_t c = vbsl_f32(cosEven, sx, cx);
	uint32x2_t sinPositive = vceq_s32(vand_s32(offsetSin, vdup_n_s32(2)), vdup_n_s32(0));
	uint32x2_t cosPositive = vceq_s32(vand_s32(offsetCos, vdup_n_s32(2)), vdup_n_s32(0));
	uint32x2_t infinite = vceq_u32(vand_u32(bits, vdup_n_u32(0x7fffffff)), vdup_n_u32(FLT_INF_BITS));

	s = vbsl_f32(sinPositive, s, vneg_f32(s));
	c = vbsl_f32(cosPositive, c, vneg_f32(c));
	s = vbsl_f32(vceq_f32(x, vdup_n_f32(0.0f)), x, s);
	s = vreinterpret_f32_u32(vorr_u32(infinite, vreinterpret_u32_f32(s)));
	c = vreinterpret_f32_u32(vorr_u32(infinite, vreinterpret_u32_f32(c)));
	*sine = vget_lane_f32(s, 0);
	*cosine = vget_lane_f32(c, 0);
}

/* gyro axes whose average over the first five states is below 1 degree/s read as still */
int MotionGyroDeadband(MotionEntry *entries, MotionEntry *entry)
{
	float32x4_t five = vdupq_n_f32(5.0f);
	float32x4_t reciprocal = vrecpeq_f32(five);
	float32x4_t sum = vaddq_f32(vdupq_n_f32(0.0f), vld1q_f32(&entries[0].gyro.x));
	float32x4_t average;
	SceFVector4 gyro;

	reciprocal = vmulq_f32(reciprocal, vrecpsq_f32(reciprocal, five));
	sum = vaddq_f32(sum, vld1q_f32(&entries[1].gyro.x));
	sum = vaddq_f32(sum, vld1q_f32(&entries[2].gyro.x));
	sum = vaddq_f32(sum, vld1q_f32(&entries[3].gyro.x));
	reciprocal = vmulq_f32(reciprocal, vrecpsq_f32(reciprocal, five));
	sum = vaddq_f32(sum, vld1q_f32(&entries[4].gyro.x));
	average = vmulq_f32(sum, reciprocal);
	entry->gyroStill[0] = (double)vgetq_lane_f32(average, 0) < 0.0175 &&
			      (double)vgetq_lane_f32(average, 0) > -0.0175;
	entry->gyroStill[1] = (double)vgetq_lane_f32(average, 1) < 0.0175 &&
			      (double)vgetq_lane_f32(average, 1) > -0.0175;
	entry->gyroStill[2] = (double)vgetq_lane_f32(average, 2) < 0.0175 &&
			      (double)vgetq_lane_f32(average, 2) > -0.0175;
	gyro.x = entry->gyroStill[0] ? 0.0f : entry->gyro.x;
	gyro.y = entry->gyroStill[1] ? 0.0f : entry->gyro.y;
	gyro.z = entry->gyroStill[2] ? 0.0f : entry->gyro.z;
	gyro.w = 0.0f;
	entry->gyro = gyro;
	return 0;
}

/* turns the orientation a twentieth of the way towards what gravity says */
int MotionTiltCorrect(SceFQuaternion *quat, const SceFVector4 *acc)
{
	const SceFVector3 *down = &s_motionDown;
	SceFQuaternion q = *quat, correction;
	float length, deviation, tx, ty, tz, tw, rx, ry, rz, scale, cx, cy, cz, cross, dot, half, sine;

	length = SafeSqrt(Dot3(acc->x, acc->y, acc->z));
	deviation = length - 1.0f;
	if (deviation != 0.0f) {
		if (deviation <= 0.0f)
			deviation = -deviation;
		if (!(deviation < 0.08f))
			return 0;
	}
	/* gravity in the reference frame */
	tx = (q.w * acc->x + q.y * acc->z) - q.z * acc->y;
	ty = (q.w * acc->y + q.z * acc->x) - q.x * acc->z;
	tz = (q.w * acc->z + q.x * acc->y) - q.y * acc->x;
	tw = q.z * acc->z + (q.x * acc->x + q.y * acc->y);
	rx = ((tw * q.x + q.w * tx) + q.y * tz) - q.z * ty;
	ry = ((tw * q.y + q.w * ty) + q.z * tx) - q.x * tz;
	rz = ((tw * q.z + q.w * tz) + q.x * ty) - q.y * tx;
	scale = RsqrtNR(Dot3(rx, ry, rz));
	rx *= scale;
	ry *= scale;
	rz *= scale;
	cx = ry * down->z - rz * down->y;
	cy = rz * down->x - rx * down->z;
	cz = rx * down->y - ry * down->x;
	cross = SafeSqrt(Dot3(cx, cy, cz));
	if (!(cross > 0.01f))
		return 0;
	dot = (rx * down->x + ry * down->y) + rz * down->z;
	half = MotionAtan2(cross, dot) * 0.05f * 0.5f;
	sine = MotionSin(half) / cross;
	correction.x = sine * cx;
	correction.y = sine * cy;
	correction.z = sine * cz;
	correction.w = MotionCos(half);
	*quat = QuatMul(&correction, &q);
	return 0;
}

/* q + dq/dt * dt with dq/dt = q * w / 2, normalised */
MOTION_VFP int MotionIntegrate(const SceFQuaternion *quat, const SceFVector4 *gyro,
			       SceFQuaternion *out, float dt)
{
	SceFQuaternion half = { quat->x * 0.5f, quat->y * 0.5f, quat->z * 0.5f, quat->w * 0.5f };
	SceFQuaternion rate = { gyro->x, gyro->y, gyro->z, 0.0f };
	SceFQuaternion dq = QuatMul(&half, &rate);
	float x = quat->x + dq.x * dt;
	float y = quat->y + dq.y * dt;
	float z = quat->z + dq.z * dt;
	float w = quat->w + dq.w * dt;
	float scale = RsqrtNR((x * x + y * y) + (z * z + w * w));

	out->x = x * scale;
	out->y = y * scale;
	out->z = z * scale;
	out->w = w * scale;
	return 0;
}

MOTION_VFP int MotionRotateEntryYaw(MotionEntry *entry, float yaw)
{
	SceFQuaternion rotation = { 0.0f, 0.0f, 0.0f, 0.0f };

	SinCos(yaw * 0.5f, &rotation.y, &rotation.w);
	entry->quat = QuatMul(&rotation, &entry->quat);
	return 0;
}

/* down from gravity, east = down x magnetic field, north = east x down */
int MotionUpdateNed(MotionEntry *entry)
{
	const SceFVector4 *acc = &entry->acc;
	const SceFVector4 *magn = &entry->magn;
	float field = RsqrtNR(Dot3(magn->x, magn->y, magn->z));
	float gravity = RsqrtNR(Dot3(acc->x, acc->y, acc->z));
	float dx = acc->x * gravity, dy = acc->y * gravity, dz = acc->z * gravity;
	float mx = magn->x * field, my = magn->y * field, mz = magn->z * field;
	float ex, ey, ez, nx, ny, nz, scale;

	entry->ned[2].x = dx;
	entry->ned[2].y = dy;
	entry->ned[2].z = dz;
	entry->ned[2].w = acc->w * gravity;
	ex = dy * mz - dz * my;
	ey = dz * mx - dx * mz;
	ez = dx * my - dy * mx;
	scale = RsqrtNR(Dot3(ex, ey, ez));
	ex *= scale;
	ey *= scale;
	ez *= scale;
	entry->ned[1].x = ex;
	entry->ned[1].y = ey;
	entry->ned[1].z = ez;
	entry->ned[1].w = ex;
	nx = ey * dz - ez * dy;
	ny = ez * dx - ex * dz;
	nz = ex * dy - ey * dx;
	scale = RsqrtNR(Dot3(nx, ny, nz));
	nx *= scale;
	entry->ned[0].x = nx;
	entry->ned[0].y = ny * scale;
	entry->ned[0].z = nz * scale;
	entry->ned[0].w = nx;
	return 0;
}

static inline float Degrees(float radians)
{
	return (float)((double)(radians * 180.0f) / MOTION_PI);
}

/* |angle - 90|, with exactly 90 giving +0 */
static inline float FromVertical(float degrees)
{
	float offset = degrees - 90.0f;

	if (offset == 0.0f)
		return 0.0f;
	if (offset <= 0.0f)
		offset = -offset;
	return offset;
}

static inline void SetBasicOrientation(MotionEntry *entry, float x, float y, float z)
{
	SceFVector4 orientation = { x, y, z, 0.0f };

	entry->basicOrientation = orientation;
}

/*
 * Face up or down when the screen normal is within flatThreshold of the
 * vertical; otherwise the edge that points down, with hysteresis kept in
 * g_motion.orientX/Z.
 */
MOTION_VFP int MotionUpdateBasicOrientation(MotionEntry *entry, float standThreshold,
					    float flatThreshold, float sideThreshold)
{
	const SceFVector4 *acc = &entry->acc;
	float length, scale, nx, ny, degrees, tilt, side, upright;
	int vertical;

	length = SafeSqrt(Dot3(acc->x, acc->y, acc->z));
	if (length != 0.0f) {
		if (length <= 0.0f)
			length = -length;
		if (!(length - 1.0f < 0.08f))
			return 0;
	}
	scale = RsqrtNR(Dot3(acc->x, acc->y, acc->z));
	degrees = Degrees(MotionAcos(acc->z * scale));
	if (degrees <= flatThreshold) {
		g_motion.orientX = 0;
		g_motion.orientZ = -1;
		SetBasicOrientation(entry, 0.0f, 0.0f, -1.0f);
		return 0;
	}
	if (degrees >= 180.0f - flatThreshold) {
		g_motion.orientX = 0;
		g_motion.orientZ = 1;
		SetBasicOrientation(entry, 0.0f, 0.0f, 1.0f);
		return 0;
	}
	tilt = FromVertical(degrees);
	if (standThreshold >= tilt) {
		vertical = g_motion.orientZ == 0 ? -1 : 0;
		if (g_motion.orientZ != 0)
			g_motion.orientX = 0;
		g_motion.orientZ = 0;
	} else {
		vertical = -1;
		if (g_motion.orientZ != 0) {
			SetBasicOrientation(entry, (float)g_motion.orientX, -1.0f, (float)g_motion.orientZ);
			return 0;
		}
	}
	scale = RsqrtNR(Dot3(acc->x, acc->y, 0.0f));
	nx = acc->x * scale;
	ny = acc->y * scale;
	degrees = Degrees(MotionAcos(nx));
	upright = MotionAcos(ny);
	side = FromVertical(degrees);
	if ((vertical | g_motion.orientX) != 0 && !(sideThreshold >= side) &&
	    !(90.0f - sideThreshold <= side)) {
		SetBasicOrientation(entry, (float)g_motion.orientX, (float)vertical,
				    (float)g_motion.orientZ);
		return 0;
	}
	if (side <= 45.0f) {
		g_motion.orientX = 0;
		g_motion.orientZ = 0;
		SetBasicOrientation(entry, 0.0f, Degrees(upright) < 90.0f ? -1.0f : 1.0f, 0.0f);
		return 0;
	}
	g_motion.orientX = degrees < 90.0f ? -1 : 1;
	g_motion.orientZ = 0;
	SetBasicOrientation(entry, (float)g_motion.orientX, 0.0f, 0.0f);
	return 0;
}

/* runs the filter on state index, prev being the state before it */
int MotionUpdateEntry(SceUInt32 prev, SceUInt32 index)
{
	const MotionConfig *config = &g_motion.config;
	MotionEntry *entry = &g_motion.entries[index];
	int ret;

	if (config->deadband != 0 && entry->gyroNew != 0) {
		ret = MotionGyroDeadband(g_motion.entries, entry);
		if (ret < 0)
			return ret;
	}
	if (entry->gyroNew != 0 && g_motion.count > 1) {
		entry->gyroNew = 0;
		ret = MotionIntegrate(&g_motion.entries[prev].quat, &entry->gyro, &entry->quat,
				      (float)((double)entry->interval * 0.001));
		if (ret < 0)
			return ret;
	}
	if (entry->accNew == 0)
		return 0;
	if (entry->magnNew != 0) {
		entry->magnNew = 0;
		if (config->magnetometer != 0) {
			ret = MotionUpdateNed(entry);
			if (ret < 0)
				return ret;
		} else {
			entry->ned[1] = (SceFVector4){ 0.0f, 1.0f, 0.0f, 0.0f };
			entry->ned[0] = (SceFVector4){ 1.0f, 0.0f, 0.0f, 0.0f };
			entry->ned[2] = (SceFVector4){ 0.0f, 0.0f, 1.0f, 0.0f };
		}
		if (config->tiltCorrection == 0) {
			if (entry->accNew == 0)
				return 0;
			goto orientation;
		}
		if (entry->accNew == 0)
			return 0;
		if (g_motion.count <= 1)
			goto orientation;
	} else if (config->tiltCorrection == 0 || g_motion.count <= 1) {
		goto orientation;
	}
	ret = MotionTiltCorrect(&entry->quat, &entry->acc);
	if (ret < 0)
		return ret;
	if (entry->accNew == 0)
		return 0;
orientation:
	entry->accNew = 0;
	ret = MotionUpdateBasicOrientation(entry, config->angleThreshold[0], config->angleThreshold[1],
					   config->angleThreshold[2]);
	return ret < 0 ? ret : 0;
}

/*
 * name unknown: basic orientation of an extra controller from its gravity
 * vector, thresholds {stand, flat, side} and its previous orientation.
 */
int SceMotion_A408BC69(const float *threshold, const SceFVector3 *acc, SceFVector3 *out,
		       const SceFVector3 *prev)
{
	float x, y, z, length, scale, degrees, tilt, side, upright;
	int reset;

	if (threshold == NULL || acc == NULL || out == NULL || prev == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	x = prev->x;
	y = prev->y;
	z = prev->z;
	length = SafeSqrt(Dot3(acc->x, acc->y, acc->z));
	if (length != 0.0f) {
		if (length <= 0.0f)
			length = -length;
		if (!(length - 1.0f < 0.08f))
			goto done;
	}
	scale = RsqrtNR(Dot3(acc->x, acc->y, acc->z));
	degrees = Degrees(MotionAcos(acc->z * scale));
	if (threshold[1] >= degrees) {
		x = 0.0f;
		y = 0.0f;
		z = -1.0f;
		goto done;
	}
	if (degrees >= 180.0f - threshold[1]) {
		x = 0.0f;
		y = 0.0f;
		z = 1.0f;
		goto done;
	}
	tilt = FromVertical(degrees);
	if (!(90.0f - threshold[0] >= tilt)) {
		if (z != 0.0f)
			goto done;
		reset = y == 0.0f && x == 0.0f;
	} else if (z == 0.0f) {
		reset = y == 0.0f && x == 0.0f;
		z = 0.0f;
	} else {
		reset = 1;
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}
	scale = RsqrtNR(Dot3(acc->x, acc->y, 0.0f));
	degrees = Degrees(MotionAcos(acc->x * scale));
	upright = MotionAcos(acc->y * scale);
	side = FromVertical(degrees);
	if (!reset && !(threshold[2] >= side) && !(90.0f - threshold[2] <= side))
		goto done;
	if (!(side <= 45.0f)) {
		x = degrees < 90.0f ? -1.0f : 1.0f;
		y = 0.0f;
		z = 0.0f;
	} else {
		x = 0.0f;
		y = Degrees(upright) < 90.0f ? -1.0f : 1.0f;
		z = 0.0f;
	}
done:
	out->x = x;
	out->y = y;
	out->z = z;
	return 0;
}
