/*
 * ScebXCe's stacks: the names of the open elements (the SAX parser's) and their parent nodes
 * (the document builder's, its entries kept in a pool for reuse).
 */
#include "bxce_internal.h"

int bxceNameStackInitialize(BxceNameStack *stack)
{
	stack->top = NULL;
	return 0;
}

int bxceNameStackTerminate(BxceNameStack *stack)
{
	BxceNameStackEntry *entry;

	while ((entry = stack->top) != NULL) {
		stack->top = entry->next;
		if (entry->name != NULL)
			ScebXCe_EC560647(entry->name);
		entry->name = NULL;
		ScebXCe_EC560647(entry);
	}
	return 0;
}

int bxceNameStackPush(BxceNameStack *stack, char *name, int length)
{
	BxceNameStackEntry *entry;

	entry = (BxceNameStackEntry *)ScebXCe_B8DDE741(sizeof(BxceNameStackEntry));
	if (entry == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;

	entry->name = name;
	entry->length = length;
	entry->next = NULL;
	if (stack->top != NULL)
		entry->next = stack->top;
	stack->top = entry;
	return 0;
}

BxceNameStackEntry *bxceNameStackPop(BxceNameStack *stack)
{
	BxceNameStackEntry *entry = stack->top;

	if (entry == NULL)
		return NULL;
	stack->top = entry->next;
	return entry;
}

int bxceNameStackIsEmpty(const BxceNameStack *stack)
{
	return stack->top == NULL;
}

int bxceNodeStackInitialize(BxceNodeStack *stack)
{
	stack->pool = ScebXCe_B8DDE741(sizeof(BxceAttrPool));
	if (stack->pool == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;

	bxceAttrPoolInitialize((BxceAttrPool *)stack->pool);
	stack->top = NULL;
	return 0;
}

void bxceNodeStackTerminate(BxceNodeStack *stack)
{
	BxceNodeStackEntry *entry, *next;
	BxceAttr *pooled;

	if (stack->pool == NULL)
		return;

	for (pooled = bxceAttrPoolPop((BxceAttrPool *)stack->pool); pooled != NULL;
	     pooled = bxceAttrPoolPop((BxceAttrPool *)stack->pool))
		ScebXCe_EC560647(pooled);
	if (stack->pool != NULL)
		ScebXCe_EC560647(stack->pool);
	stack->pool = NULL;

	for (entry = (BxceNodeStackEntry *)stack->top; entry != NULL; entry = next) {
		next = (BxceNodeStackEntry *)entry->next;
		ScebXCe_EC560647(entry);
		stack->top = next;
	}
}

int bxceNodeStackPush(BxceNodeStack *stack, BxceNodeId node)
{
	BxceNodeStackEntry *entry;

	entry = (BxceNodeStackEntry *)bxceAttrPoolPop((BxceAttrPool *)stack->pool);
	if (entry == NULL) {
		entry = (BxceNodeStackEntry *)ScebXCe_B8DDE741(sizeof(BxceNodeStackEntry));
		if (entry == NULL)
			return SCE_BXCE_ERROR_NO_MEMORY;
		entry->next = NULL;
	}

	entry->node = (void *)node;
	entry->next = stack->top;
	stack->top = entry;
	return 0;
}

/* the pool keeps the entry, or it is freed */
BxceNodeId bxceNodeStackPop(BxceNodeStack *stack)
{
	BxceNodeStackEntry *entry = (BxceNodeStackEntry *)stack->top;
	BxceNodeId node;

	stack->top = entry->next;
	node = (BxceNodeId)entry->node;
	if (bxceAttrPoolPush((BxceAttrPool *)stack->pool, (BxceAttr *)entry) == 0 && entry != NULL)
		ScebXCe_EC560647(entry);
	return node;
}
