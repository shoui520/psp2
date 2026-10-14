/*
 * libSceJson internals: library state, the allocation tracking used by the
 * parser, and the private containers behind String, Array and Object.
 *
 * String, Array, Object and their iterators hold an opaque m_impl pointer.
 * The objects behind it are:
 *
 *   String          -> SimpleString   (heap copy of the characters)
 *   Array           -> ValueList      (circular doubly linked list of Value)
 *   Object          -> PairTree       (red-black tree keyed by the name)
 *   Array::iterator -> ListIterator   (one ValueList::Node pointer)
 *   Object::iterator-> TreeIterator   (one PairTree::Node pointer)
 *
 * Sony's names for these internals are not known (PLACEHOLDER names).
 */

#ifndef _JSON_INTERNAL_H
#define _JSON_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <psp2/kernel/threadmgr.h>
#include <sce/json.h>

extern "C" {
/* SceLibKernel / SceSysclib imports */
int sceClibPrintf(const char *fmt, ...);
int sceClibSnprintf(char *dst, unsigned int size, const char *fmt, ...);
unsigned int sceClibStrnlen(const char *s, unsigned int max);
void *sceClibMemcpy_safe(void *dst, const void *src, unsigned int size);
void sceClibAbort(void) __attribute__((noreturn));
}

#define JSON_NOINLINE	__attribute__((noinline))

/* Placement new.  Sony's library carries one out-of-line copy of it. */
void *operator new(size_t size, void *where);

namespace sce {
namespace Json {

/* ---------------------------------------------------------------------- */
/* string helpers (json_libc.cpp); names unknown                          */

double jsonStrtod(const char *s);
int jsonCtype(int c);
const char *jsonStrchr(const char *s, int c);
int jsonStrcmp(const char *a, const char *b);
size_t jsonStrlen(const char *s);
int jsonStrncmp(const char *a, const char *b, int n);
int jsonOutOfRange(void);

/* json_runtime.cpp */
int jsonDebugHook(void);
int jsonNoReceive(String &buf, void *userdata);
void *jsonMemcpy(void *dst, const void *src, size_t size);

#ifdef __SNC__
#define JSON_BREAKPOINT()	__breakpoint(0)
#else
#define JSON_BREAKPOINT()	__asm__ volatile("bkpt #0")
#endif

/* ctype classes of jsonCtype() */
#define JSON_CTYPE_UPPER	0x01
#define JSON_CTYPE_LOWER	0x02
#define JSON_CTYPE_DIGIT	0x04
#define JSON_CTYPE_SPACE	0x08
#define JSON_CTYPE_PUNCT	0x10
#define JSON_CTYPE_CNTRL	0x20
#define JSON_CTYPE_XDIGIT	0x40

/* ---------------------------------------------------------------------- */
/* library state                                                          */

/* LwMutex guarding the allocation tracking; name unknown */
class Mutex {
public:
	Mutex(const char *name)
	{
		sceKernelCreateLwMutex(&m_work, name, 0x2002, 0, NULL);
	}
	~Mutex() { sceKernelDeleteLwMutex(&m_work); }
	void lock() { sceKernelLockLwMutex(&m_work, 1, NULL); }
	void unlock() { sceKernelUnlockLwMutex(&m_work, 1); }

private:
	SceKernelLwMutexWork m_work;
};

extern InitParameter *g_initParam;	/* copy of the initialize() parameter */
extern Mutex *g_mutex;
extern int g_allocTracking;		/* nesting of allocation tracking */
extern bool g_allocFailed;		/* an allocation failed while tracking */
extern Value *g_nullValue;		/* returned for missing elements */

/* Allocation tracking: a failed Malloc() inside these brackets is remembered. */
void beginAllocTracking(void);
bool endAllocTracking(void);

/* Per-tree data of a root Value (one whose parent is NULL). */
struct RootParam {
	RootParam() : nullAccess(NULL), context(NULL)
	{
		nullValue = new Value();
	}
	~RootParam()
	{
		if (nullValue != NULL) {
			delete nullValue;
			nullValue = NULL;
		}
	}

	Value::NullAccessFunction nullAccess;
	void *context;
	Value *nullValue;
};

/* ---------------------------------------------------------------------- */
/* SimpleString: the characters of a String                               */

class SimpleString {
public:
	SimpleString() : m_data(s_empty), m_length(0) {}
	SimpleString(const char *s) : m_data(s_empty), m_length(0)
	{
		assign(s, s != NULL ? jsonStrlen(s) : 0);
	}
	SimpleString(const char *s, size_t n) : m_data(s_empty), m_length(0)
	{
		assign(s, n);
	}
	SimpleString(const SimpleString &s) : m_data(s_empty), m_length(0)
	{
		assign(s);
	}
	~SimpleString()
	{
		if (m_data != s_empty)
			Free(m_data);
		m_data = (char *)-1;
		m_length = (size_t)-1;
	}

