/* Rectangle predicates on centre-and-extent geometry. */
#include <stdint.h>
typedef struct PafRectangle {
	float x;
	float y;
	float width;
	float height;
} PafRectangle;

static int ContainsPoint(const PafRectangle *rectangle, float x, float y)
{
	float half_width = rectangle->width * 0.5f;
	float half_height = rectangle->height * 0.5f;

	return rectangle->x - half_width <= x &&
		x <= rectangle->x + half_width &&
		rectangle->y + half_height >= y &&
		y >= rectangle->y - half_height;
}

int ScePafMisc_FFB654B3(const PafRectangle *rectangle, float x, float y)
{
	return ContainsPoint(rectangle, x, y);
}

int ScePafMisc_BC570EAF(const PafRectangle *rectangle, const float *point)
{
	return ContainsPoint(rectangle, point[0], point[1]);
}

int ScePafMisc_EF621B8C(const PafRectangle *left, const PafRectangle *right)
{
	float lhs_x_half = left->width * 0.5f;
	float rhs_x_half = right->width * 0.5f;
	float lhs_y_half = left->height * 0.5f;
	float rhs_y_half = right->height * 0.5f;

	/* The VFP branches in the original accept unordered comparisons. */
	if (left->x - lhs_x_half >= right->x + rhs_x_half)
		return 0;
	if (left->x + lhs_x_half <= right->x - rhs_x_half)
		return 0;
	if (left->y + lhs_y_half <= right->y - rhs_y_half)
		return 0;
	if (left->y - lhs_y_half >= right->y + rhs_y_half)
		return 0;
	return 1;
}

/* Intersect a requested integer rectangle with a cache surface viewport. */
int ScePafMisc_BDB5D7B2(const uint8_t *self, const int32_t *requested)
{
	int32_t *surface = *(int32_t *const *)(self + 44);
	int32_t left = requested[0] > 0 ? requested[0] : 0;
	int32_t top = requested[1] > 0 ? requested[1] : 0;
	int32_t right = requested[2] + requested[0];
	int32_t bottom = requested[3] + requested[1];
	int32_t width, height;

	if (surface[7] <= right)
		right = surface[7];
	width = right - left;
	if (surface[8] <= bottom)
		bottom = surface[8];
	height = bottom - top;
	if (width > 0 && height > 0) {
		surface[9] = left;
		surface[10] = top;
		surface[11] = width;
		surface[12] = height;
		return 1;
	}
	return 0;
}

int ScePafMisc_716D378A(const uint8_t *self, const int32_t *requested)
{
	int32_t *surface = *(int32_t *const *)(self + 44);
	int32_t mode = surface[17];
	int32_t left, top, right, bottom, width, height;

	if (mode == 0 || mode == 6 || mode == 7 || mode == 8)
		return 0;
	left = requested[0] > 0 ? requested[0] : 0;
	top = requested[1] > 0 ? requested[1] : 0;
	right = requested[2] + requested[0];
	bottom = requested[3] + requested[1];
	if (surface[5] <= right)
		right = surface[5];
	width = right - left;
	if (surface[6] <= bottom)
		bottom = surface[6];
	height = bottom - top;
	if (width > 0 && height > 0) {
		surface[13] = left;
		surface[14] = top;
		surface[15] = width;
		surface[16] = height;
		return 1;
	}
	return 0;
}

int ScePafMisc_02802435(const uint8_t *self, const uint8_t *candidate)
{
	const int32_t *a = (const int32_t *)self;
	const int32_t *b = (const int32_t *)candidate;

	if (!candidate)
		return 0;
	if (((b[10] > 0 && b[11] > 0) || (a[28] > 0 && a[29] > 0)) &&
	    (b[8] != a[26] || b[9] != a[27] ||
	     b[10] != a[28] || b[11] != a[29]))
		return 1;
	if (b[12] != a[30] || b[13] != a[31] ||
	    b[14] != a[32] || b[3] != a[14] ||
	    b[4] != a[15] || candidate[60] != self[132] ||
	    b[16] != a[34] || b[17] != a[35])
		return 1;
	return 0;
}
