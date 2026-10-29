/* SceTouch, SceTouchDummy: which panel report is which finger from one sample to the next.
 * Private names without a recovered SDK name remain unknown.
 */
#include "private.h"

static void appendNode(struct touchNode **list, struct touchNode *node) /* name unknown */
{
	struct touchNode *tail;

	node->next = NULL;
	tail = *list;
	if (!tail) {
		*list = node;
		return;
	}
	while (tail->next)
		tail = tail->next;
	tail->next = node;
}

static struct touchNode *takeNode(struct touchNode **list) /* name unknown */
{
	struct touchNode *node = *list;

	*list = node->next;
	node->next = NULL;
	return node;
}

int initializeTracking(struct touchTracking *tracking) /* name unknown */
{
	struct touchNode *node;

	memset(tracking, 0, sizeof(*tracking));
	for (node = tracking->nodes; node != &tracking->nodes[8]; node++)
		appendNode(&tracking->free, node);
	return 0;
}

int updateTracking(struct touchTracking *tracking, const SceTouchData *data) /* name unknown */
{
	struct touchNode *node, *kept = NULL;
	unsigned char seen = 0;
	unsigned i;
	int left;

	if (tracking->added)
		moveNodes(&tracking->added, &tracking->active);
	if (!data->reportNum) {
		if (tracking->active)
			moveNodes(&tracking->active, &tracking->free);
		return 0;
	}
	left = data->reportNum;
	while (tracking->active) {
		node = takeNode(&tracking->active);
		for (i = 0; i < data->reportNum; i++) {
			if (data->report[i].id == node->id) {
				node->reportIndex = i;
				seen |= 1 << i;
				appendNode(&kept, node);
				left--;
				break;
			}
		}
		if (i == data->reportNum)
			appendNode(&tracking->free, node);
		if (!left) {
			moveNodes(&tracking->active, &tracking->free);
			tracking->active = kept;
			return 0;
		}
	}
	tracking->active = kept;
	if (left <= 0)
		return 0;
	for (i = 0; i < data->reportNum; i++) {
		if (!(((unsigned char)~seen >> i) & 1))
			continue;
		if (!tracking->free)
			return SCE_TOUCH_ERROR_FATAL;
		node = takeNode(&tracking->free);
		node->id = data->report[i].id;
		node->reportIndex = i;
		node->owner.pid = -1;
		node->owner.region = 0xffff;
		appendNode(&tracking->added, node);
		if (!--left)
			return 0;
	}
	return SCE_TOUCH_ERROR_FATAL;
}

int moveNodes(struct touchNode **from, struct touchNode **to) /* name unknown */
{
	struct touchNode *nodes = *from, *tail = *to;

	if (!tail) {
		*to = nodes;
	} else {
		while (tail->next)
			tail = tail->next;
		tail->next = nodes;
	}
	*from = NULL;
	return 0;
}