	SimpleString &assign(const char *s, size_t n);
	SimpleString &assign(const SimpleString &s);
	size_t rfind(const char *s, size_t pos, size_t n) const;
	SimpleString &erase(size_t pos, size_t n);
	SimpleString substr(size_t pos, size_t n) const;

	void clear() { assign(NULL, 0); }

	void append(const char *s, size_t n)
	{
		char *data = (char *)Malloc(m_length + n + 1);

		if (data != NULL) {
			size_t length = m_length;

			jsonMemcpy(data, m_data, m_length);
			jsonMemcpy(data + m_length, s, n);
			data[m_length + n] = '\0';
			if (m_data != s_empty)
				Free(m_data);
			m_data = data;
			m_length = length + n;
		}
	}

	void append(size_t n, char c)
	{
		char *data = (char *)Malloc(m_length + n + 1);

		if (data != NULL) {
			size_t length = m_length;
			size_t i;

			jsonMemcpy(data, m_data, m_length);
			for (i = 0; i < n; i++)
				data[m_length + i] = c;
			data[m_length + n] = '\0';
			if (m_data != s_empty)
				Free(m_data);
			m_data = data;
			m_length = length + n;
		}
	}

	size_t find(const char *s, size_t pos, size_t n) const
	{
		size_t rest;
		const char *p, *hit;

		if (n == 0 && pos <= m_length)
			return pos;
		rest = m_length - pos;
		if (pos < m_length && n <= rest) {
			p = m_data + pos;
			while (rest != 0 && (hit = jsonStrchr(p, *s)) != NULL) {
				if (jsonStrncmp(hit, s, n) == 0)
					return hit - m_data;
				rest -= hit - p + 1;
				p = hit + 1;
			}
		}
		return String::npos;
	}

	void resize(size_t n)
	{
		if (n < m_length)
			erase(n, String::npos);
		else if (m_length < n)
			append(n - m_length, '\0');
	}

	bool equals(const char *s) const
	{
		if (*m_data != *s)
			return false;
		return jsonStrcmp(m_data, s) == 0;
	}

	static char s_empty[1];

	char *m_data;
	size_t m_length;
	char m_allocator;	/* empty allocator object */
};

static inline bool operator<(const SimpleString &a, const SimpleString &b)
{
	if ((unsigned char)a.m_data[0] < (unsigned char)b.m_data[0])
		return true;
	return jsonStrcmp(a.m_data, b.m_data) < 0;
}

/* ---------------------------------------------------------------------- */
/* ValueList: the elements of an Array                                    */

class ValueList {
public:
	struct Node {
		Node *prev;
		Node *next;
		Value value;
	};

	ValueList() : m_size(0)
	{
		m_head = (Node *)Malloc(sizeof(Node));
		if (m_head != NULL) {
			m_head->prev = m_head;
			m_head->next = m_head;
		}
	}
	ValueList(const ValueList &list);
	~ValueList()
	{
		Node *node, *next;

		if (m_head != NULL) {
			for (node = m_head->next; node != m_head; node = next) {
				next = node->next;
				node->value.~Value();
				Free(node);
			}
			Free(m_head);
		}
		m_head = (Node *)-1;
		m_size = (size_t)-1;
	}

	void clear()
	{
		Node *node, *next;

		if (m_head == NULL)
			return;
		for (node = m_head->next; node != m_head; node = next) {
			next = node->next;
			node->value.~Value();
			Free(node);
		}
		m_head->prev = m_head;
		m_head->next = m_head;
		m_size = 0;
	}

	/* A new node holding a copy of value; the head when out of memory. */
	Node *newNode(const Value &value)
	{
		Node *node = (Node *)Malloc(sizeof(Node));

		if (node == NULL)
			return m_head;
		new (&node->value) Value(value);
		return node;
	}

	void insert(Node *where, Node *node)
	{
		node->prev = where->prev;
		node->next = where;
		where->prev->next = node;
		where->prev = node;
		m_size++;
	}

	void push_back(const Value &value)
	{
		Node *node = newNode(value);

		if (node != m_head) {
			node->prev = m_head->prev;
			node->next = m_head;
			m_head->prev->next = node;
			m_head->prev = node;
			m_size++;
		}
	}

	void remove(Node *node)
	{
		node->next->prev = node->prev;
		node->prev->next = node->next;
		node->value.~Value();
		Free(node);
		m_size--;
	}

	void swap(ValueList &list)
	{
		Node *head = list.m_head;
		size_t size;

		list.m_head = m_head;
		m_head = head;
		size = list.m_size;
		list.m_size = m_size;
		m_size = size;
	}

	Node *m_head;
	size_t m_size;
	char m_allocator;
};

struct ListIterator {
	ListIterator() : node(NULL) {}
	ValueList::Node *node;
};

/* ---------------------------------------------------------------------- */
/* PairTree: the members of an Object                                     */

struct TreeIterator;

class PairTree {
public:
	enum { RED = 1, BLACK = 0 };

