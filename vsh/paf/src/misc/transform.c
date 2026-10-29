/* Orthographic and perspective homogeneous transform constructors. */
void ScePafMisc_1534ACB0(float *matrix, float left, float right,
			 float bottom, float top, float far, float near)
{
	float width = right - left;
	float height = top - bottom;
	float depth = far - near;
	unsigned i;

	for (i = 0; i < 16; i++)
		matrix[i] = 0.0f;
	matrix[0] = 2.0f / width;
	matrix[5] = 2.0f / height;
	matrix[10] = 2.0f / depth;
	matrix[12] = -(left + right) / width;
	matrix[13] = -(bottom + top) / height;
	matrix[14] = (near + far) / depth;
	matrix[15] = 1.0f;
}

void ScePafMisc_F70087F7(float *matrix, float left, float right,
			 float bottom, float top, float far, float near)
{
	float width = right - left;
	float height = top - bottom;
	float depth = near - far;
	unsigned i;

	for (i = 0; i < 16; i++)
		matrix[i] = 0.0f;
	matrix[0] = 2.0f * far / width;
	matrix[5] = 2.0f * far / height;
	matrix[8] = (left + right) / width;
	matrix[9] = (bottom + top) / height;
	matrix[10] = -(near + far) / depth;
	matrix[11] = -1.0f;
	matrix[14] = -2.0f * near * far / depth;
}

void ScePafMisc_66CB67D5(float *matrix, const float *minimum,
			 const float *maximum)
{
	ScePafMisc_1534ACB0(matrix, minimum[0], maximum[0],
			    minimum[1], maximum[1], minimum[2], maximum[2]);
}
