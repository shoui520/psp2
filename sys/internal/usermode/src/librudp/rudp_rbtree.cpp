/*
 * rudp_rbtree.cpp: intrusive Red-Black Tree container with sentinel node.
 */
#include "rudp_internal.h"

RudpRbTree::RudpRbTree(RudpRbCompareFn compareFn)
	: m_count(0), m_compareFn(compareFn), m_root(0), m_sentinel()
{
	m_root = &m_sentinel;
}

void RudpRbTree::rotateLeft(RudpRbNode *pivot)
{
	RudpRbNode *child = pivot->m_right;
	pivot->m_right = child->m_left;
	if (child->m_left != &m_sentinel) {
		child->m_left->m_parent = pivot;
	}
	child->m_parent = pivot->m_parent;
	if (pivot->m_parent == &m_sentinel) {
		m_root = child;
	} else if (pivot == pivot->m_parent->m_left) {
		pivot->m_parent->m_left = child;
	} else {
		pivot->m_parent->m_right = child;
	}
	child->m_left = pivot;
	pivot->m_parent = child;
}

void RudpRbTree::rotateRight(RudpRbNode *pivot)
{
	RudpRbNode *child = pivot->m_left;
	pivot->m_left = child->m_right;
	if (child->m_right != &m_sentinel) {
		child->m_right->m_parent = pivot;
	}
	child->m_parent = pivot->m_parent;
	if (pivot->m_parent == &m_sentinel) {
		m_root = child;
	} else if (pivot == pivot->m_parent->m_right) {
		pivot->m_parent->m_right = child;
	} else {
		pivot->m_parent->m_left = child;
	}
	child->m_right = pivot;
	pivot->m_parent = child;
}

void RudpRbTree::insertFixup(RudpRbNode *cur)
{
	while ((cur->m_parent->m_color & 1) != 0) {
		if (cur->m_parent == cur->m_parent->m_parent->m_left) {
			RudpRbNode *uncle = cur->m_parent->m_parent->m_right;
			if ((uncle->m_color & 1) != 0) {
				uncle->m_color &= ~1U;
				cur->m_parent->m_color &= ~1U;
				cur->m_parent->m_parent->m_color |= 1U;
				cur = cur->m_parent->m_parent;
			} else {
				if (cur == cur->m_parent->m_right) {
					cur = cur->m_parent;
					rotateLeft(cur);
				}
				cur->m_parent->m_color &= ~1U;
				cur->m_parent->m_parent->m_color |= 1U;
				rotateRight(cur->m_parent->m_parent);
			}
		} else {
			RudpRbNode *uncle = cur->m_parent->m_parent->m_left;
			if ((uncle->m_color & 1) != 0) {
				cur->m_parent->m_color &= ~1U;
				uncle->m_color &= ~1U;
				cur->m_parent->m_parent->m_color |= 1U;
				cur = cur->m_parent->m_parent;
			} else {
				if (cur == cur->m_parent->m_left) {
					cur = cur->m_parent;
					rotateRight(cur);
				}
				cur->m_parent->m_color &= ~1U;
				cur->m_parent->m_parent->m_color |= 1U;
				rotateLeft(cur->m_parent->m_parent);
			}
		}
	}
	m_root->m_color &= ~1U;
}

int RudpRbTree::insert(RudpRbNode *node)
{
	RudpRbNode *nil = &m_sentinel;
	RudpRbNode *parent = nil;
	RudpRbNode *cur = m_root;

	while (cur != nil) {
		parent = cur;
		if (m_compareFn(node->getKey(), cur->getKey()) != 0) {
			cur = cur->m_left;
		} else {
			cur = cur->m_right;
			if (parent == node) {
				return -1;
			}
		}
	}

	node->m_parent = parent;
	node->m_left = nil;
	node->m_right = nil;
	node->m_color = 1;

	if (parent == nil) {
		m_root = node;
	} else if (m_compareFn(node->getKey(), parent->getKey()) != 0) {
		parent->m_left = node;
	} else {
		parent->m_right = node;
	}

	insertFixup(node);
	m_count++;
	return 0;
}

