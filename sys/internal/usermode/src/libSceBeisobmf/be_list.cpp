/*
 * Intrusive doubly-linked list (BeListNode, BeList).
 */

#include "be_isobmf_internal.h"

BeListNode::BeListNode()
	: m_prev(NULL)
	, m_next(NULL)
{
}

BeListNode::~BeListNode()
{
	if (m_next != NULL) {
		delete m_next;
		m_next = NULL;
	}
}

BeListNode *BeListNode::getNext() const
{
	return m_next;
}

BeList::BeList()
	: m_count(0)
	, m_head(NULL)
	, m_tail(NULL)
	, m_current(NULL)
{
}

BeList::~BeList()
{
	if (m_head != NULL) {
		delete m_head;
		m_head = NULL;
	}
	m_tail = NULL;
	m_current = NULL;
	m_count = 0;
}

void BeList::append(BeListNode *node)
{
	if (m_head == NULL && m_tail == NULL) {
		m_head = node;
		m_tail = node;
	} else {
		node->m_prev = m_tail;
		m_tail->m_next = node;
		m_tail = node;
		m_count++;
	}
}

int BeList::moveFirst()
{
	m_current = m_head;
	return m_current != NULL;
}

int BeList::moveNext()
{
	m_current = m_current->m_next;
	return m_current != NULL;
}

BeListNode *BeList::getCurrent() const
{
	return m_current;
}
