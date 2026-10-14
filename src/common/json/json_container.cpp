/*
 * libSceJson: String, Array, Object and their iterators.  Each one is a
 * handle on a private object from the library heap (json_internal.h);
 * a handle whose allocation failed has m_impl == NULL and acts empty.
 */

#include "json_internal.h"

namespace sce {
namespace Json {

const size_t String::npos = (size_t)-1;

/* ---------------------------------------------------------------------- */
/* String                                                                 */

String::String() : m_impl(NULL)
{
	void *mem = Malloc(sizeof(SimpleString));

	if (mem == NULL)
		return;
	m_impl = new (mem) SimpleString();
	((SimpleString *)m_impl)->clear();
}

String::String(const char *s) : m_impl(NULL)
{
	void *mem = Malloc(sizeof(SimpleString));

	if (mem == NULL)
		return;
	m_impl = new (mem) SimpleString();
	((SimpleString *)m_impl)->clear();
	((SimpleString *)m_impl)->assign(s, s != NULL ? jsonStrlen(s) : 0);
}

String::String(const String &s) : m_impl(NULL)
{
	void *mem;

	if (s.m_impl == NULL)
		return;
	mem = Malloc(sizeof(SimpleString));
	if (mem == NULL)
		return;
	m_impl = new (mem) SimpleString();
	((SimpleString *)m_impl)->clear();
	((SimpleString *)m_impl)->assign(*(SimpleString *)s.m_impl);
}

String::~String()
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl != NULL) {
		impl->~SimpleString();
		Free(m_impl);
		m_impl = NULL;
	}
}

int String::compare(const String &s) const
{
	if (m_impl == NULL)
		return 0;
	if (s.m_impl == NULL)
		return 0;
	return jsonStrcmp(((SimpleString *)m_impl)->m_data, ((SimpleString *)s.m_impl)->m_data);
}

int String::compare(const char *s) const
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl == NULL)
		return 0;

	SimpleString other(s);

	return jsonStrcmp(impl->m_data, other.m_data);
}

size_t String::size() const
{
	return m_impl != NULL ? ((SimpleString *)m_impl)->m_length : 0;
}

size_t String::length() const
{
	return m_impl != NULL ? ((SimpleString *)m_impl)->m_length : 0;
}

unsigned char String::at(size_t pos) const
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl == NULL)
		return 0;
	if (pos >= impl->m_length)
		jsonOutOfRange();
	return impl->m_data[pos];
}

bool String::empty() const
{
	if (m_impl == NULL)
		return false;
	return ((SimpleString *)m_impl)->m_length == 0;
}

const char *String::c_str() const
{
	if (m_impl == NULL)
		return "";
	return ((SimpleString *)m_impl)->m_data;
}

String &String::append(const String &s)
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl != NULL) {
		const char *src = s.c_str();

		impl->append(src, jsonStrlen(src));
	}
	return *this;
}

String &String::append(const char *s)
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl != NULL)
		impl->append(s, jsonStrlen(s));
	return *this;
}

String &String::append(const char *s, size_t n)
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl != NULL)
		impl->append(s, n);
	return *this;
}

void String::clear()
{
	if (m_impl != NULL)
		((SimpleString *)m_impl)->clear();
}

size_t String::find(const String &str, size_t pos) const
{
	SimpleString *impl = (SimpleString *)m_impl;
	const char *s;

	if (impl == NULL)
		return npos;
	s = str.c_str();
	return impl->find(s, pos, jsonStrlen(s));
}

size_t String::find(const char *s, size_t pos) const
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl == NULL)
		return npos;
	return impl->find(s, pos, jsonStrlen(s));
}

size_t String::find(const char *s, size_t pos, size_t n) const
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl == NULL)
		return npos;
	return impl->find(s, pos, n);
}

size_t String::find(char c, size_t pos) const
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl == NULL)
		return npos;
	return impl->find(&c, pos, 1);
}

size_t String::rfind(const String &str, size_t pos) const
{
	SimpleString *impl = (SimpleString *)m_impl;
	const char *s;

	if (impl == NULL)
		return npos;
	s = str.c_str();
	return impl->rfind(s, pos, jsonStrlen(s));
}

