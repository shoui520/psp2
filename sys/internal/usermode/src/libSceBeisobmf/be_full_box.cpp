/*
 * ISO 14496-12 FullBox base class (IsoFullBox).
 */

#include "be_isobmf_internal.h"

void IsoFullBox::onFieldParsed()
{
	IsoBox::onFieldParsed();
	if (m_fullHeaderAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_fullHeaderAdded = 1;
		addField("version", BE_FIELD_UINT8, 1);
		addField("flags", BE_FIELD_UINT24, 3);
		m_fullHeaderSize = getBytesRead() + 4;
	}
}

IsoFullBox::IsoFullBox()
{
	m_fullHeaderAdded = 0;
	m_fullHeaderSize = 0;
}

uint64_t IsoFullBox::getFullHeaderSize() const
{
	return m_fullHeaderSize;
}
