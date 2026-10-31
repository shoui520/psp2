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