size_t String::rfind(const char *s, size_t pos) const
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl == NULL)
		return npos;
	return impl->rfind(s, pos, jsonStrlen(s));
}

size_t String::rfind(const char *s, size_t pos, size_t n) const
{
	if (m_impl == NULL)
		return npos;
	return ((SimpleString *)m_impl)->rfind(s, pos, n);
}

size_t String::rfind(char c, size_t pos) const
{
	if (m_impl == NULL)
		return npos;
	return ((SimpleString *)m_impl)->rfind(&c, pos, 1);
}

void String::resize(size_t n)
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl != NULL)
		impl->resize(n);
}

String String::substr(size_t pos, size_t len) const
{
	if (m_impl == NULL)
		return *Value::s_nullstring;

	SimpleString sub = ((SimpleString *)m_impl)->substr(pos, len);
	String str(sub.m_data);

	return str;
}

bool String::operator==(const String &s) const
{
	if (m_impl == NULL)
		return false;
	if (s.m_impl == NULL)
		return false;
	return ((SimpleString *)m_impl)->equals(((SimpleString *)s.m_impl)->m_data);
}

bool String::operator==(const char *s) const
{
	if (m_impl == NULL)
		return false;
	return ((SimpleString *)m_impl)->equals(s);
}

String &String::operator=(const String &s)
{
	if (m_impl != NULL && s.m_impl != NULL)
		((SimpleString *)m_impl)->assign(*(SimpleString *)s.m_impl);
	return *this;
}

String &String::operator+=(const char *s)
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl != NULL)
		impl->append(s, jsonStrlen(s));
	return *this;
}

String &String::operator+=(unsigned char c)
{
	SimpleString *impl = (SimpleString *)m_impl;

	if (impl != NULL) {
		char s[2];

		s[0] = c;
		s[1] = '\0';
		impl->append(s, jsonStrlen(s));
	}
	return *this;
}

/* ---------------------------------------------------------------------- */
/* Array                                                                  */

static inline ValueList *newValueList(void)
{
	void *mem = Malloc(sizeof(ValueList));
	ValueList *impl;

	if (mem == NULL)
		return NULL;
	impl = new (mem) ValueList();
	impl->clear();
	return impl;
}

/* Points it at node; a NULL node leaves it without implementation. */
static inline void setNode(Array::iterator &it, ValueList::Node *node)
{
	ListIterator *impl = ImplAccessor::impl(it);

	if (node == NULL) {
		if (impl != NULL)
			delete impl;
		ImplAccessor::release(it);
	} else if (impl != NULL) {
		impl->node = node;
	}
}

Array::Array() : m_impl(NULL)
{
	m_impl = newValueList();
}

Array::Array(const Array &a) : m_impl(NULL)
{
	void *mem;

	if (a.m_impl == NULL)
		return;
	mem = Malloc(sizeof(ValueList));
	if (mem == NULL)
		return;
	m_impl = new (mem) ValueList();
	((ValueList *)m_impl)->clear();
	ValueList(*(ValueList *)a.m_impl).swap(*(ValueList *)m_impl);
}

Array::~Array()
{
	ValueList *impl = (ValueList *)m_impl;

	if (impl != NULL) {
		impl->~ValueList();
		Free(m_impl);
		m_impl = NULL;
	}
}

Array::iterator Array::begin() const
{
	ValueList *impl = (ValueList *)m_impl;
	iterator it;

	if (impl != NULL)
		setNode(it, impl->m_head != NULL ? impl->m_head->next : NULL);
	return it;
}

Array::iterator Array::end() const
{
	ValueList *impl = (ValueList *)m_impl;
	iterator it;

	if (impl != NULL)
		setNode(it, impl->m_head);
	return it;
}

Array::iterator Array::insert(const iterator &where, const Value &val)
{
	ValueList *impl = (ValueList *)m_impl;
	ValueList::Node *pos, *node;
	iterator it;

	if (impl != NULL) {
		pos = ImplAccessor::impl(where)->node;
		node = impl->newNode(val);
		if (node != impl->m_head)
			impl->insert(pos, node);
		setNode(it, node);
	}
	return it;
}

