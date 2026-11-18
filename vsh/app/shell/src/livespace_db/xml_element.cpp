/*
 * An element of the XML tree: its attributes set by name, and the empty text and element the
 * file keeps.
 */
#include "xml_tree.h"

void XmlElement::SetAttribute(const char *name, const char *value)
{
	attrs[paf::string(name)] = value;
}

static paf::string s_xmlEmptyText;
static XmlElement s_xmlEmptyElement;