RudpRbNode *RudpRbTree::prev(RudpRbNode *node) const
{
	const RudpRbNode *nil = &m_sentinel;
	RudpRbNode *cur = node;
	RudpRbNode *target;

	if (node != 0) {
		if (node->m_left == nil) {
			RudpRbNode *parent = node->m_parent;
			while (parent != nil && cur == parent->m_left) {
				cur = parent;
				parent = parent->m_parent;
			}
			target = parent;
		} else {
			target = node->m_left;
			while (target->m_right != nil) {
				target = target->m_right;
			}
		}
	} else {
		target = m_root;
		if (target != nil) {
			while (target->m_right != nil) {
				target = target->m_right;
			}
		}
	}

	if (target == nil) {
		return 0;
	}
	return target;
}

RudpRbNode *RudpRbTree::find(const void *key) const
{
	const RudpRbNode *nil = &m_sentinel;
	RudpRbNode *cur = m_root;

	while (cur != nil) {
		if (m_compareFn(key, cur->getKey()) != 0) {
			cur = cur->m_left;
		} else if (m_compareFn(cur->getKey(), key) == 0) {
			RudpRbNode *scan = cur;
			while (scan != 0 && scan != nil) {
				scan = prev(scan);
				if (scan == 0) {
					break;
				}
				if (m_compareFn(scan->getKey(), key) != 0) {
					break;
				}
				cur = scan;
			}
			break;
		} else {
			cur = cur->m_right;
		}
	}

	if (cur == nil) {
		return 0;
	}
	return cur;
}

void RudpRbTree::removeFixup(RudpRbNode *cur)
{
	RudpRbNode *nil = &m_sentinel;

	while (cur != m_root && (cur->m_color & 1) == 0) {
		if (cur == cur->m_parent->m_left) {
			RudpRbNode *sibling = cur->m_parent->m_right;
			if (sibling == nil) {
				break;
			}
			if ((sibling->m_color & 1) != 0) {
				sibling->m_color &= ~1U;
				cur->m_parent->m_color |= 1U;
				rotateLeft(cur->m_parent);
				sibling = cur->m_parent->m_right;
			}
			if (sibling == nil || sibling->m_left == nil || sibling->m_right == nil) {
				break;
			}
			if ((sibling->m_left->m_color & 1) == 0 && (sibling->m_right->m_color & 1) == 0) {
				sibling->m_color |= 1U;
				cur = cur->m_parent;
			} else {
				if ((sibling->m_right->m_color & 1) == 0) {
					sibling->m_left->m_color &= ~1U;
					sibling->m_color |= 1U;
					rotateRight(sibling);
					sibling = cur->m_parent->m_right;
				}
				sibling->m_color = cur->m_parent->m_color;
				cur->m_parent->m_color &= ~1U;
				sibling->m_right->m_color &= ~1U;
				rotateLeft(cur->m_parent);
				cur = m_root;
			}
		} else {
			RudpRbNode *sibling = cur->m_parent->m_left;
			if (sibling == nil) {
				break;
			}
			if ((sibling->m_color & 1) != 0) {
				sibling->m_color &= ~1U;
				cur->m_parent->m_color |= 1U;
				rotateRight(cur->m_parent);
				sibling = cur->m_parent->m_left;
			}
			if (sibling == nil || sibling->m_left == nil || sibling->m_right == nil) {
				break;
			}
			if ((sibling->m_right->m_color & 1) == 0 && (sibling->m_left->m_color & 1) == 0) {
				sibling->m_color |= 1U;
				cur = cur->m_parent;
			} else {
				if ((sibling->m_left->m_color & 1) == 0) {
					sibling->m_right->m_color &= ~1U;
					sibling->m_color |= 1U;
					rotateLeft(sibling);
					sibling = cur->m_parent->m_left;
				}
				sibling->m_color = cur->m_parent->m_color;
				cur->m_parent->m_color &= ~1U;
				sibling->m_left->m_color &= ~1U;
				rotateRight(cur->m_parent);
				cur = m_root;
			}
		}
	}
	cur->m_color &= ~1U;
}

