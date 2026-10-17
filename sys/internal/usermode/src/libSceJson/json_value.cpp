/*
 * libSceJson: MemAllocator, Initializer, Value, Parser and the library heap.
 *
 * Sony built these as one translation unit: the parser inlines the Value
 * constructor and destructor.
 *
 * The parser reads one character at a time from a DataProvideFunction and
 * builds the Value tree with recursive descent.  Allocation failures while
 * a parse (or Array::push_back / Object::operator[]) is running are
 * remembered in g_allocFailed and turn the result into
 * SCE_JSON_ERROR_NOMEM.
 */

#include <psp2/io/fcntl.h>
#include <kernel/error.h>
#include "json_internal.h"

namespace sce {
namespace Json {

const String *Value::s_nullstring;
const Array *Value::s_nullarray;
const Object *Value::s_nullobject;
const int64_t Value::s_nullinteger = 0;
const uint64_t Value::s_nulluinteger = 0;
const double Value::s_nullreal = 0;
const bool Value::s_nullbool = false;

InitParameter *g_initParam;
Mutex *g_mutex;
int g_allocTracking;
bool g_allocFailed;
Value *g_nullValue;

/* ---------------------------------------------------------------------- */

MemAllocator::MemAllocator()
{
}

MemAllocator::~MemAllocator()
{
}

void MemAllocator::notifyError(int error, size_t size, void *userData)
{
	if (error == SCE_JSON_ERROR_NOMEM)
		sceClibPrintf("[libSceJson] Allocation Fail. size = %ld\n", size);
	else
		sceClibPrintf("[libSceJson] unknown notification[%#x]\n", error);
}

/* ---------------------------------------------------------------------- */

Initializer::Initializer() : initstatus(false)
{
}

Initializer::~Initializer()
{
	terminate();
}

int Initializer::initialize(const InitParameter *initparam)
{
	InitParameter *param;

	if (g_initParam != NULL || initstatus)
		return SCE_JSON_ERROR_MULTIPLEINIT;
	if (initparam->allocator == NULL)
		return SCE_JSON_ERROR_NOMEM;
	param = (InitParameter *)initparam->allocator->allocate(sizeof(InitParameter),
								 initparam->userData);
	if (param == NULL)
		return SCE_JSON_ERROR_NOMEM;
	g_initParam = param;
	param->allocator = initparam->allocator;
	param->userData = initparam->userData;
	param->filebuffersize = initparam->filebuffersize;
	g_allocTracking = 0;
	g_allocFailed = false;

	Value::s_nullstring = new String();
	Value::s_nullarray = new Array();
	Value::s_nullobject = new Object();
	g_nullValue = new Value();
	g_mutex = new Mutex("SceJson_internal");
	if (Value::s_nullstring == NULL || Value::s_nullarray == NULL ||
	    Value::s_nullobject == NULL || g_nullValue == NULL || g_mutex == NULL) {
		initstatus = true;
		terminate();
		return SCE_JSON_ERROR_NOMEM;
	}
	initstatus = true;
	return 0;
}

int Initializer::terminate()
{
	if (g_initParam == NULL || !initstatus)
		return SCE_JSON_ERROR_NOINIT;
	delete g_nullValue;
	g_nullValue = NULL;
	delete g_mutex;
	g_mutex = NULL;
	delete Value::s_nullstring;
	Value::s_nullstring = NULL;
	delete Value::s_nullarray;
	Value::s_nullarray = NULL;
	delete Value::s_nullobject;
	Value::s_nullobject = NULL;
	delete g_initParam;
	g_initParam = NULL;
	initstatus = false;
	g_allocTracking = 0;
	g_allocFailed = false;
	return 0;
}

int Initializer::setAllocatorInfoCallBack(AllocatorInfoFunction func, void *context)
{
	if (g_initParam == NULL)
		return SCE_JSON_ERROR_NOINIT;
	return 0;
}

/* ---------------------------------------------------------------------- */

void Value::set(ValueType type)
{
	if (m_type == type)
		return;
	switch (m_type) {
	case kValueTypeString:
		if (m_string != NULL) {
			delete m_string;
			m_string = NULL;
		}
		break;
	case kValueTypeArray:
		if (m_array != NULL) {
			delete m_array;
			m_array = NULL;
		}
		break;
	case kValueTypeObject:
		if (m_object != NULL) {
			delete m_object;
			m_object = NULL;
		}
		break;
	default:
		break;
	}

	switch (type) {
	case kValueTypeBoolean:
		m_boolean = false;
		break;
	case kValueTypeInteger:
		m_integer = 0;
		break;
	case kValueTypeUInteger:
		m_uinteger = 0;
		break;
	case kValueTypeReal:
		m_uinteger = 0;
		break;
	case kValueTypeString:
		m_string = new String();
		if (m_string != NULL && ImplAccessor::impl(*m_string) == NULL) {
			delete m_string;
			m_string = NULL;
		}
		if (m_string == NULL)
			type = kValueTypeNull;
		break;
	case kValueTypeArray:
		m_array = new Array();
		if (m_array != NULL && ImplAccessor::impl(*m_array) == NULL) {
			delete m_array;
			m_array = NULL;
		}
		if (m_array == NULL)
			type = kValueTypeNull;
		break;
	case kValueTypeObject:
		m_object = new Object();
		if (m_object != NULL && ImplAccessor::impl(*m_object) == NULL) {
			delete m_object;
			m_object = NULL;
		}
		if (m_object == NULL)
			type = kValueTypeNull;
		break;
	default:
		break;
	}
	m_type = type;
}

Value::~Value()
{
	set(kValueTypeNull);
	if (m_rootparam != NULL) {
		delete m_rootparam;
		m_rootparam = NULL;
	}
}

Value::Value() : m_parent(NULL), m_rootparam(NULL), m_type(kValueTypeNull)
{
	set(kValueTypeNull);
}

Value::Value(ValueType type) : m_parent(NULL), m_rootparam(NULL), m_type(kValueTypeNull)
{
	set(type);
}

void Value::set(bool b)
{
	set(kValueTypeBoolean);
	m_boolean = b;
}

Value::Value(bool b) : m_parent(NULL), m_rootparam(NULL), m_type(kValueTypeNull)
{
	set(b);
}

void Value::set(int64_t l)
{
	set(kValueTypeInteger);
	m_integer = l;
}

Value::Value(int64_t l) : m_parent(NULL), m_rootparam(NULL), m_type(kValueTypeNull)
{
	set(l);
}

void Value::set(uint64_t ul)
{
	set(kValueTypeUInteger);
	m_uinteger = ul;
}

Value::Value(uint64_t ul) : m_parent(NULL), m_rootparam(NULL), m_type(kValueTypeNull)
{
	set(ul);
}

void Value::set(double n)
{
	set(kValueTypeReal);
	m_real = n;
}

Value::Value(double n) : m_parent(NULL), m_rootparam(NULL), m_type(kValueTypeNull)
{
	set(n);
}

void Value::set(const String &s)
{
	if (ImplAccessor::impl(s) == NULL)
		return;
	set(kValueTypeString);
	if (m_string != NULL)
		*m_string = s;
}

Value::Value(const String &s) : m_parent(NULL), m_rootparam(NULL), m_type(kValueTypeNull)
{
	set(s);
}

void Value::set(const Array &a)
{
	if (ImplAccessor::impl(a) == NULL)
		return;
	set(kValueTypeArray);
	if (m_array != NULL)
		*m_array = a;
}

Value::Value(const Array &a) : m_parent(NULL), m_rootparam(NULL), m_type(kValueTypeNull)
{
	set(a);
}

void Value::set(const Object &o)
{
	if (ImplAccessor::impl(o) == NULL)
		return;
	set(kValueTypeObject);
	if (m_object != NULL)
		*m_object = o;
}

Value::Value(const Object &o) : m_parent(NULL), m_rootparam(NULL), m_type(kValueTypeNull)
{
	set(o);
}

void Value::set(const Value &x)
{
	switch (x.m_type) {
	case kValueTypeNull:
		set(kValueTypeNull);
		break;
	case kValueTypeBoolean:
		set(x.m_boolean);
		break;
	case kValueTypeInteger:
		set(x.m_integer);
		break;
	case kValueTypeUInteger:
		set(x.m_uinteger);
		break;
	case kValueTypeReal:
		set(x.m_real);
		break;
	case kValueTypeString:
		set(*x.m_string);
		break;
	case kValueTypeArray:
		set(*x.m_array);
		break;
	case kValueTypeObject:
		set(*x.m_object);
		break;
	}
}

Value::Value(const Value &x) : m_parent(NULL), m_rootparam(NULL), m_type(kValueTypeNull)
{
	set(x);
}

Value &Value::operator=(const Value &x)
{
	set(x);
	return *this;
}

Value::operator bool() const
{
	switch (m_type) {
	case kValueTypeNull:
		return false;
	case kValueTypeBoolean:
		return m_boolean;
	case kValueTypeInteger:
		return m_integer != 0;
	case kValueTypeUInteger:
		return m_uinteger != 0;
	case kValueTypeReal:
		return m_real != 0.0;
	case kValueTypeString:
		return !m_string->empty();
	default:
		return true;
	}
}

void Value::toString(String &dst) const
{
	switch (m_type) {
	case kValueTypeNull:
		dst.append("null");
		break;
	case kValueTypeBoolean:
		dst.append(m_boolean ? "true" : "false");
		break;
	case kValueTypeInteger: {
		char buf[256];

		sceClibSnprintf(buf, sizeof(buf), "%lld", m_integer);
		dst.append(buf);
		break;
	}
	case kValueTypeUInteger: {
		char buf[256];

		sceClibSnprintf(buf, sizeof(buf), "%llu", m_uinteger);
		dst.append(buf);
		break;
	}
	case kValueTypeReal: {
		char buf[256];
		char *point;
		const char *p;

		/* drop a fraction of only zeros */
		sceClibSnprintf(buf, sizeof(buf), "%f", m_real);
		point = (char *)jsonStrchr(buf, '.');
		if (point != NULL) {
			for (p = point + 1; *p != '\0'; p++) {
				if (*p != '0')
					point = NULL;
			}
			if (point != NULL)
				*point = '\0';
		}
		dst.append(buf);
		break;
	}
	case kValueTypeString:
		dst.append(*m_string);
		break;
	case kValueTypeArray:
		dst.append("array");
		break;
	case kValueTypeObject:
		dst.append("object");
		break;
	}
}

/*
 * Makes parent the parent of this value.  A value without parent is the
 * root of a tree and owns the tree's RootParam (null access callback and
 * the null value handed out for missing elements).
 */
void Value::setParent(const Value *parent)
{
	m_parent = (Value *)parent;
	if (parent == NULL) {
		if (m_rootparam == NULL) {
			m_rootparam = new RootParam();
			if (m_rootparam == NULL)
				return;
			if (m_rootparam->nullValue != NULL)
				m_rootparam->nullValue->setParent(this);
		}
		if (m_rootparam != NULL) {
			m_rootparam->context = NULL;
			m_rootparam->nullAccess = NULL;
		}
	} else if (m_rootparam != NULL) {
		delete m_rootparam;
		m_rootparam = NULL;
	}
}

/* Appends s as a quoted JSON string; name unknown. */
static void jsonQuote(String &buf, const String &s)
{
	size_t length, i;
	int c;

	buf += '"';
	length = s.length();
	for (i = 0; i < length; i++) {
		c = s.at(i);
		switch (c) {
		case '"':
			buf.append("\\\"");
			break;
		case '\\':
			buf.append("\\\\");
			break;
		case '/':
			buf.append("\\/");
			break;
		case '\b':
			buf.append("\\b");
			break;
		case '\f':
			buf.append("\\f");
			break;
		case '\n':
			buf.append("\\n");
			break;
		case '\r':
			buf.append("\\r");
			break;
		case '\t':
			buf.append("\\t");
			break;
		default:
			if (c < 0x20 || c == 0x7f) {
				char escape[7];

				sceClibSnprintf(escape, sizeof(escape), "\\u%04x", c);
				buf.append(escape);
			} else {
				buf += (unsigned char)c;
			}
			break;
		}
	}
	buf += '"';
}

int Value::serialize_internal(String &buf, DataReceiveFunction func, void *userdata,
				  Value *parent)
{
	int ret = 0;

	setParent(parent);
	switch (m_type) {
	case kValueTypeNull:
	case kValueTypeBoolean:
	case kValueTypeInteger:
	case kValueTypeUInteger:
	case kValueTypeReal:
		toString(buf);
		ret = func(buf, userdata);
		break;
	case kValueTypeString:
		jsonQuote(buf, *m_string);
		ret = func(buf, userdata);
		break;
	case kValueTypeArray:
		buf += '[';
		ret = func(buf, userdata);
		if (ret < 0)
			break;
		for (Array::iterator it = m_array->begin(); it != m_array->end(); it++) {
			if (it != m_array->begin())
				buf += ',';
			ret = it->serialize_internal(buf, func, userdata, this);
			if (ret < 0)
				break;
		}
		if (ret >= 0) {
			buf += ']';
			ret = func(buf, userdata);
		}
		break;
	case kValueTypeObject:
		buf += '{';
		ret = func(buf, userdata);
		if (ret < 0)
			break;
		for (Object::iterator it = m_object->begin(); it != m_object->end(); it++) {
			if (it != m_object->begin())
				buf += ',';
			jsonQuote(buf, it->first);
			buf += ':';
			ret = it->second.serialize_internal(buf, func, userdata, this);
			if (ret < 0)
				break;
		}
		if (ret >= 0) {
			buf += '}';
			ret = func(buf, userdata);
		}
		break;
	}
	return ret;
}

int Value::serialize(String &buf, DataReceiveFunction func, void *userdata)
{
	if (g_initParam == NULL)
		return SCE_JSON_ERROR_NOINIT;
	return serialize_internal(buf, func, userdata);
}

int Value::serialize(String &dst)
{
	if (g_initParam == NULL)
		return SCE_JSON_ERROR_NOINIT;
	return serialize_internal(dst, jsonNoReceive, NULL);
}

void Value::swap(Value &rhs)
{
	Value tmp(*this);

	*this = rhs;
	rhs = tmp;
}

void Value::clear()
{
	set(kValueTypeNull);
}

ValueType Value::getType() const
{
	return m_type;
}

String *Value::referString()
{
	return m_type == kValueTypeString ? m_string : NULL;
}

Array *Value::referArray()
{
	return m_type == kValueTypeArray ? m_array : NULL;
}

Object *Value::referObject()
{
	return m_type == kValueTypeObject ? m_object : NULL;
}

int64_t *Value::referInteger()
{
	return m_type == kValueTypeInteger || m_type == kValueTypeUInteger ? &m_integer : NULL;
}

uint64_t *Value::referUInteger()
{
	return m_type == kValueTypeUInteger || m_type == kValueTypeInteger ? &m_uinteger : NULL;
}

double *Value::referReal()
{
	return m_type == kValueTypeReal ? &m_real : NULL;
}

bool *Value::referBoolean()
{
	return m_type == kValueTypeBoolean ? &m_boolean : NULL;
}

Value *Value::referValue(size_t index)
{
	if (m_type == kValueTypeArray) {
		if (index < m_array->size()) {
			Array::iterator it = m_array->begin();

			it.advance(index);
			return it.operator->();
		}
	} else if (m_type == kValueTypeObject && index < m_object->size()) {
		Object::iterator it = m_object->begin();

		it.advance(index);
		return &it->second;
	}
	return NULL;
}

Value *Value::referValue(const String &key)
{
	if (m_type == kValueTypeObject) {
		Object::iterator it = m_object->find(key);

		if (it != m_object->end())
			return &it->second;
	}
	return NULL;
}

const Value *Value::getRoot() const
{
	const Value *root = this;

	while (root->m_parent != NULL)
		root = root->m_parent;
	return root;
}

/* The root's null access callback, if one is set. */
static inline const RootParam *jsonRootParam(const Value *value)
{
	const Value *root = value;

	while (ImplAccessor::parent(*root) != NULL)
		root = ImplAccessor::parent(*root);
	return ImplAccessor::rootParam(*root);
}

/* The value handed out for a missing element. */
static inline const Value &jsonNullValue(const Value *value)
{
	const Value *root = value;
	RootParam *param;

	while (ImplAccessor::parent(*root) != NULL)
		root = ImplAccessor::parent(*root);
	if (root != NULL) {
		param = ImplAccessor::rootParam(*root);
		if (param != NULL && param->nullValue != NULL)
			return *param->nullValue;
	}
	return *g_nullValue;
}

const String &Value::getString() const
{
	if (m_type == kValueTypeNull) {
		const RootParam *param = jsonRootParam(this);

		if (param != NULL && param->nullAccess != NULL)
			return param->nullAccess(kValueTypeString, m_parent, param->context).getString();
	} else if (m_type == kValueTypeString) {
		return *m_string;
	}
	return *s_nullstring;
}

const Array &Value::getArray() const
{
	if (m_type == kValueTypeNull) {
		const RootParam *param = jsonRootParam(this);

		if (param != NULL && param->nullAccess != NULL)
			return param->nullAccess(kValueTypeArray, m_parent, param->context).getArray();
	} else if (m_type == kValueTypeArray) {
		return *m_array;
	}
	return *s_nullarray;
}

const Object &Value::getObject() const
{
	if (m_type == kValueTypeNull) {
		const RootParam *param = jsonRootParam(this);

		if (param != NULL && param->nullAccess != NULL)
			return param->nullAccess(kValueTypeObject, m_parent, param->context).getObject();
	} else if (m_type == kValueTypeObject) {
		return *m_object;
	}
	return *s_nullobject;
}

const int64_t &Value::getInteger() const
{
	if (m_type == kValueTypeNull) {
		const RootParam *param = jsonRootParam(this);

		if (param != NULL && param->nullAccess != NULL)
			return param->nullAccess(kValueTypeInteger, m_parent, param->context).getInteger();
	} else if ((m_type == kValueTypeInteger || m_type == kValueTypeUInteger)) {
		return m_integer;
	}
	return s_nullinteger;
}

const uint64_t &Value::getUInteger() const
{
	if (m_type == kValueTypeNull) {
		const RootParam *param = jsonRootParam(this);

		if (param != NULL && param->nullAccess != NULL)
			return param->nullAccess(kValueTypeUInteger, m_parent, param->context).getUInteger();
	} else if ((m_type == kValueTypeUInteger || m_type == kValueTypeInteger)) {
		return m_uinteger;
	}
	return s_nulluinteger;
}

const double &Value::getReal() const
{
	if (m_type == kValueTypeNull) {
		const RootParam *param = jsonRootParam(this);

		if (param != NULL && param->nullAccess != NULL)
			return param->nullAccess(kValueTypeReal, m_parent, param->context).getReal();
	} else if (m_type == kValueTypeReal) {
		return m_real;
	}
	return s_nullreal;
}

const bool &Value::getBoolean() const
{
	if (m_type == kValueTypeNull) {
		const RootParam *param = jsonRootParam(this);

		if (param != NULL && param->nullAccess != NULL)
			return param->nullAccess(kValueTypeBoolean, m_parent, param->context).getBoolean();
	} else if (m_type == kValueTypeBoolean) {
		return m_boolean;
	}
	return s_nullbool;
}

const Value &Value::getValue(size_t index) const
{
	if (m_type == kValueTypeArray) {
		if (index < m_array->size()) {
			Array::iterator it = m_array->begin();

			it.advance(index);
			return *it.operator->();
		}
	} else if (m_type == kValueTypeObject && index < m_object->size()) {
		Object::iterator it = m_object->begin();

		it.advance(index);
		return it->second;
	}
	return jsonNullValue(this);
}

const Value &Value::getValue(const String &key) const
{
	if (ImplAccessor::impl(key) == NULL)
		return *g_nullValue;
	if (m_type == kValueTypeObject) {
		Object::iterator it = m_object->find(key);

		if (it != m_object->end())
			return it->second;
	}
	return jsonNullValue(this);
}

const Value &Value::operator[](size_t index) const
{
	return getValue(index);
}

const Value &Value::operator[](const String &key) const
{
	return getValue(key);
}

const Value &Value::operator[](const char *key) const
{
	return getValue(String(key));
}

int Value::count() const
{
	switch (m_type) {
	case kValueTypeArray:
		return m_array->size();
	case kValueTypeObject:
		return m_object->size();
	default:
		return 0;
	}
}

int Value::setNullAccessCallBack(NullAccessFunction func, void *context)
{
	if (m_parent != NULL)
		return SCE_JSON_ERROR_NOROOT;
	if (m_rootparam == NULL)
		return SCE_JSON_ERROR_NOROOT;
	m_rootparam->nullAccess = func;
	m_rootparam->context = context;
	return 0;
}

/* ---------------------------------------------------------------------- */
/* Parser                                                                 */

/* Character source of the parser with one character of push back. */
class InputStream {
public:
	InputStream(Parser::DataProvideFunction func, void *userdata)
		: m_func(func), m_userdata(userdata), m_char(-1), m_result(0), m_unget(false) {}

