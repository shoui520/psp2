/* Convert centre-and-extent rectangles into endpoint bounds. */
typedef struct PafRectangle {
	float x;
	float y;
	float width;
	float height;
} PafRectangle;

void ScePafMisc_3D34B4D1(float *bounds, const PafRectangle *rectangle)
{
	float half_width = rectangle->width * 0.5f;
	float half_height = rectangle->height * 0.5f;

	bounds[0] = rectangle->y + half_height;
	bounds[1] = rectangle->x - half_width;
	bounds[2] = rectangle->y - half_height;
	bounds[3] = rectangle->x + half_width;
}

void ScePafMisc_7A4AAFF2(float *bounds, const PafRectangle *rectangle)
{
	float half_width = rectangle->width * 0.5f;
	float half_height = rectangle->height * 0.5f;

	bounds[0] = rectangle->x - half_width;
	bounds[1] = rectangle->y + half_height;
	bounds[2] = rectangle->x + half_width;
	bounds[3] = rectangle->y - half_height;
}