Array::iterator Array::erase(const iterator &where)
{
	ValueList *impl = (ValueList *)m_impl;
	ValueList::Node *node, *next;
	iterator it;

	if (impl != NULL) {
		node = ImplAccessor::impl(where)->node;
		next = node->next;
		impl->remove(node);
		setNode(it, next);
	}
	return it;
}

Value &Array::back() const
{
	if (m_impl == NULL)
		return *g_nullValue;
	return ((ValueList *)m_impl)->m_head->prev->value;
}

void Array::clear()
{
	ValueList *impl = (ValueList *)m_impl;

	if (impl != NULL)
		impl->clear();
}

size_t Array::size() const
{
	return m_impl != NULL ? ((ValueList *)m_impl)->m_size : 0;
}

void Array::pop_back()
{
	ValueList *impl = (ValueList *)m_impl;

	if (impl != NULL && impl->m_size != 0 && impl->m_head != NULL)
		impl->remove(impl->m_head->prev);
}

void Array::push_back(const Value &val)
{
	ValueList *impl;

	if (m_impl == NULL)
		return;
	beginAllocTracking();
	impl = (ValueList *)m_impl;
	if (impl->m_head != NULL)
		impl->push_back(val);
	if (endAllocTracking())
		pop_back();
}

bool Array::empty() const
{
	if (m_impl == NULL)
		return true;
	return ((ValueList *)m_impl)->m_size == 0;
}

Array &Array::operator=(const Array &a)
{
	ValueList *impl = (ValueList *)m_impl;

	if (impl != NULL && a.m_impl != NULL)
		ValueList(*(ValueList *)a.m_impl).swap(*impl);
	return *this;
}

/* ---------------------------------------------------------------------- */
/* Array::iterator                                                        */

Array::iterator::iterator() : m_itimpl(NULL)
{
	void *mem = Malloc(sizeof(ListIterator));

	if (mem != NULL)
		m_itimpl = new (mem) ListIterator();
}

Array::iterator::iterator(const iterator &it) : m_itimpl(NULL)
{
	void *mem;

	if (it.m_itimpl == NULL || (mem = Malloc(sizeof(ListIterator))) == NULL)
		return;
	m_itimpl = new (mem) ListIterator();
	((ListIterator *)m_itimpl)->node = ((ListIterator *)it.m_itimpl)->node;
}

bool Array::iterator::operator!=(iterator it) const
{
	if (m_itimpl == NULL)
		return false;
	if (it.m_itimpl == NULL)
		return false;
	return ((ListIterator *)m_itimpl)->node != ((ListIterator *)it.m_itimpl)->node;
}

Value *Array::iterator::operator->() const
{
	if (m_itimpl == NULL)
		return g_nullValue;
	return &((ListIterator *)m_itimpl)->node->value;
}

Value &Array::iterator::operator*() const
{
	if (m_itimpl == NULL)
		return *g_nullValue;
	return ((ListIterator *)m_itimpl)->node->value;
}

Array::iterator &Array::iterator::operator++()
{
	ListIterator *impl = (ListIterator *)m_itimpl;

	if (impl != NULL && impl->node != NULL)
		impl->node = impl->node->next;
	return *this;
}

Array::iterator &Array::iterator::operator++(int)
{
	ListIterator *impl = (ListIterator *)m_itimpl;

	if (impl != NULL && impl->node != NULL)
		impl->node = impl->node->next;
	return *this;
}

void Array::iterator::advance(size_t off)
{
	ListIterator *impl;
	size_t i;

	if (m_itimpl == NULL)
		return;
	for (i = 0; i < off; i++) {
		impl = (ListIterator *)m_itimpl;
		if (impl->node != NULL)
			impl->node = impl->node->next;
	}
}

/* ---------------------------------------------------------------------- */
/* Object                                                                 */

