/* Object-chain checks and indexed tree traversal. */
#include <stdint.h>

typedef struct PafChainClass {
	uint32_t reserved[4];
	uint32_t flags;
} PafChainClass;

typedef struct PafChainPayload {
	uint32_t reserved;
	PafChainClass *type;
} PafChainPayload;

typedef struct PafChainNode {
	struct PafChainNode *previous;
	struct PafChainNode *next;
	PafChainPayload *payload;
} PafChainNode;

typedef struct PafChain {
	uint32_t reserved;
	PafChainNode *head;
} PafChain;

int ScePafMisc_07A5D381(const PafChain *chain)
{
	PafChainNode *head = chain->head;
	PafChainNode *node = head ? head->next : 0;

	while (node != head) {
		PafChainPayload *payload = node->payload;

		if (payload && payload->type) {
			uint32_t flags = payload->type->flags;
			uint32_t kind = flags & 0xf000U;

			if (kind != 0x3000U &&
			    !(kind == 0x2000U && (flags & 0xf00U) == 0))
				return 0;
		}
		node = node->next;
	}
	return 1;
}

typedef struct PafSearchNode {
	struct PafSearchNode *parent;
	struct PafSearchNode *left;
	struct PafSearchNode *right;
	int32_t key;
	void *result;
	uint8_t reserved;
	int8_t isNil;
} PafSearchNode;

typedef struct PafSearchTree {
	uint32_t reserved[4];
	PafSearchNode *sentinel;
} PafSearchTree;

void *ScePafMisc_14432321(const PafSearchTree *const *owner, int32_t key)
{
	PafSearchNode *sentinel = (*owner)->sentinel;
	PafSearchNode *node;

	if (!sentinel)
		return 0;
	node = sentinel->parent;
	while (!node->isNil) {
		if (key < node->key)
			node = node->left;
		else if (key > node->key)
			node = node->right;
		else
			break;
	}
	return node == sentinel ? 0 : node->result;
}

typedef struct PafIndexedPayload {
	uint8_t reserved[28];
	uint32_t category;
} PafIndexedPayload;

typedef struct PafIndexedNode {
	struct PafIndexedNode *parent;
	struct PafIndexedNode *left;
	struct PafIndexedNode *right;
	int32_t key;
	PafIndexedPayload *payload;
	uint32_t reserved;
	uint8_t marked;
	uint8_t reserved2[4];
	int8_t isNil;
} PafIndexedNode;

typedef struct PafTreeSlot {
	PafIndexedNode *sentinel;
	uint32_t reserved[2];
} PafTreeSlot;

typedef struct PafTreeGroups {
	uint32_t reserved[3];
	PafTreeSlot slots[8];
} PafTreeGroups;

int ScePafMisc_32E2C605(PafTreeGroups *groups, uint32_t group,
			int32_t key, uint32_t category)
{
	PafIndexedNode *sentinel, *node;

	if (group >= 8)
		return -1;
	sentinel = groups->slots[group].sentinel;
	if (!sentinel)
		return -1;
	node = sentinel->parent;
	while (!node->isNil) {
		if (key > node->key)
			node = node->left;
		else if (key < node->key)
			node = node->right;
		else
			break;
	}
	if (node == sentinel)
		return -1;
	if (category && node->payload->category != category)
		return -1;
	node->marked = 1;
	return 0;
}

static PafIndexedNode *NextIndexedNode(PafIndexedNode *node)
{
	PafIndexedNode *next = node->right;

	if (!next->isNil) {
		PafIndexedNode *child = next->left;

		while (!child->isNil) {
			next = child;
			child = next->left;
		}
		return next;
	}
	next = node->parent;
	while (!next->isNil && next->left != node) {
		node = next;
		next = next->parent;
	}
	return next;
}

int ScePafMisc_CB4DB658(PafTreeGroups *groups, uint32_t group,
			uint32_t category)
{
	PafIndexedNode *sentinel, *node;

	if (group >= 8)
		return -1;
	sentinel = groups->slots[group].sentinel;
	if (!sentinel)
		return 0;
	node = sentinel->left;
	while (node != sentinel) {
		if (!category || node->payload->category == category)
			node->marked = 1;
		node = NextIndexedNode(node);
	}
	return 0;
}
