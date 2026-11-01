/*
 * paf::common::SharedStringBody's exported members and SharedStringSet's
 * inserts. Each constructor is exported twice (complete and base object).
 */
#include <common/shared_string.h>

using paf::string;
using paf::intrusive_ptr;
using paf::common::SharedStringBody;
using paf::common::SharedStringSet;

/* The empty string and no references, before the text is set. */
static inline void Construct(SharedStringBody *self)
{
	self->ptr = &string::empty_string;
	self->count = 0;
	self->m_ref_count = 0;
}

int32_t ScePafCommon_A704EEE6(SharedStringBody *self)
{
	return self->AddRef();
}

int32_t ScePafCommon_3728D46C(SharedStringBody *self)
{
	return self->Release();
}

SharedStringBody *ScePafCommon_CA19F75C(SharedStringBody *self)
{
	Construct(self);
	return self;
}

SharedStringBody *ScePafCommon_50C5FA24(SharedStringBody *self)
{
	Construct(self);
	return self;
}

SharedStringBody *ScePafCommon_01F82D1B(SharedStringBody *self, const string &str)
{
	Construct(self);
	self->assign(str);
	return self;
}

SharedStringBody *ScePafCommon_F98D9F4F(SharedStringBody *self, const string &str)
{
	Construct(self);
	self->assign(str);
	return self;
}

SharedStringBody *ScePafCommon_F1E2CBF8(SharedStringBody *self, const string &str,
					size_t pos, size_t len)
{
	Construct(self);
	self->assign(str, pos, len);
	return self;
}

SharedStringBody *ScePafCommon_8F824A3E(SharedStringBody *self, const string &str,
					size_t pos, size_t len)
{
	Construct(self);
	self->assign(str, pos, len);
	return self;
}

SharedStringBody *ScePafCommon_BFB672E6(SharedStringBody *self, const SharedStringBody &src)
{
	Construct(self);
	self->assign(src);
	return self;
}

SharedStringBody *ScePafCommon_CC229501(SharedStringBody *self, const SharedStringBody &src)
{
	Construct(self);
	self->assign(src);
	return self;
}

SharedStringBody *ScePafCommon_0342728E(SharedStringBody *self, const SharedStringBody &src,
					size_t pos, size_t len)
{
	Construct(self);
	self->assign(src, pos, len);
	return self;
}

SharedStringBody *ScePafCommon_435EE162(SharedStringBody *self, const SharedStringBody &src,
					size_t pos, size_t len)
{
	Construct(self);
	self->assign(src, pos, len);
	return self;
}

SharedStringBody *ScePafCommon_AED94915(SharedStringBody *self, const char *str)
{
	Construct(self);
	self->assign(str);
	return self;
}

SharedStringBody *ScePafCommon_25E1C8C4(SharedStringBody *self, const char *str)
{
	Construct(self);
	self->assign(str);
	return self;
}

SharedStringBody *ScePafCommon_5EEAEBAE(SharedStringBody *self, const char *str, size_t len)
{
	Construct(self);
	self->assign(str, len);
	return self;
}

SharedStringBody *ScePafCommon_1F1C3F8D(SharedStringBody *self, const char *str, size_t len)
{
	Construct(self);
	self->assign(str, len);
	return self;
}

SharedStringBody *ScePafCommon_6CD38929(SharedStringBody *self, size_t len, char ch)
{
	Construct(self);
	self->append(len, ch);
	return self;
}

SharedStringBody *ScePafCommon_74D8D5D7(SharedStringBody *self, size_t len, char ch)
{
	Construct(self);
	self->append(len, ch);
	return self;
}

paf::pair<SharedStringSet::iterator, bool>
ScePafCommon_BD163724(SharedStringSet *self, const char *str)
{
	return self->Insert(str);
}

paf::pair<SharedStringSet::iterator, bool>
ScePafCommon_66DF2953(SharedStringSet *self, const string &str)
{
	return self->Insert(str);
}

paf::pair<SharedStringSet::iterator, bool>
ScePafCommon_886EC6BE(SharedStringSet *self, const intrusive_ptr<SharedStringBody> &body)
{
	return self->Insert(body);
}
