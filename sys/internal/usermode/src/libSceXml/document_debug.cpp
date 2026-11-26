/*
 * document_debug.cpp: DocumentDebug and memory manager debug level stubs
 * (0x810009B4-0x810009CB).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{

namespace Dom
{

size_t DocumentDebug::getStructSize() const
{
	return 0;
}

size_t DocumentDebug::getCharTableSize() const
{
	return 0;
}

size_t DocumentDebug::getAttrTableSize() const
{
	return 0;
}

size_t DocumentDebug::getElementTableSize() const
{
	return 0;
}

} /* namespace Dom */

int setMemManagerDebugLevel(int level)
{
	(void)level;
	return 0;
}

int getMemManagerDebugLevel(void)
{
	return 0;
}

} /* namespace Xml */
} /* namespace sce */