/* Points it at node; a NULL node leaves it without implementation. */
static inline void setNode(Object::iterator &it, PairTree::Node *node)
{
	TreeIterator *impl = ImplAccessor::impl(it);

	if (node == NULL) {
		if (impl != NULL)
			delete impl;
		ImplAccessor::release(it);
	} else if (impl != NULL) {
		impl->node = node;
	}
}

Object::Pair::~Pair()
{
}

Object::Object() : m_impl(NULL)
{
	PairTree *impl = new PairTree();

	if (impl != NULL && (impl->m_head == NULL || impl->m_head->left == NULL)) {
		delete impl;
		impl = NULL;
	}
	m_impl = impl;
}

Object::Object(const Object &o) : m_impl(NULL)
{
	void *mem;
	PairTree *impl;

	if (o.m_impl == NULL || (mem = Malloc(sizeof(PairTree))) == NULL)
		return;
	impl = new (mem) PairTree();
	m_impl = impl;

	PairTree copy(*(PairTree *)o.m_impl);

	impl->swap(copy);
}

Object::~Object()
{
	if (m_impl != NULL) {
		clear();
		((PairTree *)m_impl)->~PairTree();
		Free(m_impl);
		m_impl = NULL;
	}
}

Object::iterator Object::find(const String &key) const
{
	iterator it;

	if (m_impl != NULL && ImplAccessor::impl(key) != NULL)
		setNode(it, ((PairTree *)m_impl)->find(*ImplAccessor::impl(key)).node);
	return it;
}

Object::iterator Object::insert(const Pair &objpair)
{
	PairTree *impl = (PairTree *)m_impl;
	PairTree::Node *node;
	iterator it;
	bool inserted;

	if (impl != NULL) {
		node = impl->insert(PairTree::Entry(SimpleString(objpair.first.c_str()), objpair),
				    &inserted);
		setNode(it, node);
	}
	return it;
}

void Object::erase(const String &key)
{
	if (m_impl != NULL && ImplAccessor::impl(key) != NULL)
		((PairTree *)m_impl)->erase(*ImplAccessor::impl(key));
}

void Object::clear()
{
	if (m_impl != NULL)
		((PairTree *)m_impl)->clear();
}

size_t Object::size() const
{
	return m_impl != NULL ? ((PairTree *)m_impl)->m_size : 0;
}

bool Object::empty() const
{
	if (m_impl == NULL)
		return true;
	return ((PairTree *)m_impl)->m_size == 0;
}

JSON_NOINLINE
Object::iterator Object::begin() const
{
	PairTree *impl = (PairTree *)m_impl;
	iterator it;

	if (impl != NULL)
		setNode(it, impl->m_head != NULL ? impl->m_head->left : NULL);
	return it;
}

Object::iterator Object::end() const
{
	PairTree *impl = (PairTree *)m_impl;
	iterator it;

	if (impl != NULL)
		setNode(it, impl->m_head);
	return it;
}

Object::Pair::Pair() : second(kValueTypeNull)
{
}

Value &Object::operator[](const String &key)
{
	Object::Pair *pair;

	if (m_impl == NULL)
		return *g_nullValue;
	if (ImplAccessor::impl(key) == NULL)
		return *g_nullValue;
	beginAllocTracking();
	{
		PairTree *impl = (PairTree *)m_impl;
		SimpleString name(key.c_str());

		pair = &(*impl)[name];
	}
	if (endAllocTracking())
		return *g_nullValue;
	pair->first = key;
	return pair->second;
}

Object &Object::operator=(const Object &o)
{
	PairTree *impl = (PairTree *)m_impl;

	if (impl != NULL && o.m_impl != NULL) {
		PairTree copy(*(PairTree *)o.m_impl);

		impl->swap(copy);
	}
	return *this;
}

Object::Pair::Pair(const String &srcstr, const Value &srcval) : first(srcstr), second(srcval)
{
}

/* ---------------------------------------------------------------------- */
/* iterators                                                              */

Array::iterator::~iterator()
{
	if (m_itimpl != NULL) {
		Free(m_itimpl);
		m_itimpl = NULL;
	}
}