	int get()
	{
		char data;

		if (m_unget) {
			m_unget = false;
			return m_char;
		}
		data = 0;
		m_result = m_func(data, m_userdata);
		m_char = data & 0xff;
		return m_char;
	}

	void unget() { m_unget = true; }
	int result() const { return m_result; }
	bool failed() const { return m_result < 0; }

	/* Skips white space; the next get() returns the first other character. */
	bool skipSpace()
	{
		int c;

		do {
			c = get();
		} while (c == ' ' || c == '\t' || c == '\n' || c == '\r');
		unget();
		if (failed())
			return false;
		return true;
	}

	/* Consumes c (after white space) if it comes next. */
	bool expect(int c)
	{
		int d;

		if (!skipSpace())
			return false;
		d = get();
		if (failed())
			return false;
		if (d == c)
			return true;
		unget();
		return false;
	}

	/* Consumes the rest of a literal. */
	bool expect(const char *s)
	{
		int c;

		for (; *s != '\0'; s++) {
			c = get();
			if (failed())
				return false;
			if (c != *s) {
				unget();
				return false;
			}
		}
		return true;
	}

private:
	Parser::DataProvideFunction m_func;
	void *m_userdata;
	int m_char;
	int m_result;
	bool m_unget;
};

bool endAllocTracking(void)
{
	Mutex *mutex = g_mutex;
	bool failed;

	mutex->lock();
	failed = g_allocFailed;
	if (--g_allocTracking == 0)
		g_allocFailed = false;
	mutex->unlock();
	return failed;
}

bool Parser::parseNumber(Value &v, InputStream &is, Value *parent)
{
	bool real;
	int c;
	const char *s;
	uint64_t value, next;

	if (g_allocFailed)
		return false;

	String text;

	real = false;
	for (;;) {
		c = is.get();
		if (c >= '0' && c <= '9') {
			text += (unsigned char)c;
		} else if (c == '-') {
			if (!text.empty())
				real = true;
			text += (unsigned char)c;
		} else if (c == '+' || c == '.' || c == 'e' || c == 'E') {
			if (!text.empty())
				real = true;
			text += (unsigned char)c;
		} else {
			break;
		}
	}
	is.unget();

	if (!real) {
		s = text.c_str();
		if (*s == '-')
			s++;
		value = 0;
		for (; *s != '\0'; s++) {
			next = value * 10 + (*s - '0');
			if (next < value) {
				real = true;
				break;
			}
			value = next;
		}
	}
	if (!real) {
		if (*text.c_str() != '-') {
			v.set(kValueTypeUInteger);
			v.m_uinteger = value;
			v.setParent(parent);
			return true;
		}
		if (value < 0x8000000000000000ULL) {
			v.set(kValueTypeInteger);
			v.m_integer = -(int64_t)value;
			v.setParent(parent);
			return true;
		}
	}
	double number = jsonStrtod(text.c_str());

	v.set(kValueTypeReal);
	v.m_real = number;
	v.setParent(parent);
	return true;
}

/* \uXXXX: four hex digits, -1 when malformed. */
int Parser::parseQuadHex(InputStream &is)
{
	int value = 0;
	unsigned int i;
	int c, digit;

	for (i = 0; i < 4; i++) {
		c = is.get();
		if (is.failed())
			return -1;
		if (c == -1)
			return -1;
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'A' && c <= 'F') {
			digit = c - 'A' + 10;
		} else if (c >= 'a' && c <= 'f') {
			digit = c - 'a' + 10;
		} else {
			is.unget();
			return -1;
		}
		value = value * 16 + digit;
	}
	return value;
}

/* The code point of a \u escape (with its surrogate pair) as UTF-8. */
bool Parser::parseCodePoint(String &str, InputStream &is)
{
	int code, low;

	code = parseQuadHex(is);
	if (code == -1)
		return false;
	if (code >= 0xd800 && code <= 0xdfff) {
		if (code >= 0xdc00)	/* a low surrogate first */
			return false;
		if (is.get() != '\\' || is.get() != 'u') {
			is.unget();
			return false;
		}
		low = parseQuadHex(is);
		if (low < 0xdc00 || low > 0xdfff)
			return false;
		code = (((code & 0x3ff) << 10) | (low & 0x3ff)) + 0x10000;
	}

	if (code < 0x80) {
		str += (unsigned char)code;
	} else if (code < 0x800) {
		str += (unsigned char)(0xc0 | ((code >> 6) & 0x1f));
		str += (unsigned char)(0x80 | (code & 0x3f));
	} else if (code < 0x10000) {
		str += (unsigned char)(0xe0 | ((code >> 12) & 0xf));
		str += (unsigned char)(0x80 | ((code >> 6) & 0x3f));
		str += (unsigned char)(0x80 | (code & 0x3f));
	} else {
		str += (unsigned char)(0xf0 | ((code >> 18) & 0x7));
		str += (unsigned char)(0x80 | ((code >> 12) & 0x3f));
		str += (unsigned char)(0x80 | ((code >> 6) & 0x3f));
		str += (unsigned char)(0x80 | (code & 0x3f));
	}
	return true;
}

/* The characters of a string after its opening quote. */
bool Parser::parseString(String &s, InputStream &is)
{
	int c;

	if (g_allocFailed)
		return false;
	for (;;) {
		c = is.get();
		if (is.failed())
			return false;
		if (c < 0x20) {
			is.unget();
			return false;
		}
		if (c == '"')
			return true;
		if (c == '\\') {
			c = is.get();
			if (is.failed())
				return false;
			switch (c) {
			case '"':
				s += '"';
				break;
			case '\\':
				s += '\\';
				break;
			case '/':
				s += '/';
				break;
			case 'b':
				s += '\b';
				break;
			case 'f':
				s += '\f';
				break;
			case 'n':
				s += '\n';
				break;
			case 'r':
				s += '\r';
				break;
			case 't':
				s += '\t';
				break;
			case 'u':
				if (!parseCodePoint(s, is))
					return false;
				break;
			default:
				return false;
			}
		} else {
			s += (unsigned char)c;
		}
	}
}

bool Parser::parseObject(Value &v, InputStream &is, Value *parent)
{
	Object *object;

	if (g_allocFailed)
		return false;
	v.set(kValueTypeObject);
	v.setParent(parent);
	if (v.m_type != kValueTypeObject || (object = v.m_object) == NULL)
		return false;
	if (is.expect('}'))
		return true;

	String key;

	do {
		if (!is.expect('"') || !parseString(key, is) || !is.expect(':') ||
		    !parseValue((*object)[key], is, &v))
			return false;
		key.clear();
	} while (is.expect(','));
	return is.expect('}');
}

bool Parser::parseArray(Value &v, InputStream &is, Value *parent)
{
	Array *array;

	if (g_allocFailed)
		return false;
	v.set(kValueTypeArray);
	v.setParent(parent);
	if (v.m_type != kValueTypeArray || (array = v.m_array) == NULL)
		return false;
	if (is.expect(']'))
		return true;
	do {
		array->push_back(Value());
		if (g_allocFailed)
			return false;
		if (!parseValue(array->back(), is, &v))
			return false;
	} while (is.expect(','));
	return is.expect(']');
}

inline bool Parser::parseString(Value &v, InputStream &is, Value *parent)
{
	if (g_allocFailed)
		return false;
	v.set(kValueTypeString);
	v.setParent(parent);
	if (v.m_type != kValueTypeString || v.m_string == NULL)
		return false;
	return parseString(*v.m_string, is);
}

bool Parser::parseValue(Value &v, InputStream &is, Value *parent)
{
	int c;

	if (g_allocFailed)
		return false;
	if (!is.skipSpace())
		return false;
	c = is.get();
	if (is.failed())
		return false;
	switch (c) {
	case 'n':
		if (!is.expect("ull"))
			return false;
		v.set(kValueTypeNull);
		v.setParent(parent);
		return true;
	case 'f':
		if (!is.expect("alse"))
			return false;
		v.set(kValueTypeBoolean);
		v.m_boolean = false;
		v.setParent(parent);
		return true;
	case 't':
		if (!is.expect("rue"))
			return false;
		v.set(kValueTypeBoolean);
		v.m_boolean = true;
		v.setParent(parent);
		return true;
	case '"':
		return parseString(v, is, parent);
	case '[':
		return parseArray(v, is, parent);
	case '{':
		return parseObject(v, is, parent);
	default:
		if ((c >= '0' && c <= '9') || c == '-') {
			is.unget();
			return parseNumber(v, is, parent);
		}
		is.unget();
		return false;
	}
}

void beginAllocTracking(void)
{
	Mutex *mutex = g_mutex;

	mutex->lock();
	g_allocTracking++;
	mutex->unlock();
}

int Parser::parse(Value &dst, DataProvideFunction func, void *userdata)
{
	int ret;

	if (g_initParam == NULL)
		return SCE_JSON_ERROR_NOINIT;
	beginAllocTracking();

	InputStream is(func, userdata);

	ret = 0;
	if (!parseValue(dst, is, NULL)) {
		ret = is.result();
		if (ret >= 0)
			ret = SCE_JSON_ERROR_PARSE_INVALID_CHAR;
	}
	if (endAllocTracking())
		ret = SCE_JSON_ERROR_NOMEM;
	return ret;
}

int Parser::parse(Value &dst, const char *src, size_t siz_src)
{
	MemoryStream stream;

	if (g_initParam == NULL)
		return SCE_JSON_ERROR_NOINIT;
	stream.cur = src;
	stream.end = src + siz_src;
	return parse(dst, memoryStreamRead, &stream);
}

void Free(void *ptr)
{
	MemAllocator *allocator;

	if (g_initParam != NULL && (allocator = g_initParam->allocator) != NULL)
		allocator->deallocate(ptr, g_initParam->userData);
}

void *Malloc(size_t size)
{
	MemAllocator *allocator;
	void *ptr;

	if (g_initParam == NULL || (allocator = g_initParam->allocator) == NULL)
		return NULL;
	ptr = allocator->allocate(size, g_initParam->userData);
	if (ptr == NULL) {
		Mutex *mutex;

		g_initParam->allocator->notifyError(SCE_JSON_ERROR_NOMEM, size,
						    g_initParam->userData);
		mutex = g_mutex;
		mutex->lock();
		if (g_allocTracking != 0)
			g_allocFailed = true;
		mutex->unlock();
	}
	return ptr;
}

int Parser::parse(Value &dst, const char *path)
{
	size_t size;
	FileStream stream;
	int ret;

	if (g_initParam == NULL)
		return SCE_JSON_ERROR_NOINIT;
	size = g_initParam->filebuffersize;
	if (size == 0)
		return SCE_JSON_ERROR_NOBUF;

	stream.fd = -1;
	stream.bufferSize = 0;
	stream.data = NULL;
	stream.length = 0;
	stream.position = 0;
	stream.buffer = (char *)Malloc(size);
	if (stream.buffer != NULL) {
		stream.bufferSize = size;
		stream.data = stream.buffer;
		stream.fd = sceIoOpen(path, SCE_O_RDONLY, 0400);
	}
	ret = parse(dst, fileStreamRead, &stream);
	if (stream.fd >= 0) {
		sceIoClose(stream.fd);
		stream.fd = -1;
	}
	Free(stream.buffer);
	return ret;
}

/* ---------------------------------------------------------------------- */
/* Parser streams                                                         */

int memoryStreamRead(char &data, void *userdata)
{
	MemoryStream *stream = (MemoryStream *)userdata;

	if (stream->cur >= stream->end)
		return SCE_JSON_ERROR_PARSE_INVALID_CHAR;
	data = *stream->cur++;
	return 0;
}

int fileStreamRead(char &data, void *userdata)
{
	FileStream *stream = (FileStream *)userdata;
	int n;

	if (stream->fd < 0) {
		switch (stream->fd) {
		case (int)SCE_ERROR_ERRNO_ENOENT:
			return SCE_JSON_ERROR_NOFILE;
		case -1:
			return SCE_JSON_ERROR_NOINIT;	/* no buffer */
		default:
			return stream->fd;
		}
	}
	if (stream->position == stream->length) {
		n = sceIoRead(stream->fd, stream->buffer, stream->bufferSize);
		if (n == 0)
			return SCE_JSON_ERROR_PARSE_INVALID_CHAR;
		if (n < 0)
			return n;
		stream->length = n;
		stream->position = 0;
	}
	data = stream->data[stream->position];
	stream->position++;
	return 0;
}

} /* namespace Json */
} /* namespace sce */
