/* Shadow projection and orthogonal affine-matrix inverse. */
#include <stdint.h>

/* Build the plane-through-point shadow projection matrix, column-major. */
float *ScePafMisc_EE135486(float *result, const float *point,
			 const float *normal, const float *light)
{
	float plane[4];
	float first;
	float second;
	float scale;
	unsigned col, row;

	plane[0] = normal[0];
	plane[1] = normal[1];
	plane[2] = normal[2];
	first = point[0] * normal[0] + point[1] * normal[1];
	plane[3] = -(first + point[2] * normal[2]);
	first = light[0] * plane[0] + light[1] * plane[1];
	second = light[2] * plane[2] + light[3] * plane[3];
	scale = first + second;
	for (col = 0; col < 4; col++)
		for (row = 0; row < 4; row++)
			result[col * 4 + row] =
				(row == col ? scale : 0.0f) - light[row] * plane[col];
	return result;
}

/* The inverse of an orthonormal 3D affine transform. */
float *ScePafMisc_293092F5(float *result, const float *matrix)
{
	float columns[3][4];
	float translation[3];
	unsigned col, row;

	for (col = 0; col < 3; col++)
		for (row = 0; row < 3; row++)
			columns[col][row] = matrix[row * 4 + col];
	for (col = 0; col < 3; col++)
		columns[col][3] = 0.0f;
	for (col = 0; col < 3; col++)
		translation[col] = matrix[12 + col];
	for (col = 0; col < 3; col++)
		for (row = 0; row < 4; row++)
			result[col * 4 + row] = columns[col][row];
	for (row = 0; row < 4; row++) {
		float value = -(columns[0][row] * translation[0]);
		value -= columns[1][row] * translation[1];
		value -= columns[2][row] * translation[2];
		result[12 + row] = value + (row == 3 ? 1.0f : 0.0f);
	}
	return result;
}