Object::iterator::iterator() : m_itimpl(NULL)
{
	void *mem = Malloc(sizeof(TreeIterator));

	if (mem != NULL)
		m_itimpl = new (mem) TreeIterator();
}

Object::iterator::iterator(const iterator &it) : m_itimpl(NULL)
{
	void *mem;

	if (it.m_itimpl == NULL || (mem = Malloc(sizeof(TreeIterator))) == NULL)
		return;
	m_itimpl = new (mem) TreeIterator();
	((TreeIterator *)m_itimpl)->node = ((TreeIterator *)it.m_itimpl)->node;
}

Object::iterator::~iterator()
{
	if (m_itimpl != NULL) {
		Free(m_itimpl);
		m_itimpl = NULL;
	}
}

bool Object::iterator::operator==(iterator it) const
{
	if (m_itimpl == NULL)
		return false;
	if (it.m_itimpl == NULL)
		return false;
	return ((TreeIterator *)m_itimpl)->node == ((TreeIterator *)it.m_itimpl)->node;
}

bool Object::iterator::operator!=(iterator it) const
{
	if (m_itimpl == NULL)
		return false;
	if (it.m_itimpl == NULL)
		return false;
	return ((TreeIterator *)m_itimpl)->node != ((TreeIterator *)it.m_itimpl)->node;
}

/* in-order successor */
static inline PairTree::Node *nextNode(PairTree::Node *node)
{
	PairTree::Node *parent;

	if (!node->right->isnil)
		return PairTree::min(node->right);
	while (!(parent = node->parent)->isnil && parent->left != node)
		node = parent;
	return parent;
}

Object::iterator &Object::iterator::operator++()
{
	TreeIterator *impl = (TreeIterator *)m_itimpl;

	if (impl != NULL && impl->node != NULL)
		impl->node = nextNode(impl->node);
	return *this;
}

Object::iterator &Object::iterator::operator++(int)
{
	TreeIterator *impl = (TreeIterator *)m_itimpl;

	if (impl != NULL && impl->node != NULL)
		impl->node = nextNode(impl->node);
	return *this;
}

Object::Pair &Object::iterator::operator*() const
{
	if (m_itimpl == NULL) {
		iterator it = Value::s_nullobject->begin();

		return *it;
	}
	return ((TreeIterator *)m_itimpl)->node->entry.pair;
}

Object::Pair *Object::iterator::operator->() const
{
	if (m_itimpl == NULL) {
		iterator it = Value::s_nullobject->begin();

		return &*it;
	}
	return &((TreeIterator *)m_itimpl)->node->entry.pair;
}

Array::iterator &Array::iterator::operator=(const iterator &it)
{
	ListIterator *impl = (ListIterator *)m_itimpl;

	if (impl != NULL) {
		if (it.m_itimpl == NULL) {
			delete impl;
			m_itimpl = NULL;
		} else {
			setNode(*this, ((ListIterator *)it.m_itimpl)->node);
		}
	}
	return *this;
}

Object::iterator &Object::iterator::operator=(const iterator &it)
{
	TreeIterator *impl = (TreeIterator *)m_itimpl;

	if (impl != NULL) {
		if (it.m_itimpl == NULL) {
			delete impl;
			m_itimpl = NULL;
		} else {
			setNode(*this, ((TreeIterator *)it.m_itimpl)->node);
		}
	}
	return *this;
}

void Object::iterator::advance(size_t off)
{
	TreeIterator *impl;
	size_t i;

	if (m_itimpl == NULL)
		return;
	for (i = 0; i < off; i++) {
		impl = (TreeIterator *)m_itimpl;
		if (impl->node != NULL)
			impl->node = nextNode(impl->node);
	}
}

/* ---------------------------------------------------------------------- */
/* private containers (out of line)                                      */

char SimpleString::s_empty[1];