RudpRbNode *RudpRbTree::next(RudpRbNode *node, int *depth) const
{
	const RudpRbNode *nil = &m_sentinel;
	RudpRbNode *cur = node;

	if (node == 0) {
		cur = m_root;
		if (cur == nil) {
			return 0;
		}
		while (cur->m_left != nil) {
			cur = cur->m_left;
			if (depth != 0) {
				(*depth)++;
			}
		}
		return (cur == nil) ? 0 : cur;
	}

	if (node->m_right != nil) {
		cur = node->m_right;
		if (depth != 0) {
			(*depth)++;
		}
		while (cur->m_left != nil) {
			cur = cur->m_left;
			if (depth != 0) {
				(*depth)++;
			}
		}
		return (cur == nil) ? 0 : cur;
	}

	if (node->m_parent == nil) {
		return 0;
	}

	if (node == node->m_parent->m_left) {
		cur = node->m_parent;
		if (depth != 0) {
			(*depth)--;
		}
		return (cur == nil) ? 0 : cur;
	}

	if (node != node->m_parent->m_right) {
		return (cur == nil) ? 0 : cur;
	}

	int savedDepth = 0;
	if (depth != 0) {
		savedDepth = *depth;
	}
	while (cur->m_parent != nil && cur == cur->m_parent->m_right) {
		cur = cur->m_parent;
		if (depth != 0) {
			(*depth)--;
		}
	}
	cur = cur->m_parent;
	if (cur != nil) {
		if (depth != 0) {
			(*depth)--;
		}
		return cur;
	}
	if (depth != 0) {
		*depth = savedDepth;
	}
	return 0;
}

void RudpRbTree::remove(RudpRbNode *node)
{
	RudpRbNode *nil = &m_sentinel;
	RudpRbNode *splice;

	if (node->m_left == nil || node->m_right == nil) {
		splice = node;
	} else {
		splice = next(node, 0);
	}

	RudpRbNode *child = (splice->m_left == nil) ? splice->m_right : splice->m_left;
	if (child != nil) {
		child->m_parent = splice->m_parent;
	}
	if (splice->m_parent == nil) {
		m_root = child;
	} else if (splice == splice->m_parent->m_left) {
		splice->m_parent->m_left = child;
	} else {
		splice->m_parent->m_right = child;
	}

	if (splice != node) {
		splice->m_parent = node->m_parent;
		splice->m_left = node->m_left;
		splice->m_right = node->m_right;
		splice->m_color = node->m_color;
		node->m_left->m_parent = splice;
		if (node->m_right != splice) {
			if (splice->m_parent == nil) {
				m_root = splice;
			}
			if (node->m_right->m_parent != nil) {
				node->m_right->m_parent = splice;
			}
		}
		if (node == node->m_parent->m_left) {
			node->m_parent->m_left = splice;
		} else {
			node->m_parent->m_right = splice;
		}
	}

	if ((node->m_color & 1) == 0) {
		if (child == nil) {
			if (splice != node) {
				removeFixup(splice);
			}
		} else {
			removeFixup(child);
		}
	}

	node->m_parent = nil;
	node->m_right = nil;
	node->m_left = nil;
	node->m_color |= 1U;
	m_count--;
}

RudpRbNode *RudpRbTree::removeByKey(const void *key)
{
	RudpRbNode *found = find(key);
	if (found == 0) {
		return 0;
	}
	remove(found);
	return found;
}

RudpRbNode *RudpRbTree::first(int *depth) const
{
	const RudpRbNode *nil = &m_sentinel;
	RudpRbNode *cur = m_root;
	if (depth != 0) {
		*depth = 0;
	}
	if (cur == nil) {
		return 0;
	}
	while (cur->m_left != nil) {
		cur = cur->m_left;
		if (depth != 0) {
			(*depth)++;
		}
	}
	return cur;
}

void RudpRbTree::clear()
{
	while (true) {
		RudpRbNode *node = first(0);
		if (node == 0) {
			break;
		}
		removeByKey(node->getKey());
	}
	m_count = 0;
	m_root = &m_sentinel;
}

uint32_t RudpRbTree::getCount() const
{
	return m_count;
}
