/* Test whether a centre-and-extent rectangle contains another. */
typedef struct PafRectangle {
	float x;
	float y;
	float width;
	float height;
} PafRectangle;

int ScePafMisc_9DF81658(const PafRectangle *inside,
			const PafRectangle *outside)
{
	float inside_half_x = inside->width * 0.5f;
	float outside_half_x = outside->width * 0.5f;
	float inside_half_y = inside->height * 0.5f;
	float outside_half_y = outside->height * 0.5f;

	/* Preserve the native comparison branching, including unordered cases. */
	if (inside->x - inside_half_x < outside->x - outside_half_x)
		return 0;
	if (inside->x + inside_half_x > outside->x + outside_half_x)
		return 0;
	if (inside->y + inside_half_y > outside->y + outside_half_y)
		return 0;
	if (inside->y - inside_half_y < outside->y - outside_half_y)
		return 0;
	return 1;
}
