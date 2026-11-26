/*
 * N-ary box tree (BeTreeNode, BeTree).
 */

#include "be_isobmf_internal.h"

BeTreeNode::BeTreeNode()
	: m_firstChild(NULL)
	, m_parent(NULL)
	, m_nextSibling(NULL)
	, m_prevSibling(NULL)
{
}

BeTreeNode::~BeTreeNode()
{
	if (m_firstChild != NULL) {
		delete m_firstChild;
		m_firstChild = NULL;
	}
	if (m_nextSibling != NULL) {
		delete m_nextSibling;
		m_nextSibling = NULL;
	}
}

BeTreeNode *BeTreeNode::getFirstChild() const
{
	return m_firstChild;
}

BeTreeNode *BeTreeNode::getNextSibling() const
{
	return m_nextSibling;
}

BeTreeNode *BeTreeNode::getPrevSibling() const
{
	return m_prevSibling;
}

BeTreeNode *BeTreeNode::getParent() const
{
	return m_parent;
}

BeTree::BeTree()
	: m_stackDepth(0)
	, m_root(NULL)
	, m_current(NULL)
{
	for (int i = 0; i < 32; i++)
		m_savedStack[i] = NULL;
}

int BeTree::clear()
{
	if (m_root != NULL) {
		delete m_root;
		m_root = NULL;
	}
	m_current = NULL;
	return 0;
}

BeTree::~BeTree()
{
	clear();
}

void BeTree::insertChild(BeTreeNode *node)
{
	if (m_root == NULL) {
		m_root = node;
		m_current = node;
	} else if (m_current != NULL) {
		node->m_parent = m_current;
		if (m_current->m_firstChild == NULL)
			m_current->m_firstChild = node;
		m_current = node;
	}
}

void BeTree::insertSibling(BeTreeNode *node)
{
	if (m_root == NULL) {
		m_root = node;
		m_current = node;
	} else if (m_current != NULL) {
		while (m_current->m_nextSibling != NULL)
			m_current = m_current->m_nextSibling;
		node->m_parent = m_current->m_parent;
		node->m_prevSibling = m_current;
		m_current->m_nextSibling = node;
		m_current = m_current->m_nextSibling;
	}
}

int BeTree::moveToRoot()
{
	m_current = m_root;
	return m_current != NULL;
}

int BeTree::moveToParent()
{
	if (m_current == NULL)
		return 0;
	m_current = m_current->m_parent;
	return 1;
}

int BeTree::pushCursor()
{
	if (m_stackDepth == 31)
		return 0;
	m_savedStack[m_stackDepth] = m_current;
	m_stackDepth++;
	return 1;
}

int BeTree::popCursor()
{
	if (m_stackDepth == 1)
		return 0;
	m_stackDepth--;
	m_current = m_savedStack[m_stackDepth];
	return 1;
}

BeTreeNode *BeTree::getCurrent() const
{
	return m_current;
}
