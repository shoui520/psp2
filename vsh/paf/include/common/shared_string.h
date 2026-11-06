/*
 * paf::common::SharedStringBody: a string that counts its own references.
 * SharedString holds one by an intrusive pointer next to its characters, and
 * SharedStringSet keeps each text once, ordered by its first character and
 * then by sce_paf_strcmp.
 *
 * No Sony name is known for any of them: their members are exported by NID
 * only (ScePafCommon), and those are written as C functions taking the object.
 * libpaf (PAF_LIBRARY_BUILD) inlines AddRef and Release; a module that links
 * libpaf calls them through the exports, as Sony's modules do (dbutil).
 */
#ifndef PAF_COMMON_SHARED_STRING_H
#define PAF_COMMON_SHARED_STRING_H

#include <stddef.h>
#include <stdint.h>
#include <sce_atomic.h>
#include <std/string>
#include <std/string.h>
#include <std/memory>
#include <std/set>

namespace paf {
namespace common {
class SharedStringBody;
}
}

extern "C" {
int32_t ScePafCommon_A704EEE6(paf::common::SharedStringBody *self);
int32_t ScePafCommon_3728D46C(paf::common::SharedStringBody *self);
}

namespace paf {
namespace common {

class SharedStringBody : public string {
public:
	SharedStringBody(const char *str) : m_ref_count(0)
	{
		assign(str);
	}

	SharedStringBody(const string &str) : m_ref_count(0)
	{
		assign(str);
	}

#ifdef PAF_LIBRARY_BUILD
	int32_t AddRef()
	{
		return sceAtomicIncrement32AcqRel(&m_ref_count) + 1;
	}

	int32_t Release()
	{
		int32_t count = sceAtomicDecrement32AcqRel(&m_ref_count) - 1;

		if (count == 0)
			delete this;
		return count;
	}
#else
	int32_t AddRef()
	{
		return ScePafCommon_A704EEE6(this);
	}

	int32_t Release()
	{
		return ScePafCommon_3728D46C(this);
	}
#endif

	int32_t m_ref_count;
};

class SharedString {
public:
	SharedString(const intrusive_ptr<SharedStringBody> &body)
		: m_str(body->ptr), m_body(body)
	{
	}

	bool operator<(const SharedString &right) const
	{
		if ((unsigned char)m_str[0] < (unsigned char)right.m_str[0])
			return true;
		return sce_paf_strcmp(m_str, right.m_str) < 0;
	}

	const char *m_str;
	intrusive_ptr<SharedStringBody> m_body;
};

class SharedStringSet : public set<SharedString> {
public:
	pair<iterator, bool> Insert(const intrusive_ptr<SharedStringBody> &body)
	{
		return insert(SharedString(body));
	}

	pair<iterator, bool> Insert(const char *str)
	{
		return Insert(intrusive_ptr<SharedStringBody>(new SharedStringBody(str)));
	}

	pair<iterator, bool> Insert(const string &str)
	{
		return Insert(intrusive_ptr<SharedStringBody>(new SharedStringBody(str)));
	}

	/* The string str by the set's order (first character, then sce_paf_strcmp), else end(). */
	iterator find(const char *const &str)
	{
		if (head == NULL)
			return iterator(NULL);

		_Node *node = head->parent;

		while (!node->isnil) {
			if (Less(str, node->value.m_str))
				node = node->left;
			else if (!Less(node->value.m_str, str))
				return iterator(node);
			else
				node = node->right;
		}
		return iterator(head);
	}

private:
	static bool Less(const char *left, const char *right)
	{
		if ((unsigned char)left[0] < (unsigned char)right[0])
			return true;
		return sce_paf_strcmp(left, right) < 0;
	}
};

}
}

extern "C" {
paf::common::SharedStringBody *ScePafCommon_50C5FA24(paf::common::SharedStringBody *self);
paf::common::SharedStringBody *ScePafCommon_CA19F75C(paf::common::SharedStringBody *self);
paf::common::SharedStringBody *ScePafCommon_01F82D1B(paf::common::SharedStringBody *self,
						     const paf::string &str);
paf::common::SharedStringBody *ScePafCommon_F98D9F4F(paf::common::SharedStringBody *self,
						     const paf::string &str);
paf::common::SharedStringBody *ScePafCommon_F1E2CBF8(paf::common::SharedStringBody *self,
						     const paf::string &str, size_t pos, size_t len);
paf::common::SharedStringBody *ScePafCommon_8F824A3E(paf::common::SharedStringBody *self,
						     const paf::string &str, size_t pos, size_t len);
paf::common::SharedStringBody *ScePafCommon_BFB672E6(paf::common::SharedStringBody *self,
						     const paf::common::SharedStringBody &src);
paf::common::SharedStringBody *ScePafCommon_CC229501(paf::common::SharedStringBody *self,
						     const paf::common::SharedStringBody &src);
paf::common::SharedStringBody *ScePafCommon_0342728E(paf::common::SharedStringBody *self,
						     const paf::common::SharedStringBody &src,
						     size_t pos, size_t len);
paf::common::SharedStringBody *ScePafCommon_435EE162(paf::common::SharedStringBody *self,
						     const paf::common::SharedStringBody &src,
						     size_t pos, size_t len);
paf::common::SharedStringBody *ScePafCommon_AED94915(paf::common::SharedStringBody *self,
						     const char *str);
paf::common::SharedStringBody *ScePafCommon_25E1C8C4(paf::common::SharedStringBody *self,
						     const char *str);
paf::common::SharedStringBody *ScePafCommon_5EEAEBAE(paf::common::SharedStringBody *self,
						     const char *str, size_t len);
paf::common::SharedStringBody *ScePafCommon_1F1C3F8D(paf::common::SharedStringBody *self,
						     const char *str, size_t len);
paf::common::SharedStringBody *ScePafCommon_6CD38929(paf::common::SharedStringBody *self,
						     size_t len, char ch);
paf::common::SharedStringBody *ScePafCommon_74D8D5D7(paf::common::SharedStringBody *self,
						     size_t len, char ch);
paf::pair<paf::common::SharedStringSet::iterator, bool>
ScePafCommon_BD163724(paf::common::SharedStringSet *self, const char *str);
paf::pair<paf::common::SharedStringSet::iterator, bool>
ScePafCommon_66DF2953(paf::common::SharedStringSet *self, const paf::string &str);
paf::pair<paf::common::SharedStringSet::iterator, bool>
ScePafCommon_886EC6BE(paf::common::SharedStringSet *self,
		      const paf::intrusive_ptr<paf::common::SharedStringBody> &body);
}

#endif
