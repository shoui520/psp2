/*
 * ScebXCe's AttrImpl: an attribute's name and value, each in a buffer of its own.
 */
#include "bxce_internal.h"

namespace bXCe {

AttrImpl::AttrImpl() : nameCapacity(0), valueCapacity(0), nameLength(0), valueLength(0),
		       name(NULL), value(NULL), next(NULL)
{
}

AttrImpl::~AttrImpl()
{
}

int AttrImpl::initialize()
{
	nameLength = 0;
	valueLength = 0;
	value = NULL;
	next = NULL;
	name = NULL;

	name = (char *)ScebXCe_B8DDE741(BXCE_ATTR_INITIAL_SIZE);
	if (name == NULL)
		return SCE_BXCE_RESULT_NO_MEMORY;
	value = (char *)ScebXCe_B8DDE741(BXCE_ATTR_INITIAL_SIZE);
	if (value == NULL)
		return SCE_BXCE_RESULT_NO_MEMORY;

	nameCapacity = BXCE_ATTR_INITIAL_SIZE;
	valueCapacity = BXCE_ATTR_INITIAL_SIZE;
	next = NULL;
	return bxceConvertResult(0);
}

int AttrImpl::terminate()
{
	if (name != NULL)
		ScebXCe_EC560647(name);
	if (value != NULL)
		ScebXCe_EC560647(value);
	name = NULL;
	value = NULL;
	return bxceConvertResult(0);
}

/* Sony's getters give the end of the name and value and their buffers' sizes */
const char *AttrImpl::getName() const
{
	return name + nameLength;
}

const char *AttrImpl::getValue() const
{
	return value + valueLength;
}

int AttrImpl::getNameLength() const
{
	return nameCapacity;
}

int AttrImpl::getValueLength() const
{
	return valueCapacity;
}

} /* namespace bXCe */