JSON_NOINLINE
SimpleString &SimpleString::assign(const char *s, size_t n)
{
	char *data;

	if (m_data == s)
		return *this;
	if (m_data != s_empty)
		Free(m_data);
	if (n == 0 || s == NULL || *s == '\0') {
		m_data = s_empty;
		m_length = 0;
		return *this;
	}
	data = (char *)Malloc(n + 1);
	if (data == NULL) {
		m_data = s_empty;
		m_length = 0;
	} else {
		m_data = data;
		m_length = n;
		jsonMemcpy(data, s, n);
		m_data[n] = '\0';
	}
	return *this;
}

} /* namespace Json */
} /* namespace sce */

JSON_NOINLINE
void *operator new(size_t size, void *where)
{
	return where;
}

namespace sce {
namespace Json {

JSON_NOINLINE
SimpleString &SimpleString::assign(const SimpleString &s)
{
	char *data;

	if (&s == this)
		return *this;
	if (m_data != s_empty)
		Free(m_data);
	if (s.m_length == 0) {
		m_data = s_empty;
		m_length = 0;
		return *this;
	}
	data = (char *)Malloc(s.m_length + 1);
	if (data == NULL) {
		m_data = s_empty;
		m_length = 0;
	} else {
		m_data = data;
		m_length = s.m_length;
		jsonMemcpy(data, s.m_data, s.m_length + 1);
	}
	return *this;
}

JSON_NOINLINE
size_t SimpleString::rfind(const char *s, size_t pos, size_t n) const
{
	const char *p;

	if (n == 0)
		return pos < m_length ? pos : m_length;
	if (n <= m_length) {
		if (m_length - n <= pos)
			pos = m_length - n;
		for (p = m_data + pos;; p--) {
			if (*s == *p && jsonStrncmp(p, s, n) == 0)
				return p - m_data;
			if (p == m_data)
				break;
		}
	}
	return String::npos;
}

JSON_NOINLINE
SimpleString &SimpleString::erase(size_t pos, size_t n)
{
	char *data;
	size_t length;

	if (m_length < pos)
		jsonOutOfRange();
	if (m_length - pos < n)
		n = m_length - pos;
	data = (char *)Malloc(m_length - n + 1);
	if (data != NULL) {
		length = m_length;
		jsonMemcpy(data, m_data, pos);
		jsonMemcpy(data + pos, m_data + pos + n, length - pos - n);
		data[length - n] = '\0';
		if (m_data != s_empty)
			Free(m_data);
		m_data = data;
		m_length = length - n;
	}
	return *this;
}

JSON_NOINLINE
SimpleString SimpleString::substr(size_t pos, size_t n) const
{
	if (m_length < pos)
		jsonOutOfRange();
	if (pos >= m_length)
		return SimpleString();
	if (n == String::npos)
		n = m_length - pos;
	else if (pos + n > m_length)
		n = m_length - pos;
	return SimpleString(m_data + pos, n);
}

/* ---------------------------------------------------------------------- */
/* ValueList                                                              */

JSON_NOINLINE
ValueList::ValueList(const ValueList &list) : m_size(0)
{
	Node *src, *last, *node;

	m_head = (Node *)Malloc(sizeof(Node));
	if (m_head == NULL)
		return;
	src = list.m_head != NULL ? list.m_head->next : NULL;
	last = m_head;
	while (src != list.m_head) {
		node = newNode(src->value);
		if (node == m_head)
			break;
		node->prev = last;
		last->next = node;
		m_size++;
		if (src != NULL)
			src = src->next;
		last = node;
	}
	last->next = m_head;
	m_head->prev = last;
}

/* ---------------------------------------------------------------------- */
/* PairTree                                                               */

JSON_NOINLINE
void PairTree::clear()
{
	Node *node, *parent;

	if (m_head == NULL)
		return;
	node = m_head->left;	/* the leftmost node */
	while (!node->isnil) {
		if (!node->left->isnil) {
			node = node->left;
		} else if (!node->right->isnil) {
			node = node->right;
		} else {
			/* a leaf: unlink and free it, then continue at its parent */
			parent = node->parent;
			if (!parent->isnil) {
				if (parent->left == node)
					parent->left = m_head;
				else
					parent->right = m_head;
			}
			node->entry.~Entry();
			Free(node);
			node = parent;
		}
	}
	m_head->parent = m_head;
	m_head->left = m_head;
	m_head->right = m_head;
	m_size = 0;
}

JSON_NOINLINE
PairTree::Node *PairTree::copyNodes(Node *parent, const Node *src)
{
	Node *node = (Node *)Malloc(sizeof(Node));

	if (node == NULL)
		return m_head;
	new (node) Node(parent, m_head, *src);
	if (!src->left->isnil)
		node->left = copyNodes(node, src->left);
	if (!src->right->isnil)
		node->right = copyNodes(node, src->right);
	return node;
}

JSON_NOINLINE
PairTree::PairTree(const PairTree &tree) : m_size(0)
{
	Node *root, *head;

	m_head = (Node *)Malloc(sizeof(Node));
	if (m_head == NULL)
		return;
	m_head->parent = m_head;
	m_head->left = m_head;
	m_head->right = m_head;
	m_head->color = BLACK;
	m_head->isnil = 1;
	if (tree.m_head == NULL || tree.m_head->parent->isnil)
		return;
	root = copyNodes(m_head, tree.m_head->parent);
	head = m_head;
	if (root == head)
		return;
	head->parent = root;
	head->left = min(root);
	head->right = max(root);
	m_size = tree.m_size;
}

/* the node holding key, the nil head when absent; NULL without a head */
static inline PairTree::Node *lookup(const PairTree::Node *head, const SimpleString &key)
{
	PairTree::Node *node;

	if (head == NULL)
		return NULL;
	node = head->parent;
	while (!node->isnil) {
		if (key < node->entry.key)
			node = node->left;
		else if (node->entry.key < key)
			node = node->right;
		else
			break;
	}
	return node;
}

JSON_NOINLINE
TreeIterator PairTree::find(const SimpleString &key) const
{
	return TreeIterator(lookup(m_head, key));
}
JSON_NOINLINE
void PairTree::insertFixup(Node *node)
{
	Node *parent, *uncle;

	if (m_head == NULL)
		return;
	node->color = RED;
	while (node != m_head->parent && (parent = node->parent)->color == RED) {
		if (parent == parent->parent->left) {
			uncle = parent->parent->right;
			if (uncle->color == RED) {
				parent->color = BLACK;
				uncle->color = BLACK;
				node->parent->parent->color = RED;
				node = node->parent->parent;
				continue;
			}
			if (node == parent->right) {
				node = parent;
				rotateLeft(node);
			}
			node->parent->color = BLACK;
			node->parent->parent->color = RED;
			rotateRight(node->parent->parent);
		} else {
			uncle = parent->parent->left;
			if (uncle->color == RED) {
				parent->color = BLACK;
				uncle->color = BLACK;
				node->parent->parent->color = RED;
				node = node->parent->parent;
				continue;
			}
			if (node == parent->left) {
				node = parent;
				rotateRight(node);
			}
			node->parent->color = BLACK;
			node->parent->parent->color = RED;
			rotateLeft(node->parent->parent);
		}
	}
	m_head->parent->color = BLACK;
}

/*
 * Inserts entry unless its key exists; returns the node holding the key,
 * the head when out of memory, NULL without head.
 */
JSON_NOINLINE
PairTree::Node *PairTree::insert(const Entry &entry, bool *inserted)
{
	Node *parent, *node;
	int side;

	parent = m_head;
	if (parent == NULL)
		return NULL;
	node = parent->parent;
	if (inserted != NULL)
		*inserted = false;
	side = 0;
	while (!node->isnil) {
		parent = node;
		if (entry.key < node->entry.key) {
			side = -1;
			node = node->left;
		} else if (!(node->entry.key < entry.key)) {
			return node;
		} else {
			side = 1;
			node = node->right;
		}
	}

	node = (Node *)Malloc(sizeof(Node));
	if (node == NULL)
		return m_head;
	new (node) Node(parent, m_head, entry);
	if (side == 0) {
		Node *head = m_head;

		head->parent = node;
		head->left = node;
		head->right = node;
	} else if (side < 0) {
		parent->left = node;
		if (parent == m_head->left)
			m_head->left = node;
	} else {
		parent->right = node;
		if (parent == m_head->right)
			m_head->right = node;
	}
	m_size++;
	insertFixup(node);
	if (inserted != NULL)
		*inserted = true;
	return node;
}

JSON_NOINLINE
void PairTree::eraseFixup(Node *node, Node *parent)
{
	Node *sibling;

	if (m_head == NULL)
		return;
	while (!parent->isnil && node->color == BLACK) {
		if (node == parent->left) {
			sibling = parent->right;
			if (sibling->color == RED) {
				sibling->color = BLACK;
				parent->color = RED;
				sibling = sibling->left;
				rotateLeft(parent);
			}
			if (sibling->left->color == BLACK && sibling->right->color == BLACK) {
				sibling->color = RED;
				node = parent;
				parent = parent->parent;
				continue;
			}
			if (sibling->right->color == BLACK) {
				sibling->left->color = BLACK;
				sibling->color = RED;
				rotateRight(sibling);
				sibling = sibling->parent;
			}
			sibling->color = parent->color;
			parent->color = BLACK;
			sibling->right->color = BLACK;
			rotateLeft(parent);
		} else {
			sibling = parent->left;
			if (sibling->color == RED) {
				sibling->color = BLACK;
				parent->color = RED;
				sibling = sibling->right;
				rotateRight(parent);
			}
			if (sibling->left->color == BLACK && sibling->right->color == BLACK) {
				sibling->color = RED;
				node = parent;
				parent = parent->parent;
				continue;
			}
			if (sibling->left->color == BLACK) {
				sibling->right->color = BLACK;
				sibling->color = RED;
				rotateLeft(sibling);
				sibling = sibling->parent;
			}
			sibling->color = parent->color;
			parent->color = BLACK;
			sibling->left->color = BLACK;
			rotateRight(parent);
		}
		node = m_head->parent;
		parent = node->parent;
	}
	node->color = BLACK;
}

JSON_NOINLINE
void PairTree::eraseNode(Node *node)
{
	Node *head = m_head;
	Node *splice, *child, *childParent, *link;
	bool fixup = false;

	if (head == NULL)
		return;

	/* splice: the node taken out of the tree (node or its successor) */
	if (node->left->isnil || node->right->isnil) {
		splice = node;
		childParent = node->parent;
	} else if (node->right->left->isnil) {
		splice = node->right;
		childParent = splice;
	} else {
		splice = nextNode(node);
		childParent = splice->parent;
	}
	link = splice->parent;
	child = !splice->left->isnil ? splice->left : splice->right;

	if (!child->isnil) {
		child->parent = link;
		link = splice->parent;
	}
	if (link->isnil)
		head->parent = child;
	else if (link->left == splice)
		link->left = child;
	else
		link->right = child;

	if (splice->color == BLACK)
		fixup = true;
	if (splice != node) {
		/* the successor takes the place of node */
		splice->parent = node->parent;
		if (node->parent->isnil)
			head->parent = splice;
		else if (node->parent->left == node)
			node->parent->left = splice;
		else
			node->parent->right = splice;
		splice->left = node->left;
		if (!node->left->isnil)
			node->left->parent = splice;
		splice->right = node->right;
		if (!node->right->isnil)
			node->right->parent = splice;
		splice->color = node->color;
	}
	if (fixup) {
		eraseFixup(child, childParent);
		head = m_head;
	}

	if (head->left == node)
		head->left = child->isnil ? childParent : min(child);
	if (head->right == node)
		head->right = child->isnil ? childParent : max(child);

	node->entry.~Entry();
	Free(node);
	m_size--;
}

JSON_NOINLINE
void PairTree::erase(const SimpleString &key)
{
	Node *node = lookup(m_head, key);

	if (node == NULL || node->isnil)
		return;
	eraseNode(node);
}

/* The pair stored under key, inserted empty when missing. */
JSON_NOINLINE
Object::Pair &PairTree::operator[](const SimpleString &key)
{
	Object::Pair pair;
	Node *node;

	node = insert(Entry(key, pair), NULL);
	return node->entry.pair;
}

} /* namespace Json */
} /* namespace sce */