	struct Entry {
		Entry(const SimpleString &key, const Object::Pair &pair) : key(key), pair(pair) {}

		SimpleString key;
		Object::Pair pair;
	};

	struct Node {
		Node(Node *parent, Node *head, const Entry &entry)
			: parent(parent), left(head), right(head), entry(entry)
		{
			isnil = 0;
			color = RED;
		}
		Node(Node *parent, Node *head, const Node &node)
			: parent(parent), left(head), right(head), entry(node.entry)
		{
			color = node.color;
			isnil = 0;
		}

		Node *parent;
		Node *left;
		Node *right;
		Entry entry;
		char color;
		char isnil;
	};

	PairTree() : m_size(0)
	{
		m_head = (Node *)Malloc(sizeof(Node));
		if (m_head != NULL) {
			m_head->parent = m_head;
			m_head->left = m_head;
			m_head->right = m_head;
			m_head->color = BLACK;
			m_head->isnil = 1;
		}
	}
	PairTree(const PairTree &tree);
	~PairTree()
	{
		clear();
		if (m_head != NULL)
			Free(m_head);
		m_head = (Node *)-1;
		m_size = (size_t)-1;
	}

	void clear();
	Node *copyNodes(Node *parent, const Node *node);
	TreeIterator find(const SimpleString &key) const;
	void insertFixup(Node *node);
	Node *insert(const Entry &entry, bool *inserted);
	void eraseFixup(Node *node, Node *parent);
	void eraseNode(Node *node);
	void erase(const SimpleString &key);
	Object::Pair &operator[](const SimpleString &key);

	static Node *min(Node *node)
	{
		while (!node->left->isnil)
			node = node->left;
		return node;
	}
	static Node *max(Node *node)
	{
		while (!node->right->isnil)
			node = node->right;
		return node;
	}

	void rotateLeft(Node *node)
	{
		Node *pivot;

		if (m_head == NULL)
			return;
		pivot = node->right;
		node->right = pivot->left;
		if (!pivot->left->isnil)
			pivot->left->parent = node;
		pivot->parent = node->parent;
		if (node->parent->isnil)
			m_head->parent = pivot;
		else if (node == node->parent->left)
			node->parent->left = pivot;
		else
			node->parent->right = pivot;
		pivot->left = node;
		node->parent = pivot;
	}

	void rotateRight(Node *node)
	{
		Node *pivot;

		if (m_head == NULL)
			return;
		pivot = node->left;
		node->left = pivot->right;
		if (!pivot->right->isnil)
			pivot->right->parent = node;
		pivot->parent = node->parent;
		if (node->parent->isnil)
			m_head->parent = pivot;
		else if (node == node->parent->right)
			node->parent->right = pivot;
		else
			node->parent->left = pivot;
		pivot->right = node;
		node->parent = pivot;
	}

	void swap(PairTree &tree)
	{
		Node *head = tree.m_head;
		size_t size;

		tree.m_head = m_head;
		m_head = head;
		size = tree.m_size;
		tree.m_size = m_size;
		m_size = size;
	}

	Node *m_head;
	size_t m_size;
	char m_allocator;
};

struct TreeIterator {
	TreeIterator() : node(NULL) {}
	TreeIterator(PairTree::Node *n) : node(n) {}
	TreeIterator(const TreeIterator &it) : node(it.node) {}
	PairTree::Node *node;
};

/* ---------------------------------------------------------------------- */
/* access to the private members of the public classes                    */

class ImplAccessor {
public:
	static SimpleString *impl(const String &s) { return (SimpleString *)s.m_impl; }
	static ValueList *impl(const Array &a) { return (ValueList *)a.m_impl; }
	static PairTree *impl(const Object &o) { return (PairTree *)o.m_impl; }
	static ListIterator *impl(const Array::iterator &it) { return (ListIterator *)it.m_itimpl; }
	static TreeIterator *impl(const Object::iterator &it) { return (TreeIterator *)it.m_itimpl; }

	static void release(Array::iterator &it) { it.m_itimpl = NULL; }
	static void release(Object::iterator &it) { it.m_itimpl = NULL; }

	static Value *parent(const Value &v) { return v.m_parent; }
	static RootParam *rootParam(const Value &v) { return v.m_rootparam; }
	static ValueType type(const Value &v) { return v.m_type; }
	static String *string(const Value &v) { return v.m_string; }
	static Array *array(const Value &v) { return v.m_array; }
	static Object *object(const Value &v) { return v.m_object; }
	static void setParent(Value &v, const Value *parent) { v.setParent(parent); }
};

/* stream callbacks of the Parser (json_impl.cpp) */
struct MemoryStream {
	const char *cur;
	const char *end;
};

struct FileStream {
	int fd;
	char *buffer;
	size_t bufferSize;
	char *data;
	int length;
	int position;
};

int memoryStreamRead(char &data, void *userdata);
int fileStreamRead(char &data, void *userdata);

} /* namespace Json */
} /* namespace sce */

#endif /* _JSON_INTERNAL_H */
