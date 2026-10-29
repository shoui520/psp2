/* Shared three-component vector state, originally held in libpaf BSS. */
#include <stdint.h>

float sce_paf_misc_global_vector[3];

void ScePafMisc_DC13D542(const float *source)
{
	float x, y, z;

	if (!source)
		return;
	x = source[0];
	y = source[1];
	z = source[2];
	sce_paf_misc_global_vector[0] = x;
	sce_paf_misc_global_vector[1] = y;
	sce_paf_misc_global_vector[2] = z;
}

void ScePafMisc_B4E6CA68(float *destination)
{
	float x, y, z;

	if (!destination)
		return;
	x = sce_paf_misc_global_vector[0];
	y = sce_paf_misc_global_vector[1];
	z = sce_paf_misc_global_vector[2];
	destination[0] = x;
	destination[1] = y;
	destination[2] = z;
}
