/* Transform a rectangle using independent position and scale vectors. */
typedef struct PafRectangle {
	float x;
	float y;
	float width;
	float height;
} PafRectangle;

void ScePafMisc_362D3890(PafRectangle *destination,
			  const PafRectangle *rectangle, const float *origin,
			  float *direction, const float *scale)
{
	float signX = direction[0] < 0.0f ? -1.0f : 1.0f;
	float signY = direction[1] < 0.0f ? -1.0f : 1.0f;
	float width = rectangle->width * scale[0];
	float height = rectangle->height * scale[1];
	float x = (rectangle->x - origin[0]) * scale[0];
	float y = (rectangle->y - origin[1]) * scale[1];

	direction[0] = signX;
	direction[1] = signY;
	destination->x = x * signX;
	destination->y = y * signY;
	destination->width = width >= 0.0f ? width : 0.0f;
	destination->height = height >= 0.0f ? height : 0.0f;
}
