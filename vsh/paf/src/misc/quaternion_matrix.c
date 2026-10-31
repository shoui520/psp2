/* Quaternion conversion. */
typedef struct PafQuaternion {
	float x;
	float y;
	float z;
	float w;
} PafQuaternion;

void ScePafMisc_2921E1B2(float *matrix, const PafQuaternion *q)
{
	float x = q->x, y = q->y, z = q->z, w = q->w;
	float norm = (x * x + y * y) + (z * z + w * w);
	float xx, yy, zz, xy, xz, yz, wx, wy, wz;
	float scale, xs, ys, zs;

	if (norm == 0.0f) {
		unsigned i;

		for (i = 0; i < 16; i++)
			matrix[i] = 0.0f;
		matrix[0] = 1.0f;
		matrix[5] = 1.0f;
		matrix[10] = 1.0f;
		matrix[15] = 1.0f;
		return;
	}
	scale = 2.0f / norm;
	xs = x * scale;
	ys = y * scale;
	zs = z * scale;
	xx = x * xs;
	yy = y * ys;
	zz = z * zs;
	xy = x * ys;
	xz = x * zs;
	yz = y * zs;
	wx = w * xs;
	wy = w * ys;
	wz = w * zs;

	matrix[0] = 1.0f - yy - zz;
	matrix[1] = xy + wz;
	matrix[2] = xz - wy;
	matrix[3] = 0.0f;
	matrix[4] = xy - wz;
	matrix[5] = 1.0f - xx - zz;
	matrix[6] = yz + wx;
	matrix[7] = 0.0f;
	matrix[8] = xz + wy;
	matrix[9] = yz - wx;
	matrix[10] = 1.0f - xx - yy;
	matrix[11] = 0.0f;
	matrix[12] = 0.0f;
	matrix[13] = 0.0f;
	matrix[14] = 0.0f;
	matrix[15] = 1.0f;
}
