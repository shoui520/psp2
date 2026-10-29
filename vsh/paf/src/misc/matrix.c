/* Transpose a four-by-four matrix, permitting independent input and output. */
void ScePafMisc_34EF5545(float *out, const float *in)
{
	float matrix[4][4];
	unsigned row, col;

	for (row = 0; row < 4; row++)
		for (col = 0; col < 4; col++)
			matrix[row][col] = in[row * 4 + col];
	for (row = 0; row < 4; row++)
		for (col = 0; col < 4; col++)
			out[row * 4 + col] = matrix[col][row];
}
